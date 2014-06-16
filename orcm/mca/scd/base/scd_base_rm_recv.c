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
#include "opal/mca/base/base.h"

#include "orte/types.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/name_fns.h"
#include "orte/util/regex.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/cfgi/cfgi_types.h"
#include "orcm/mca/scd/base/base.h"

static bool rm_recv_issued=false;

static void orcm_scd_base_rm_base_recv(int status, orte_process_name_t* sender,
                               opal_buffer_t* buffer, orte_rml_tag_t tag,
                               void* cbdata);

int orcm_scd_base_rm_comm_start(void)
{
    if (rm_recv_issued) {
        return ORTE_SUCCESS;
    }
    
    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:base:rm:receive start comm",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_RM,
                            ORTE_RML_PERSISTENT,
                            orcm_scd_base_rm_base_recv,
                            NULL);
    rm_recv_issued = true;
    
    return ORTE_SUCCESS;
}


int orcm_scd_base_rm_comm_stop(void)
{
    if (!rm_recv_issued) {
        return ORTE_SUCCESS;
    }
    
    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:base:rm:receive stop comm",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_RM);
    rm_recv_issued = false;
    
    return ORTE_SUCCESS;
}


/* process incoming messages in order of receipt */
static void orcm_scd_base_rm_base_recv(int status, orte_process_name_t* sender,
                                       opal_buffer_t* buffer, orte_rml_tag_t tag,
                                       void* cbdata)
{
    orcm_rm_cmd_flag_t command;
    int rc, cnt, i;
    opal_buffer_t *ans;
    orcm_node_state_t state;
    orte_process_name_t node;
    orcm_node_t *nodeptr;
    orcm_alloc_t *alloc;
    orcm_session_t *session;
    bool found;

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:base:rm:receive processing msg",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    /* unpack the command */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &command,
                                              &cnt, ORCM_RM_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    
    if (ORCM_NODESTATE_UPDATE_COMMAND == command) {
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:base:rm:receive got ORCM_NODESTATE_UPDATE_COMMAND",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        ans = OBJ_NEW(opal_buffer_t);
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &state,
                                                  &cnt, OPAL_INT8))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &node,
                                                  &cnt, ORTE_NAME))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        /* set node to state */
        found = false;
        for (i = 0; i < orcm_scd_base.nodes.size; i++) {
            if (NULL == (nodeptr = 
                         (orcm_node_t*)opal_pointer_array_get_item(&orcm_scd_base.nodes,
                                                                   i))) {
                continue;
            }
            if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL,
                                                            &nodeptr->daemon,
                                                            &node)) {
                found = true;
                nodeptr->state = state;
                /* set to available for now, eventually pass this off to scheduler */
                nodeptr->scd_state = ORCM_SCD_NODE_STATE_UNALLOC;
                OPAL_OUTPUT_VERBOSE((1, orcm_scd_base_framework.framework_output,
                                     "%s scd:base:rm:receive Setting node %s to state %i (%s)",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_NAME_PRINT(&node),
                                     (int)state,
                                     orcm_node_state_to_str(state)));
                break;
            }
        }

        if (!found) {
            OPAL_OUTPUT_VERBOSE((1, orcm_scd_base_framework.framework_output,
                                 "%s scd:base:rm:receive Couldn't find node %s to update state",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 ORTE_NAME_PRINT(&node)));
        }

        OBJ_RELEASE(ans);
    } else if (ORCM_STEPD_COMPLETE_COMMAND == command) {
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:base:rm:receive got ORCM_STEPD_COMPLETE_COMMAND",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &alloc,
                                                  &cnt, ORCM_ALLOC))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        session = OBJ_NEW(orcm_session_t);
        session->alloc = alloc;
        session->id = session->alloc->id;
        ORCM_ACTIVATE_SCD_STATE(session, ORCM_SESSION_STATE_TERMINATED);
    }

    return;
}
