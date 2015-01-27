/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
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

#include "orcm/util/attr.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/pwrmgmt/base/base.h"
#include "orcm/mca/scd/base/base.h"

static bool recv_issued=false;

static void orcm_scd_base_recv(int status, orte_process_name_t* sender,
                               opal_buffer_t* buffer, orte_rml_tag_t tag,
                               void* cbdata);

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
