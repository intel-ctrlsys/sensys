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
static int analytics_base_recv_pack_int(opal_buffer_t* buffer, int value, int count);
static int analytics_base_recv_unpack_int(opal_buffer_t* buffer, int count);
static orcm_analytics_cmd_flag_t analytics_base_recv_unpack_command(opal_buffer_t* buffer,
                                                                    int count);
static void orcm_analytics_base_recv_send_answer(orte_process_name_t* sender,
                                                 opal_buffer_t *ans, int rc);

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


static int analytics_base_recv_pack_int(opal_buffer_t* buffer, int value, int count)
{
    int ret;

    if (OPAL_SUCCESS != (ret = opal_dss.pack(buffer, &value, count, OPAL_INT))) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                             "%s analytics:base:receive can't pack value",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        ORTE_ERROR_LOG(ret);
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}


static int analytics_base_recv_unpack_int(opal_buffer_t* buffer, int count)
{
    int ret, value;

    /* unpack the command */
    if (OPAL_SUCCESS != (ret = opal_dss.unpack(buffer, &value,
                                              &count, OPAL_INT))) {
        ORTE_ERROR_LOG(ret);
        return ORCM_ERROR;
    }
    return value;
}


static orcm_analytics_cmd_flag_t analytics_base_recv_unpack_command(opal_buffer_t* buffer,
                                                                    int count)
{
    int ret;
    orcm_analytics_cmd_flag_t value;

    /* unpack the command */
    if (OPAL_SUCCESS != (ret = opal_dss.unpack(buffer, &value,
                                              &count, ORCM_ANALYTICS_CMD_T))) {
        ORTE_ERROR_LOG(ret);
        return ORCM_ANALYTICS_WORFLOW_ERROR;
    }
    return value;
}

static void orcm_analytics_base_recv_send_answer(orte_process_name_t* sender,
                                                 opal_buffer_t *ans, int rc)
{
    int  ret;

    ret = analytics_base_recv_pack_int(ans, rc, ANALYTICS_COUNT_DEFAULT);

    if (ORTE_SUCCESS != (ret = orte_rml.send_buffer_nb(sender, ans,
                                                       ORCM_RML_TAG_ANALYTICS,
                                                       orte_rml_send_callback,
                                                       NULL))) {
        ORTE_ERROR_LOG(ret);
        OBJ_RELEASE(ans);
    }
    return;
}

/* process incoming messages in order of receipt */
static void orcm_analytics_base_recv(int status, orte_process_name_t* sender,
                                     opal_buffer_t* buffer, orte_rml_tag_t tag,
                                     void* cbdata)
{
    int ret, id;
    opal_buffer_t *ans;
    orcm_analytics_cmd_flag_t command;

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                         "%s analytics:base:receive processing msg from %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(sender)));
    
    ans = OBJ_NEW(opal_buffer_t);

    command = analytics_base_recv_unpack_command(buffer, ANALYTICS_COUNT_DEFAULT);
    
    switch (command) {
        case ORCM_ANALYTICS_WORKFLOW_CREATE:
            if (ORCM_SUCCESS == (ret = orcm_analytics_base_workflow_create(buffer, &id))) {
                ret = analytics_base_recv_pack_int(ans, id, ANALYTICS_COUNT_DEFAULT);
            }
            break;
        case ORCM_ANALYTICS_WORKFLOW_DELETE:
            /* unpack the id */
            if (ORCM_ERROR != (id = analytics_base_recv_unpack_int(buffer,
                                                                   ANALYTICS_COUNT_DEFAULT))) {
                ret = orcm_analytics_base_workflow_delete(id);
            }
            break;
        default:
            OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                                 "%s analytics:base:receive got unknown command from %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 ORTE_NAME_PRINT(sender)));
            ret = ORCM_ERR_BAD_PARAM;
            break;
    }

    orcm_analytics_base_recv_send_answer(sender, ans, ret);
    return;
}
