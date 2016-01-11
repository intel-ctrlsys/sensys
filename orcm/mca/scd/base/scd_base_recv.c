/*
 * Copyright (c) 2014-2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include "opal/dss/dss.h"
#include "opal/mca/mca.h"
#include "opal/util/output.h"
#include "opal/util/malloc.h"
#include "opal/mca/base/base.h"

#include "orte/types.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_wait.h"

#include "orcm/util/attr.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/pwrmgmt/base/base.h"
#include "orcm/mca/scd/base/base.h"

#include "orcm/mca/db/db.h"
#include "orcm/mca/db/base/base.h"

#include "orcm/util/utils.h"

static bool recv_issued=false;

static void orcm_scd_base_recv(int status, orte_process_name_t* sender,
                        opal_buffer_t* buffer, orte_rml_tag_t tag,
                        void* cbdata);

void open_callback(int dbhandle, int status, opal_list_t *in, opal_list_t *out, void *cbdata);
void close_callback(int dbhandle, int status, opal_list_t *in, opal_list_t *out, void *cbdata);
void fetch_callback(int dbhandle, int status, opal_list_t *in, opal_list_t *out, void *cbdata);
bool is_wanted_column(const char* name);
char* get_plugin_from_sensor_name(const char* sensor_name);
int get_inventory_list(opal_list_t *filters, opal_list_t **results);
void orcm_scd_base_fetch_recv(int status, orte_process_name_t* sender,
                              opal_buffer_t* buffer, orte_rml_tag_t tag,
                              void* cbdata);
int build_filter_list(opal_buffer_t* buffer, opal_list_t **filter_list);
int query_db_view(opal_list_t *filters, opal_list_t **results, const char *db_view);
int assemble_response(opal_list_t *results, opal_buffer_t **response_buffer);
char *query_header(const char *db_view);

int orcm_scd_base_comm_start(void)
{
    if (recv_issued) {
        return ORTE_SUCCESS;
    }

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:base:receive start comm",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SCD,
                            ORTE_RML_PERSISTENT,
                            orcm_scd_base_recv,
                            NULL);
    /* setup to to receive 'fetch' commands */
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ORCMD_FETCH, ORTE_RML_PERSISTENT,
                            orcm_scd_base_fetch_recv, NULL);

    recv_issued = true;

    return ORTE_SUCCESS;
}


int orcm_scd_base_comm_stop(void)
{
    if (!recv_issued) {
        return ORTE_SUCCESS;
    }

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:base:receive stop comm",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SCD);
    recv_issued = false;

    return ORTE_SUCCESS;
}


/* process incoming messages in order of receipt */
static void orcm_scd_base_recv(int status, orte_process_name_t* sender,
                               opal_buffer_t* buffer, orte_rml_tag_t tag,
                               void* cbdata)
{
    orcm_scd_cmd_flag_t command, sub_command;
    int rc, i, j, cnt, result;
    int32_t int_param;
    int32_t *int_param_ptr = &int_param;
    float float_param;
    float *float_param_ptr = &float_param;
    bool bool_param;
    bool *bool_param_ptr = &bool_param;
    orcm_alloc_t *alloc, **allocs;
    opal_buffer_t *ans, *rmbuf;
    orcm_session_t *session;
    orcm_queue_t *q;
    orcm_node_t **nodes;
    orcm_session_id_t sessionid;
    bool per_session, found;
    int success = OPAL_SUCCESS;

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:base:receive processing msg from %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(sender)));

    /* always pass some answer back to the caller so they
     * don't hang
     */
    ans = OBJ_NEW(opal_buffer_t);

    /* unpack the command */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &command,
                                              &cnt, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto answer;
    }

    if (ORCM_SESSION_REQ_COMMAND == command) {
        /* session request - this comes in the form of a
         * requested allocation to support the session
         */
        int32_t alloc_power_budget = 0;
        //get power defaults
        int32_t alloc_power_mode = orcm_scd_base_get_cluster_power_mode();
        int32_t alloc_power_window = orcm_scd_base_get_cluster_power_window();
        int32_t alloc_power_overage = orcm_scd_base_get_cluster_power_overage();
        int32_t alloc_power_underage = orcm_scd_base_get_cluster_power_underage();
        int32_t alloc_power_overage_time = orcm_scd_base_get_cluster_power_overage_time();
        int32_t alloc_power_underage_time = orcm_scd_base_get_cluster_power_underage_time();
        float alloc_power_frequency = orcm_scd_base_get_cluster_power_frequency();
        bool alloc_power_strict = orcm_scd_base_get_cluster_power_strict();

        int32_t node_power_budget = 0;
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &alloc,
                                                  &cnt, ORCM_ALLOC))) {
            ORTE_ERROR_LOG(rc);
            goto answer;
        }
        /* assign a session to it */
        session = OBJ_NEW(orcm_session_t);
        session->alloc = alloc;
        session->id = orcm_scd_base_get_next_session_id();
        alloc->id = session->id;

        if (-1 == orcm_scd_base_get_cluster_power_budget()) {
            node_power_budget = -1;
            alloc_power_budget = -1;
        } else {
            if ((orcm_scd_base.nodes.size - orcm_scd_base.nodes.number_free) == 0) {
                node_power_budget = -1;
            } else {
                node_power_budget = orcm_scd_base_get_cluster_power_budget() / (orcm_scd_base.nodes.size - orcm_scd_base.nodes.number_free);
            }
            if (-1 == node_power_budget) {
                alloc_power_budget = -1;
            } else {
                alloc_power_budget = node_power_budget * alloc->min_nodes;
            }
        }

        if(OPAL_SUCCESS != (rc = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_POWER_BUDGET_KEY,
                                                   ORTE_ATTR_GLOBAL, &alloc_power_budget, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        if(OPAL_SUCCESS != (rc = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_POWER_MODE_KEY,
                                                   ORTE_ATTR_GLOBAL, &alloc_power_mode, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        if(OPAL_SUCCESS != (rc = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_POWER_WINDOW_KEY,
                                                   ORTE_ATTR_GLOBAL, &alloc_power_window, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        if(OPAL_SUCCESS != (rc = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_CAP_OVERAGE_LIMIT_KEY,
                                                   ORTE_ATTR_GLOBAL, &alloc_power_overage, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        if(OPAL_SUCCESS != (rc = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_CAP_UNDERAGE_LIMIT_KEY,
                                                   ORTE_ATTR_GLOBAL, &alloc_power_underage, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        if(OPAL_SUCCESS != (rc = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_CAP_OVERAGE_TIME_LIMIT_KEY,
                                                   ORTE_ATTR_GLOBAL, &alloc_power_overage_time, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        if(OPAL_SUCCESS != (rc = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_CAP_UNDERAGE_TIME_LIMIT_KEY,
                                                   ORTE_ATTR_GLOBAL, &alloc_power_underage_time, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        if(OPAL_SUCCESS != (rc = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_MANUAL_FREQUENCY_KEY,
                                                   ORTE_ATTR_GLOBAL, &alloc_power_frequency, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        if(OPAL_SUCCESS != (rc = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_FREQ_STRICT_KEY,
                                                   ORTE_ATTR_GLOBAL, &alloc_power_strict, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        /* select the power management component */
        if (ORCM_SUCCESS != (rc = orcm_pwrmgmt.alloc_notify(alloc))) {
            /* We couldn't fufill the request, so fail the request */
            int err = -1;
            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &err, 1, ORCM_ALLOC_ID_T))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(ans);
                return;
            }
            goto answer;
        }

        /* send session id back to sender */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &session->id,
                                                1, ORCM_ALLOC_ID_T))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }
        if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender, ans,
                                                          ORCM_RML_TAG_SCD,
                                                          orte_rml_send_callback,
                                                          NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }

        /* pass it to the scheduler */
        ORCM_ACTIVATE_SCD_STATE(session, ORCM_SESSION_STATE_INIT);

        return;
    } else if (ORCM_SESSION_INFO_COMMAND == command) {
        /* pack the number of queues we have */
        cnt = opal_list_get_size(&orcm_scd_base.queues);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &cnt, 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }

        /* for each queue, */
        OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
            /* pack the name */
            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &q->name,
                                                    1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(ans);
                return;
            }

            /* pack the count of sessions on the queue */
            cnt = (int)opal_list_get_size(&q->sessions);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &cnt, 1, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(ans);
                return;
            }
            if (0 < cnt) {
                /* pack all the sessions on the queue */
                allocs = (orcm_alloc_t**)malloc(cnt * sizeof(orcm_alloc_t*));
                if (!allocs) {
                    ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
                    return;
                }
                i = 0;
                OPAL_LIST_FOREACH(session, &q->sessions, orcm_session_t) {
                    allocs[i] = session->alloc;
                    i++;
                }
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, allocs,
                                                        i, ORCM_ALLOC))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
                free(allocs);
            }
        }
        /* send back results */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(sender, ans,
                                          ORCM_RML_TAG_SCD,
                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }

        return;
    } else if (ORCM_SESSION_CANCEL_COMMAND == command) {
        /* session cancel - this comes in the form of a
         * session id to be cancelled
         */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &sessionid,
                                                  &cnt, ORCM_ALLOC_ID_T))) {
            ORTE_ERROR_LOG(rc);
            goto answer;
        }

        session = OBJ_NEW(orcm_session_t);
        session->id = sessionid;
        /* pass it to the scheduler */
        ORCM_ACTIVATE_SCD_STATE(session, ORCM_SESSION_STATE_CANCEL);

        /* send confirmation back to sender */
        result = 0;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(sender, ans,
                                          ORCM_RML_TAG_SCD,
                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }
        return;
    } else if (ORCM_NODE_INFO_COMMAND == command) {
        /* pack the number of nodes we have */
        cnt = orcm_scd_base.nodes.lowest_free;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &cnt, 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }

        if (0 < cnt) {
            /* pack all the nodes */
            nodes = (orcm_node_t**)malloc(cnt * sizeof(orcm_node_t*));
            if (!nodes) {
                ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
                return;
            }
            i = 0;
            for (j = 0; j < orcm_scd_base.nodes.lowest_free; j++) {
                if (NULL == (nodes[i] =
                             (orcm_node_t*)opal_pointer_array_get_item(&orcm_scd_base.nodes,
                                                                       j))) {
                    continue;
                }
                OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                                     "%s scd:base:receive PACKING NODE: %s (%s)",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     nodes[i]->name,
                                     ORTE_NAME_PRINT(&nodes[i]->daemon)));
                i++;
            }

            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, nodes, i, ORCM_NODE))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(ans);
                return;
            }
            free(nodes);
        }

        /* send back results */
        if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender, ans,
                                                          ORCM_RML_TAG_SCD,
                                                          orte_rml_send_callback,
                                                          NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }

        return;
    } else if (ORCM_SET_POWER_COMMAND == command) {
        cnt = 1;

        /* unpack the subcommand */
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &sub_command,
                                                  &cnt, ORCM_SCD_CMD_T))) {
            ORTE_ERROR_LOG(rc);
            goto answer;
        }

        /* unpack the bool (tells us if this is global or per_session) */
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &per_session,
                                                  &cnt, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            goto answer;
        }

        if (false == per_session) {
            //global
            switch(sub_command) {
            case ORCM_SET_POWER_BUDGET_COMMAND:
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                          &cnt, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }
                result = orcm_scd_base_set_cluster_power_budget(int_param);
            break;
            case ORCM_SET_POWER_MODE_COMMAND:
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                          &cnt, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }
                result = orcm_scd_base_set_cluster_power_mode(int_param);
            break;
            case ORCM_SET_POWER_WINDOW_COMMAND:
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                          &cnt, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }
                result = orcm_scd_base_set_cluster_power_window(int_param);
            break;
            case ORCM_SET_POWER_OVERAGE_COMMAND:
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                          &cnt, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }
                result = orcm_scd_base_set_cluster_power_overage(int_param);
            break;
            case ORCM_SET_POWER_UNDERAGE_COMMAND:
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                          &cnt, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }
                result = orcm_scd_base_set_cluster_power_underage(int_param);
            break;
            case ORCM_SET_POWER_OVERAGE_TIME_COMMAND:
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                          &cnt, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }
                result = orcm_scd_base_set_cluster_power_overage_time(int_param);
            break;
            case ORCM_SET_POWER_UNDERAGE_TIME_COMMAND:
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                          &cnt, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }
                result = orcm_scd_base_set_cluster_power_underage_time(int_param);
            break;
            case ORCM_SET_POWER_FREQUENCY_COMMAND:
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &float_param,
                                                          &cnt, OPAL_FLOAT))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }
                result = orcm_scd_base_set_cluster_power_frequency(float_param);
            break;
            case ORCM_SET_POWER_STRICT_COMMAND:
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &bool_param,
                                                          &cnt, OPAL_BOOL))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }
                result = orcm_scd_base_set_cluster_power_strict(bool_param);
            break;
            default:
                result = ORTE_ERR_BAD_PARAM;
            }
        }
        else{
            //per session
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &sessionid,
                                                      &cnt, ORCM_ALLOC_ID_T))) {
                ORTE_ERROR_LOG(rc);
                goto answer;
            }

            //let's find the session
            found = false;
            /* for each queue, */
            OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
                OPAL_LIST_FOREACH(session, &q->sessions, orcm_session_t) {
                    alloc = session->alloc;
                    if(alloc->id == sessionid) { //found the session
                        found = true;
                        switch(sub_command) {
                        case ORCM_SET_POWER_BUDGET_COMMAND:
                            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                                      &cnt, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                goto answer;
                            }
                            result = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_POWER_BUDGET_KEY,
                                                        ORTE_ATTR_GLOBAL, &int_param, OPAL_INT32);
                        break;
                        case ORCM_SET_POWER_MODE_COMMAND:
                            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                                      &cnt, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                goto answer;
                            }
                            result = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_POWER_MODE_KEY,
                                                        ORTE_ATTR_GLOBAL, &int_param, OPAL_INT32);
                            orcm_pwrmgmt.alloc_notify(alloc);
                        break;
                        case ORCM_SET_POWER_WINDOW_COMMAND:
                            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                                      &cnt, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                goto answer;
                            }
                            result = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_POWER_WINDOW_KEY,
                                                        ORTE_ATTR_GLOBAL, &int_param, OPAL_INT32);
                        break;
                        case ORCM_SET_POWER_OVERAGE_COMMAND:
                            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                                      &cnt, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                goto answer;
                            }
                            result = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_CAP_OVERAGE_LIMIT_KEY,
                                                        ORTE_ATTR_GLOBAL, &int_param, OPAL_INT32);
                        break;
                        case ORCM_SET_POWER_UNDERAGE_COMMAND:
                            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                                      &cnt, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                goto answer;
                            }
                            result = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_CAP_UNDERAGE_LIMIT_KEY,
                                                        ORTE_ATTR_GLOBAL, &int_param, OPAL_INT32);
                        break;
                        case ORCM_SET_POWER_OVERAGE_TIME_COMMAND:
                            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                                      &cnt, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                goto answer;
                            }
                            result = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_CAP_OVERAGE_TIME_LIMIT_KEY,
                                                        ORTE_ATTR_GLOBAL, &int_param, OPAL_INT32);
                        break;
                        case ORCM_SET_POWER_UNDERAGE_TIME_COMMAND:
                            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &int_param,
                                                                      &cnt, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                goto answer;
                            }
                            result = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_CAP_UNDERAGE_TIME_LIMIT_KEY,
                                                        ORTE_ATTR_GLOBAL, &int_param, OPAL_INT32);
                        break;
                        case ORCM_SET_POWER_FREQUENCY_COMMAND:
                            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &float_param,
                                                                      &cnt, OPAL_FLOAT))) {
                                ORTE_ERROR_LOG(rc);
                                goto answer;
                            }
                            result = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_MANUAL_FREQUENCY_KEY,
                                                        ORTE_ATTR_GLOBAL, &float_param, OPAL_FLOAT);
                        break;
                        case ORCM_SET_POWER_STRICT_COMMAND:
                            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &bool_param,
                                                                      &cnt, OPAL_BOOL))) {
                                ORTE_ERROR_LOG(rc);
                                goto answer;
                            }
                            result = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_FREQ_STRICT_KEY,
                                                        ORTE_ATTR_GLOBAL, &bool_param, OPAL_BOOL);
                        break;
                        default:
                            result = ORTE_ERR_BAD_PARAM;
                        }
                        if(!strncmp(q->name, "running", 8)) {
                            //session is currently running, send request to the RM
                            rmbuf = OBJ_NEW(opal_buffer_t);
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(rmbuf, &command,
                                            1, ORCM_RM_CMD_T))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(rmbuf);
                                result = rc;
                                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
                                    ORTE_ERROR_LOG(rc);
                                    OBJ_RELEASE(ans);
                                    return;
                                }
                                goto answer;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(rmbuf, &alloc,
                                                        1, ORCM_ALLOC))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(rmbuf);
                                result = rc;
                                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
                                    ORTE_ERROR_LOG(rc);
                                    OBJ_RELEASE(ans);
                                    return;
                                }
                                goto answer;
                            }
                            if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER,
                                                      rmbuf,
                                                      ORCM_RML_TAG_RM,
                                                      orte_rml_send_callback,
                                                      NULL))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(rmbuf);
                                result = rc;
                                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
                                    ORTE_ERROR_LOG(rc);
                                    OBJ_RELEASE(ans);
                                    return;
                                }
                                goto answer;
                            }
                        }
                    }
                    if (true == found) {
                        break;
                    }
                }
                if (true == found) {
                    break;
                }
            }
        }

        /* send confirmation back to sender */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(sender, ans,
                                          ORCM_RML_TAG_SCD,
                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }
        return;
    } else if (ORCM_GET_POWER_COMMAND == command) {
        /* unpack the subcommand */
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &sub_command,
                                                  &cnt, ORCM_SCD_CMD_T))) {
            ORTE_ERROR_LOG(rc);
            goto answer;
        }

        /* unpack the bool (tells us if this is global or per_session) */
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &per_session,
                                                  &cnt, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            goto answer;
        }

        if (false == per_session) {
            //global
            switch(sub_command) {
            case ORCM_GET_POWER_BUDGET_COMMAND:
                int_param = orcm_scd_base_get_cluster_power_budget();
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
            break;
            case ORCM_GET_POWER_MODE_COMMAND:
                int_param = orcm_scd_base_get_cluster_power_mode();
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
            break;
            case ORCM_GET_POWER_WINDOW_COMMAND:
                int_param = orcm_scd_base_get_cluster_power_window();
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
            break;
            case ORCM_GET_POWER_OVERAGE_COMMAND:
                int_param = orcm_scd_base_get_cluster_power_overage();
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
            break;
            case ORCM_GET_POWER_UNDERAGE_COMMAND:
                int_param = orcm_scd_base_get_cluster_power_underage();
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
            break;
            case ORCM_GET_POWER_OVERAGE_TIME_COMMAND:
                int_param = orcm_scd_base_get_cluster_power_overage_time();
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
            break;
            case ORCM_GET_POWER_UNDERAGE_TIME_COMMAND:
                int_param = orcm_scd_base_get_cluster_power_underage_time();
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
            break;
            case ORCM_GET_POWER_FREQUENCY_COMMAND:
                float_param = orcm_scd_base_get_cluster_power_frequency();
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &float_param, 1, OPAL_FLOAT))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
            break;
            case ORCM_GET_POWER_STRICT_COMMAND:
                bool_param = orcm_scd_base_get_cluster_power_strict();
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &bool_param, 1, OPAL_BOOL))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
            break;
            default:
                rc = ORTE_ERR_BAD_PARAM;
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &rc, 1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
            }
        }
        else {
            //per session
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &sessionid,
                                                      &cnt, ORCM_ALLOC_ID_T))) {
                ORTE_ERROR_LOG(rc);
                goto answer;
            }

            //let's find the session
            found = false;
            /* for each queue, */
            OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
                OPAL_LIST_FOREACH(session, &q->sessions, orcm_session_t) {
                    alloc = session->alloc;
                    if(alloc->id == sessionid) { //found the session
                        found = true;
                        switch(sub_command) {
                        case ORCM_GET_POWER_BUDGET_COMMAND:
                            if (false == orte_get_attribute(&alloc->constraints, ORCM_PWRMGMT_POWER_BUDGET_KEY,
                                                            (void**)&int_param_ptr, OPAL_INT32)) {
                                result = ORTE_ERR_BAD_PARAM;
                                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
                                    ORTE_ERROR_LOG(rc);
                                    OBJ_RELEASE(ans);
                                    return;
                                }
                                goto answer;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                        break;
                        case ORCM_GET_POWER_MODE_COMMAND:
                            if (false == orte_get_attribute(&alloc->constraints, ORCM_PWRMGMT_POWER_MODE_KEY,
                                                            (void**)&int_param_ptr, OPAL_INT32)) {
                                result = ORTE_ERR_BAD_PARAM;
                                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
                                    ORTE_ERROR_LOG(rc);
                                    OBJ_RELEASE(ans);
                                    return;
                                }
                                goto answer;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                        break;
                        case ORCM_GET_POWER_WINDOW_COMMAND:
                            if (false == orte_get_attribute(&alloc->constraints, ORCM_PWRMGMT_POWER_WINDOW_KEY,
                                                            (void**)&int_param_ptr, OPAL_INT32)) {
                                result = ORTE_ERR_BAD_PARAM;
                                if (OPAL_SUCCESS != (result = opal_dss.pack(ans, &rc, 1, OPAL_INT))) {
                                    ORTE_ERROR_LOG(rc);
                                    OBJ_RELEASE(ans);
                                    return;
                                }
                                goto answer;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                        break;
                        case ORCM_GET_POWER_OVERAGE_COMMAND:
                            if (false == orte_get_attribute(&alloc->constraints, ORCM_PWRMGMT_CAP_OVERAGE_LIMIT_KEY,
                                                            (void**)&int_param_ptr, OPAL_INT32)) {
                                result = ORTE_ERR_BAD_PARAM;
                                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
                                    ORTE_ERROR_LOG(rc);
                                    OBJ_RELEASE(ans);
                                    return;
                                }
                                goto answer;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                        break;
                        case ORCM_GET_POWER_UNDERAGE_COMMAND:
                            if (false == orte_get_attribute(&alloc->constraints, ORCM_PWRMGMT_CAP_UNDERAGE_LIMIT_KEY,
                                                            (void**)&int_param_ptr, OPAL_INT32)) {
                                result = ORTE_ERR_BAD_PARAM;
                                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
                                    ORTE_ERROR_LOG(rc);
                                    OBJ_RELEASE(ans);
                                    return;
                                }
                                goto answer;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                        break;
                        case ORCM_GET_POWER_OVERAGE_TIME_COMMAND:
                            if (false == orte_get_attribute(&alloc->constraints, ORCM_PWRMGMT_CAP_OVERAGE_TIME_LIMIT_KEY,
                                                            (void**)&int_param_ptr, OPAL_INT32)) {
                                result = ORTE_ERR_BAD_PARAM;
                                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
                                    ORTE_ERROR_LOG(rc);
                                    OBJ_RELEASE(ans);
                                    return;
                                }
                                goto answer;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                        break;
                        case ORCM_GET_POWER_UNDERAGE_TIME_COMMAND:
                            if (false == orte_get_attribute(&alloc->constraints, ORCM_PWRMGMT_CAP_UNDERAGE_TIME_LIMIT_KEY,
                                                            (void**)&int_param_ptr, OPAL_INT32)) {
                                result = ORTE_ERR_BAD_PARAM;
                                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
                                    ORTE_ERROR_LOG(rc);
                                    OBJ_RELEASE(ans);
                                    return;
                                }
                                goto answer;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &int_param, 1, OPAL_INT32))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                        break;
                        case ORCM_GET_POWER_FREQUENCY_COMMAND:
                            if (false == orte_get_attribute(&alloc->constraints, ORCM_PWRMGMT_MANUAL_FREQUENCY_KEY,
                                                            (void**)&float_param_ptr, OPAL_FLOAT)) {
                                result = ORTE_ERR_BAD_PARAM;
                                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
                                    ORTE_ERROR_LOG(rc);
                                    OBJ_RELEASE(ans);
                                    return;
                                }
                                goto answer;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &float_param, 1, OPAL_FLOAT))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                        break;
                        case ORCM_GET_POWER_STRICT_COMMAND:
                            if (false == orte_get_attribute(&alloc->constraints, ORCM_PWRMGMT_FREQ_STRICT_KEY,
                                                            (void**)&bool_param_ptr, OPAL_BOOL)) {
                                result = ORTE_ERR_BAD_PARAM;
                                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
                                    ORTE_ERROR_LOG(rc);
                                    OBJ_RELEASE(ans);
                                    return;
                                }
                                goto answer;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &success, 1, OPAL_INT))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &bool_param, 1, OPAL_BOOL))) {
                                ORTE_ERROR_LOG(rc);
                                OBJ_RELEASE(ans);
                                return;
                            }
                        break;
                       default:
                           rc = ORTE_ERR_BAD_PARAM;
                           if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &rc, 1, OPAL_INT))) {
                               ORTE_ERROR_LOG(rc);
                               OBJ_RELEASE(ans);
                               return;
                           }
                       }
                    }
                    if (true == found) {
                        break;
                    }
                }
                if (true == found) {
                    break;
                }
            }
        }

        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(sender, ans,
                                          ORCM_RML_TAG_SCD,
                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }
        return;
    }

 answer:
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender, ans,
                                                      ORCM_RML_TAG_SCD,
                                                      orte_rml_send_callback,
                                                      NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(ans);
        return;
    }
}

typedef struct {
    int dbhandle;
    int session_handle;
    int number_of_rows;
    int status;
    bool active;
} fetch_cb_data, *fetch_cb_data_ptr;

void open_callback(int dbhandle, int status, opal_list_t *in, opal_list_t *out, void *cbdata)
{
    fetch_cb_data* data = (fetch_cb_data*)cbdata;
    data->dbhandle = dbhandle;
    data->status = status;
    data->session_handle = -1;
    data->number_of_rows = -1;
    data->active = false;
}

void close_callback(int dbhandle, int status, opal_list_t *in, opal_list_t *out, void *cbdata)
{
    fetch_cb_data* data = (fetch_cb_data*)cbdata;
    data->status = status;
    data->active = false;
}

void fetch_callback(int dbhandle, int status, opal_list_t *in, opal_list_t *out, void *cbdata)
{
    fetch_cb_data* data = (fetch_cb_data*)cbdata;
    if(ORCM_SUCCESS == status && dbhandle == data->dbhandle) {
        if(NULL != out && 0 != opal_list_get_size(out)) {
            opal_value_t *handle_object = (opal_value_t*)opal_list_get_first(out);
            if(NULL != handle_object && OPAL_INT == handle_object->type) {
                data->session_handle = handle_object->data.integer;
            } else {
                status = ORCM_ERROR;
            }
        } else {
            status = ORCM_ERROR;
        }
    }
    if(NULL != in) {
        OBJ_RELEASE(in);
    }
    if(NULL != out) {
        OBJ_RELEASE(out);
    }
    data->status = status;
    data->active = false;
}

bool is_wanted_column(const char* name)
{
    static char *wanted_column_names[] = {
        "hostname",
        "feature",
        "value",
        NULL
    };
    bool rv = false;
    for(int i = 0; NULL != wanted_column_names[i]; ++i) {
        if(0 == strcmp(wanted_column_names[i], name)) {
            rv = true;
            break;
        }
    }
    return rv;
}

char* get_plugin_from_sensor_name(const char* sensor_name)
{
    char* pos1 = strchr(sensor_name, '_');
    if(NULL != pos1) {
        char* pos2 = strchr(++pos1, '_');
        if(NULL != pos2) {
            size_t length = (size_t)(--pos2) - (size_t)pos1 + 2;
            char* plugin = (char*)malloc(length);
            if(NULL != plugin) {
                strncpy(plugin, pos1, length - 1);
                plugin[length - 1] = '\0';
            }
            return plugin;
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

#define SAFE_FREE(x) if(NULL!=x) { free(x); x = NULL; }
#define SAFE_OBJ_RELEASE(x) if(NULL!=x) { OBJ_RELEASE(x); x = NULL; }

int assemble_response(opal_list_t *db_query_results, opal_buffer_t **response_buffer)
{
    int rc = ORCM_SUCCESS;
    int returned_status = 0;
    uint16_t results_count = 0;
    opal_value_t *tmp_value = NULL;

    /*Init response buffer*/
    *response_buffer = OBJ_NEW(opal_buffer_t);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(*response_buffer,
                                            &returned_status, 1, OPAL_INT))) {
        rc = ORCM_ERR_PACK_FAILURE;
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /*Add results*/
    if(ORCM_SUCCESS == rc && 0 == returned_status && NULL != db_query_results) {
        results_count = (uint16_t)opal_list_get_size(db_query_results);
        opal_output_verbose(4, "Results count to send back %d", results_count);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(*response_buffer, &results_count, 1, OPAL_UINT16))) {
            rc = ORCM_ERR_PACK_FAILURE;
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        /*Pack the results list */
        OPAL_LIST_FOREACH(tmp_value, db_query_results, opal_value_t){
            if (OPAL_SUCCESS != (rc = opal_dss.pack(*response_buffer,
                                                    &tmp_value->data.string,
                                                    1, OPAL_STRING))) {
                rc = ORCM_ERR_PACK_FAILURE;
                ORTE_ERROR_LOG(rc);
                return rc;
            }
        }
    }
    return rc;
}

char *query_header(const char* db_view)
{
     if (0 == strcmp("nodes_idle_time_view", db_view)) {
         return "NODE,IDLE_TIME";
     } else if (0 == strcmp("node", db_view)) {
         return "NODE,STATUS";
     } else if (0 == strcmp("syslog_view", db_view)) {
         return "NODE,SENSOR_LOG,DATE_TIME,MESSAGE";
     } else {
         return "NODE,SENSOR,DATE_TIME,VALUE,UNITS";
     }
}

int build_filter_list(opal_buffer_t* buffer,opal_list_t **filter_list)
{
    int n = 1;
    uint8_t operation = 0;
    int rc = ORCM_SUCCESS;
    char* tmp_str = NULL;
    char* tmp_key = NULL;
    orcm_db_filter_t *tmp_filter = NULL;
    uint16_t filters_list_count = 0;

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &filters_list_count, &n, OPAL_UINT16))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    opal_output_verbose(4, "Filters list count in buffer: %d", filters_list_count);
    for(uint16_t i = 0; i < filters_list_count; ++i) {
        n = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &tmp_key, &n, OPAL_STRING))) {
            opal_output(0, "Retrieved key from unpack: %s", tmp_key);
            ORTE_ERROR_LOG(rc);
            return ORCM_ERROR;
        }
        opal_output_verbose(4, "Retrieved key: %s", tmp_key);
        n = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &operation, &n, OPAL_UINT8))) {
            opal_output(0, "Retrieved operation from unpack: %s", tmp_key);
            ORTE_ERROR_LOG(rc);
            return ORCM_ERROR;
        }
        opal_output_verbose(4, "Retrieved operation from unpack: %d", operation);
        n = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &tmp_str, &n, OPAL_STRING))) {
            opal_output(0, "Retrieved string from unpack: %s", tmp_str);
            ORTE_ERROR_LOG(rc);
            return ORCM_ERROR;
        }
        opal_output_verbose(4, "Retrieved string from unpack: %s", tmp_str);
        if (NULL == *filter_list) {
            *filter_list = OBJ_NEW(opal_list_t);
        }
        tmp_filter = OBJ_NEW(orcm_db_filter_t);
        tmp_filter->value.type = OPAL_STRING;
        tmp_filter->value.key = tmp_key;
        tmp_filter->value.data.string = tmp_str;
        tmp_filter->op = (orcm_db_comparison_op_t)operation;
        opal_list_append(*filter_list, &tmp_filter->value.super);
    }
    return rc;
}
#define SAFE_FREE(x) if(NULL!=x) { free(x); x = NULL; }
#define SAFE_OBJ_RELEASE(x) if(NULL!=x) { OBJ_RELEASE(x); x = NULL; }
#define TMP_STR_SIZE 1024

int query_db_view(opal_list_t *filters, opal_list_t **results, const char *db_view)
{
    int db_status = -1;
    fetch_cb_data data;
    opal_list_t *row = NULL;
    opal_value_t *string_row = NULL;
    opal_value_t *item = NULL;
    opal_list_t *fetch_output = OBJ_NEW(opal_list_t);
    char tmp_str[TMP_STR_SIZE];
    char tmp_ts[20];
    int num_rows = 0;
    int row_index = 0;
    size_t row_str_size = 0;
    size_t data_str_size = 0;
    time_t time_secs;
    struct tm *human_time;

    /*Setup fetch callback data*/
    data.dbhandle = -1;
    data.active = true;
    /*Open connection to DB*/
    orcm_db.open(NULL, NULL, open_callback, &data);
    ORTE_WAIT_FOR_COMPLETION(data.active);
    if (ORCM_SUCCESS != data.status){
        opal_output(0, "Failed to open database to retrieve sensor");
        if(NULL != filters){
            SAFE_OBJ_RELEASE(filters);
        }
        db_status = data.status;
        goto db_cleanup;
    }
    data.active = true;
    orcm_db.fetch(data.dbhandle, db_view, filters, fetch_output, fetch_callback, &data);
    ORTE_WAIT_FOR_COMPLETION(data.active);
    /*Free filters list as we no longer need it*/
    SAFE_OBJ_RELEASE(filters);
    if (ORCM_SUCCESS != data.status || -1 == data.session_handle) {
        opal_output(0, "Failed to fetch from the database");
        db_status = data.status;
        goto db_cleanup;
    }
    data.status = orcm_db.get_num_rows(data.dbhandle, data.session_handle, &num_rows);
    if (ORCM_SUCCESS != data.status) {
        opal_output(0, "Failed to get the number of sensor rows in the databse");
        db_status = data.status;
        goto db_cleanup;
    }
    opal_output_verbose(4, "The amount of rows obtained by query is: %d", num_rows);
    if(0 <  num_rows){
        *results = OBJ_NEW(opal_list_t);
        /*Create first item of results*/
        string_row = OBJ_NEW(opal_value_t);
        string_row->type = OPAL_STRING;
        string_row->data.string = strdup(query_header(db_view));
        opal_list_append(*results, &string_row->super);
        for (row_index = 0; row_index < num_rows; ++row_index){
            row = OBJ_NEW(opal_list_t);
            data.status = orcm_db.get_next_row(data.dbhandle, data.session_handle, row);
            if (ORCM_SUCCESS != data.status){
                opal_output(0, "Failed to get row %d when querying the database",
                            row_index);
                db_status = data.status;
                goto db_cleanup;
            }
            tmp_str[0] = '\0';
            OPAL_LIST_FOREACH(item, row, opal_value_t){
                switch (item->type){
                    case OPAL_STRING:
                        data_str_size = strlen(item->data.string);
                        if (sizeof(tmp_str) > strlen(tmp_str) + data_str_size + 1 ) {
                            strncat(tmp_str, item->data.string, data_str_size);
                        } else {
                            opal_output(0, "Failed to add value to row!");
                        }
                        strncat(tmp_str, ",", 1);
                        break;
                    default:
                        continue;
                }
            }
            string_row = OBJ_NEW(opal_value_t);
            string_row->type = OPAL_STRING;
            row_str_size = strlen(tmp_str)-1;
            /*Trim trailing comma*/
            if (0 < row_str_size){
                tmp_str[row_str_size]='\0';
            } else {
                opal_output(0, "Failed to remove trailing comma from row");
            }
            string_row->data.string = strdup(tmp_str);
            opal_list_append(*results, &string_row->super);
            SAFE_OBJ_RELEASE(row);
        }
    }
db_cleanup:
    data.status = orcm_db.close_result_set(data.dbhandle, data.session_handle);
    if (ORCM_SUCCESS != data.status){
        opal_output(0, "Failed to close the database results handle");
        SAFE_OBJ_RELEASE(*results);
    }
    data.active = true;
    orcm_db.close(data.dbhandle, close_callback, &data);
    ORTE_WAIT_FOR_COMPLETION(data.active);
    if (ORCM_SUCCESS != data.status) {
        opal_output(0, "Failed to close the database handle");
    }
    return db_status;
}

int get_inventory_list(opal_list_t *filters, opal_list_t **results)
{
    int rv = ORCM_SUCCESS;
    int num_rows = 0;
    opal_list_t *fetch_output = OBJ_NEW(opal_list_t);
    fetch_cb_data data;
    opal_list_t *row = NULL;
    opal_value_t *string_row = NULL;
    char* tmp = NULL;
    char* new_tmp = NULL;

    data.dbhandle = -1;

    data.active = true;
    orcm_db.open(NULL, NULL, open_callback, &data);
    ORTE_WAIT_FOR_COMPLETION(data.active);

    if(ORCM_SUCCESS != data.status) {
        opal_output(0, "Failed to open database to retrieve inventory");
        if(NULL != filters) {
            SAFE_OBJ_RELEASE(filters);
        }
        rv = data.status;
        goto clean_exit;
    }

    data.active = true;
    orcm_db.fetch(data.dbhandle, "node_features_view", filters, fetch_output, fetch_callback, &data);
    ORTE_WAIT_FOR_COMPLETION(data.active);

    if(ORCM_SUCCESS != data.status || -1 == data.session_handle) {
        opal_output(0, "Failed to fetch the inventory database");
        rv = data.status;
        goto clean_exit;
    }

    data.status = orcm_db.get_num_rows(data.dbhandle, data.session_handle, &num_rows);
    if(ORCM_SUCCESS != data.status) {
        opal_output(0, "Failed to get number of inventory rows in the inventory database");
        rv = data.status;
        goto clean_exit;
    }

    if(0 < num_rows) {
        bool first_item = true;
        *results = OBJ_NEW(opal_list_t);
        for(int i = 0; i < num_rows; ++i) {
            row = OBJ_NEW(opal_list_t);
            string_row = OBJ_NEW(opal_value_t);
            opal_value_t *item = NULL;
            bool first_column = true;
            int col_num = 0;

            data.status = orcm_db.get_next_row(data.dbhandle, data.session_handle, row);

            if(ORCM_SUCCESS != data.status) {
                opal_output(0, "Failed to get inventory row %d in the inventory database", i);
                rv = data.status;
                goto error_exit;
            }
            if(-1 == asprintf(&tmp, "\"")) {
                rv = ORCM_ERROR;
                goto error_exit;
            }
            OPAL_LIST_FOREACH(item, row, opal_value_t) {
                if(true == is_wanted_column(item->key)) {
                    if(false == first_column) {
                        if(-1 == asprintf(&new_tmp, "%s\",\"", tmp)) {
                            rv = ORCM_ERROR;
                            goto error_exit;
                        }
                        SAFE_FREE(tmp);
                        tmp = new_tmp;
                        new_tmp = NULL;
                    }
                    if(0 == strcmp(item->key, "feature"))
                    {
                        char* plugin = get_plugin_from_sensor_name(item->data.string);
                        if(-1 == asprintf(&new_tmp, "%s%s", tmp, plugin)) {
                            rv = ORCM_ERROR;
                            SAFE_FREE(plugin);
                            goto error_exit;
                        }
                        SAFE_FREE(plugin);
                        SAFE_FREE(tmp);
                        tmp = new_tmp;
                        new_tmp = NULL;
                    } else {
                        if(-1 == asprintf(&new_tmp, "%s%s", tmp, item->data.string)) {
                            rv = ORCM_ERROR;
                            goto error_exit;
                        }
                        SAFE_FREE(tmp);
                        tmp = new_tmp;
                        new_tmp = NULL;
                    }
                    if(true == first_column) {
                        first_column = false;
                    }
                }
                ++col_num;
            }
            if(-1 == asprintf(&new_tmp, "%s\"", tmp)) {
                rv = ORCM_ERROR;
                goto error_exit;
            }
            SAFE_FREE(tmp);
            tmp = new_tmp;
            new_tmp = NULL;
            if(true == first_item) {
                string_row->type = OPAL_STRING;
                string_row->data.string = strdup("\"Node Name\",\"Source Plugin Name\",\"Sensor Name\"");
                opal_list_append(*results, &string_row->super);
                string_row = OBJ_NEW(opal_value_t);
            }
            string_row->type = OPAL_STRING;
            string_row->data.string = strdup(tmp);
            opal_list_append(*results, &string_row->super);
            first_item = false;

            SAFE_FREE(tmp);
            SAFE_OBJ_RELEASE(row);
        }
    }
    goto clean_exit;

error_exit:
    SAFE_FREE(tmp);
    SAFE_OBJ_RELEASE(row);
    OBJ_RELEASE(string_row);
    SAFE_OBJ_RELEASE(*results);

clean_exit:
    data.status = orcm_db.close_result_set(data.dbhandle, data.session_handle);

    if(ORCM_SUCCESS != data.status) {
        opal_output(0, "Failed to close the inventory database results handle");
        SAFE_OBJ_RELEASE(*results);
    }
    data.active = true;
    orcm_db.close(data.dbhandle, close_callback, &data);
    ORTE_WAIT_FOR_COMPLETION(data.active);
    if(ORCM_SUCCESS != data.status) {
        opal_output(0, "Failed to close the inventory database handle");
    }

    return rv;
}
#undef SAFE_OBJ_RELEASE
#undef SAFE_FREE

void orcm_scd_base_fetch_recv(int status, orte_process_name_t* sender,
                              opal_buffer_t* buffer, orte_rml_tag_t tag,
                              void* cbdata)
{
    int rc = ORCM_SUCCESS;
    int n = 1;
    orcm_rm_cmd_flag_t command = 0;
    opal_list_t *filter_list = NULL;
    uint16_t filters_list_count = 0;
    char* tmp_str = NULL;
    char* tmp_key = NULL;
    opal_value_t *tmp_value = NULL;
    uint8_t operation = 0;
    orcm_db_filter_t *tmp_filter = NULL;
    opal_list_t *results_list = NULL;
    uint16_t results_count;
    opal_buffer_t *response_buffer = NULL;
    int returned_status = 0;

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &command, &n, ORCM_RM_CMD_T))) {
        ORTE_ERROR_LOG (rc);
        return;
    }
    switch(command) {
        case ORCM_GET_DB_QUERY_HISTORY_COMMAND:
            if (ORCM_ERROR == build_filter_list(buffer, &filter_list)){
                OBJ_RELEASE(filter_list);
            }
            query_db_view(filter_list, &results_list, "data_sensors_view");
            if (ORCM_SUCCESS != (rc = assemble_response(results_list, &response_buffer))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (NULL == response_buffer){
                rc = ORCM_ERR_BAD_PARAM;
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender,
                                                              response_buffer,
                                                              ORCM_RML_TAG_ORCMD_FETCH,
                                                              orte_rml_send_callback,
                                                              cbdata))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(response_buffer);
                return;
            }
            if(NULL != results_list){
                OBJ_RELEASE(results_list);
            }
            break;
        case ORCM_GET_DB_QUERY_SENSOR_COMMAND:
            if (ORCM_ERROR == build_filter_list(buffer, &filter_list)){
                OBJ_RELEASE(filter_list);
            }
            query_db_view(filter_list, &results_list, "data_sensors_view");
            if (ORCM_SUCCESS != (rc = assemble_response(results_list, &response_buffer))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (NULL == response_buffer){
                rc = ORCM_ERR_BAD_PARAM;
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender, response_buffer,
                                                              ORCM_RML_TAG_ORCMD_FETCH,
                                                              orte_rml_send_callback, cbdata))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(response_buffer);
                return;
            }
            if(NULL != results_list){
                OBJ_RELEASE(results_list);
            }
            break;
        case ORCM_GET_DB_QUERY_LOG_COMMAND:
            if (ORCM_ERROR == build_filter_list(buffer, &filter_list)){
                OBJ_RELEASE(filter_list);
            }
            query_db_view(filter_list, &results_list, "syslog_view");
            if (ORCM_SUCCESS != (rc = assemble_response(results_list, &response_buffer))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (NULL == response_buffer){
                rc = ORCM_ERR_BAD_PARAM;
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender,
                                                              response_buffer,
                                                              ORCM_RML_TAG_ORCMD_FETCH,
                                                              orte_rml_send_callback,
                                                              cbdata))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(response_buffer);
                return;
            }
            if(NULL != results_list){
                OBJ_RELEASE(results_list);
            }
            break;
        case ORCM_GET_DB_QUERY_IDLE_COMMAND:
            if (ORCM_ERROR == build_filter_list(buffer, &filter_list)){
                OBJ_RELEASE(filter_list);
            }
            query_db_view(filter_list, &results_list, "nodes_idle_time_view");
            if (ORCM_SUCCESS != (rc = assemble_response(results_list, &response_buffer))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (NULL == response_buffer){
                rc = ORCM_ERR_BAD_PARAM;
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender,
                                                              response_buffer,
                                                              ORCM_RML_TAG_ORCMD_FETCH,
                                                              orte_rml_send_callback,
                                                              cbdata))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(response_buffer);
                return;
            }
            if(NULL != results_list){
                OBJ_RELEASE(results_list);
            }
            break;
        case ORCM_GET_DB_QUERY_NODE_COMMAND:
            if (ORCM_ERROR == build_filter_list(buffer, &filter_list)){
                OBJ_RELEASE(filter_list);
            }
            query_db_view(filter_list, &results_list, "node");
            if (ORCM_SUCCESS != (rc = assemble_response(results_list, &response_buffer))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (NULL == response_buffer){
                rc = ORCM_ERR_BAD_PARAM;
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender,
                                                              response_buffer,
                                                              ORCM_RML_TAG_ORCMD_FETCH,
                                                              orte_rml_send_callback,
                                                              cbdata))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(response_buffer);
                return;
            }
            if(NULL != results_list){
                OBJ_RELEASE(results_list);
            }
            break;
        case ORCM_GET_DB_SENSOR_INVENTORY_COMMAND:
            /* Build filter list */
            n = 1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &filters_list_count, &n, OPAL_UINT16))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            for(uint16_t i = 0; i < filters_list_count; ++i) {
                n = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &tmp_key, &n, OPAL_STRING))) {
                    ORTE_ERROR_LOG(rc);
                    if(NULL != filter_list) {
                        OBJ_RELEASE(filter_list);
                    }
                    return;
                }
                n = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &operation, &n, OPAL_UINT8))) {
                    ORTE_ERROR_LOG(rc);
                    if(NULL != filter_list) {
                        OBJ_RELEASE(filter_list);
                    }
                    return;
                }
                n = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &tmp_str, &n, OPAL_STRING))) {
                    ORTE_ERROR_LOG(rc);
                    if(NULL != filter_list) {
                        OBJ_RELEASE(filter_list);
                    }
                    return;
                }
                if(NULL == filter_list) {
                    filter_list = OBJ_NEW(opal_list_t);
                }
                tmp_filter = OBJ_NEW(orcm_db_filter_t);
                tmp_filter->value.type = OPAL_STRING;
                tmp_filter->value.key = tmp_key;
                tmp_filter->value.data.string = tmp_str;
                tmp_filter->op = (orcm_db_comparison_op_t)operation;
                opal_list_append(filter_list, &tmp_filter->value.super);
            }
            returned_status = get_inventory_list(filter_list, &results_list);
            if (NULL != filter_list) {
                OBJ_RELEASE(filter_list);
            }
            response_buffer = OBJ_NEW(opal_buffer_t);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(response_buffer, &returned_status, 1, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                goto send_buffer;
            }
            opal_output(0, "rc: %d returned_status: %d results_list %p", rc, returned_status,results_list);
            if(ORCM_SUCCESS == rc && 0 == returned_status && NULL != results_list) {
                results_count = (uint16_t)opal_list_get_size(results_list);
                if (OPAL_SUCCESS != (rc = opal_dss.pack(response_buffer, &results_count, 1, OPAL_UINT16))) {
                    ORTE_ERROR_LOG(rc);
                    goto send_buffer;
                }
                OPAL_LIST_FOREACH(tmp_value, results_list, opal_value_t) {
                    if (OPAL_SUCCESS != (rc = opal_dss.pack(response_buffer, &tmp_value->data.string, 1, OPAL_STRING))) {
                        ORTE_ERROR_LOG(rc);
                        goto send_buffer;
                    }
                }
            }
send_buffer:
            if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender, response_buffer,
                                                              ORCM_RML_TAG_ORCMD_FETCH,
                                                              orte_rml_send_callback, cbdata))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(response_buffer);
                return;
            }
            if(NULL != results_list) {
                OBJ_RELEASE(results_list);
            }
            break;
        default:
            opal_output(0, "%s: COMMAND UNKNOWN", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    }
}
