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

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/rm/base/base.h"

static bool recv_issued=false;

static void orcm_rm_base_recv(int status, orte_process_name_t* sender,
                               opal_buffer_t* buffer, orte_rml_tag_t tag,
                               void* cbdata);

int orcm_rm_base_comm_start(void)
{
    if (recv_issued) {
        return ORTE_SUCCESS;
    }
    
    OPAL_OUTPUT_VERBOSE((5, orcm_rm_base_framework.framework_output,
                         "%s rm:base:receive start comm",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_RM,
                            ORTE_RML_PERSISTENT,
                            orcm_rm_base_recv,
                            NULL);
    recv_issued = true;
    
    return ORTE_SUCCESS;
}


int orcm_rm_base_comm_stop(void)
{
    if (!recv_issued) {
        return ORTE_SUCCESS;
    }
    
    OPAL_OUTPUT_VERBOSE((5, orcm_rm_base_framework.framework_output,
                         "%s rm:base:receive stop comm",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_RM);
    recv_issued = false;
    
    return ORTE_SUCCESS;
}


/* process incoming messages in order of receipt */
static void orcm_rm_base_recv(int status, orte_process_name_t* sender,
                               opal_buffer_t* buffer, orte_rml_tag_t tag,
                               void* cbdata)
{
    orcm_rm_cmd_flag_t command;
    int rc, cnt;
    opal_buffer_t *ans;
    char *state;
    orte_process_name_t node;

    OPAL_OUTPUT_VERBOSE((5, orcm_rm_base_framework.framework_output,
                         "%s rm:base:receive processing msg",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    /* always pass some answer back to the caller so they
     * don't hang
     */
    ans = OBJ_NEW(opal_buffer_t);

    /* unpack the command */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &command, &cnt, ORCM_RM_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto answer;
    }
    
    if (ORCM_NODESTATE_UPDATE_COMMAND == command) {
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &state, &cnt, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            goto answer;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &node, &cnt, ORTE_NAME))) {
            ORTE_ERROR_LOG(rc);
            goto answer;
        }

        /* set node to state */

        return;
    }
    else if (ORCM_NODESTATE_REQ_COMMAND == command) {
        /* get state of all nodes and pass back to caller
         */
        return;
    }
    else if (ORCM_RESOURCE_REQ_COMMAND == command) {
        /* resource request - verify state of resource, and set to allocated
         */
        return;
    }

 answer:
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender, ans,
                                                      ORCM_RML_TAG_RM,
                                                      orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(ans);
        return;
    }
}
