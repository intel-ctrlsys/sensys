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

#include "orcm/runtime/orcm_globals.h"

#include "orcm/mca/analytics/base/base.h"
#include "orcm/mca/analytics/base/analytics_private.h"

static bool recv_issued=false;

static void orcm_analytics_base_recv(int status, orte_process_name_t* sender,
                                     opal_buffer_t* buffer, orte_rml_tag_t tag,
                                     void* cbdata);

int orcm_analytics_base_comm_start(void)
{
    if (recv_issued) {
        return ORCM_SUCCESS;
    }
    
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                         "%s analytics:base:receive start comm",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_ANALYTICS,
                            ORTE_RML_PERSISTENT,
                            orcm_analytics_base_recv,
                            NULL);
    recv_issued = true;
    
    return ORCM_SUCCESS;
}


int orcm_analytics_base_comm_stop(void)
{
    if (!recv_issued) {
        return ORCM_SUCCESS;
    }
    
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                         "%s analytics:base:receive stop comm",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
    recv_issued = false;
    
    return ORCM_SUCCESS;
}


/* process incoming messages in order of receipt */
static void orcm_analytics_base_recv(int status, orte_process_name_t* sender,
                                     opal_buffer_t* buffer, orte_rml_tag_t tag,
                                     void* cbdata)
{
    int cnt, rc, id, ret;
    opal_buffer_t *ans;
    orcm_analytics_cmd_flag_t command;

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                         "%s analytics:base:receive processing msg from %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(sender)));
    
    /* always pass some answer back to the caller so they don't hang */
    ans = OBJ_NEW(opal_buffer_t);

    /* unpack the command */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &command,
                                              &cnt, ORCM_ANALYTICS_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto answer;
    }
    
    switch (command) {
        case ORCM_ANALYTICS_WORKFLOW_CREATE:
            rc = orcm_analytics_base_workflow_create(buffer, &id);
            if (OPAL_SUCCESS != (ret = opal_dss.pack(ans, &id, 1, OPAL_INT))) {
                OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                                     "%s analytics:base:receive cant pack workflow id, sender %s is likely hung",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_NAME_PRINT(sender)));
                /* FIXME should we delete the workflow if the sender can't track it ? */
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(ans);
                return;
            }
            break;
        case ORCM_ANALYTICS_WORKFLOW_DELETE:
            /* unpack the command */
            cnt = 1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &id,
                                                      &cnt, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                goto answer;
            }
            rc = orcm_analytics_base_workflow_delete(id);
            break;
        default:
            OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                                 "%s analytics:base:receive got unknown command from %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 ORTE_NAME_PRINT(sender)));
            rc = ORCM_ERR_BAD_PARAM;
            break;
    }

answer:
    if (OPAL_SUCCESS != (ret = opal_dss.pack(ans, &rc, 1, OPAL_INT))) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                             "%s analytics:base:receive cant pack return value, sender %s is likely hung",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_NAME_PRINT(sender)));
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(ans);
        return;
    }
    
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender, ans,
                                                      ORCM_RML_TAG_ANALYTICS,
                                                      orte_rml_send_callback,
                                                      NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(ans);
        return;
    }
}
