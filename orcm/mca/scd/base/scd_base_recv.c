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

#include "opal/mca/mca.h"
#include "opal/util/output.h"
#include "opal/util/malloc.h"
#include "opal/mca/base/base.h"

#include "orte/types.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/name_fns.h"

#include "orcm/runtime/orcm_globals.h"
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
    orcm_scd_cmd_flag_t command;
    int rc, i, j, cnt;
    orcm_alloc_t *alloc, **allocs;
    opal_buffer_t *ans;
    orcm_session_t *session;
    orcm_queue_t *q;
    orcm_node_t **nodes;
    orcm_session_id_t sessionid;

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:base:receive processing msg from %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ORTE_NAME_PRINT(sender)));

    /* always pass some answer back to the caller so they
     * don't hang
     */
    ans = OBJ_NEW(opal_buffer_t);

    /* unpack the command */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &command, &cnt, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto answer;
    }
    
    if (ORCM_SESSION_REQ_COMMAND == command) {
        /* session request - this comes in the form of a
         * requested allocation to support the session
         */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &alloc, &cnt, ORCM_ALLOC))) {
            ORTE_ERROR_LOG(rc);
            goto answer;
        }
        /* assign a session to it */
        session = OBJ_NEW(orcm_session_t);
        session->alloc = alloc;
        session->id = orcm_scd_base_get_next_session_id();
        session->alloc->id = session->id;

        /* send session id back to sender */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &session->id, 1, ORCM_ALLOC_ID_T))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }
        if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender, ans,
                                                          ORCM_RML_TAG_SCD,
                                                          orte_rml_send_callback, NULL))) {
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
            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &q->name, 1, OPAL_STRING))) {
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
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, allocs, i, ORCM_ALLOC))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }
                free(allocs);
            }
        }
        /* send back results */
        if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender, ans,
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
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &sessionid, &cnt, ORCM_ALLOC_ID_T))) {
            ORTE_ERROR_LOG(rc);
            goto answer;
        }

        session = OBJ_NEW(orcm_session_t);
        session->id = sessionid;
        /* pass it to the scheduler */
        ORCM_ACTIVATE_SCD_STATE(session, ORCM_SESSION_STATE_CANCEL);

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
            fprintf(stdout, "lowestfree: %i, size: %i\n", (int)orcm_scd_base.nodes.lowest_free, (int)orcm_scd_base.nodes.size);
            for (j = 0; j < orcm_scd_base.nodes.lowest_free; j++) {
                if (NULL == (nodes[i] = (orcm_node_t*)opal_pointer_array_get_item(&orcm_scd_base.nodes, j))) {
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
                                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }

        return;
    } else if (ORCM_VM_READY_COMMAND == command) {
        /* get session id to see which one is ready */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &sessionid, &cnt, ORCM_ALLOC_ID_T))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }
        /* TODO tell orun that VM is ready */
    } else if (ORCM_RUN_COMMAND == command) {
        return;
    }

 answer:
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender, ans,
                                                      ORCM_RML_TAG_SCD,
                                                      orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(ans);
        return;
    }
}
