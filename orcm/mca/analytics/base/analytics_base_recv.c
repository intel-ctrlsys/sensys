/*
 * Copyright (c) 2014-2016  Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
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
static void orcm_analytics_base_recv_send_answer(orte_process_name_t* sender,
                                                 opal_buffer_t *ans, int rc);

int orcm_analytics_base_comm_start(void)
{
    if (true == recv_issued) {
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
    if (true != recv_issued) {
        return ORCM_SUCCESS;
    }

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                         "%s analytics:base:receive stop comm",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
    recv_issued = false;
    return ORCM_SUCCESS;
}


int orcm_analytics_base_recv_pack_int(opal_buffer_t* buffer, int *value, int count)
{
    int ret;

    ret = opal_dss.pack(buffer, value, count, OPAL_INT);
    if (OPAL_SUCCESS != ret) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                             "%s analytics:base:receive can't pack value",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        ORTE_ERROR_LOG(ret);
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

static uint8_t orcm_analytics_base_recv_unpack_uint8_t(opal_buffer_t* buffer)
{
    int ret;
    uint8_t value;
    int count = ANALYTICS_COUNT_DEFAULT;

    /* unpack the command */
    ret = opal_dss.unpack(buffer, &value, &count, OPAL_UINT8);
    if (OPAL_SUCCESS != ret ) {
        ORTE_ERROR_LOG(ret);
        return ORCM_ERROR;
    }
    return value;
}

static void orcm_analytics_base_recv_send_answer(orte_process_name_t* sender,
                                                 opal_buffer_t *ans, int rc)
{
    int  ret;

    ret = orcm_analytics_base_recv_pack_int(ans, &rc, ANALYTICS_COUNT_DEFAULT);

    if (ORTE_SUCCESS != (ret = orte_rml.send_buffer_nb(sender, ans,
                                                       ORCM_RML_TAG_ANALYTICS,
                                                       orte_rml_send_callback,
                                                       NULL))) {
        ORTE_ERROR_LOG(ret);
    }
    return;
}

/* process incoming messages in order of receipt */
static void orcm_analytics_base_recv(int status, orte_process_name_t* sender,
                                     opal_buffer_t* buffer, orte_rml_tag_t tag,
                                     void* cbdata)
{
    int ret;
    int id;
    opal_buffer_t *ans = NULL;
    orcm_analytics_cmd_flag_t command;
    uint8_t storage_command;


    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                         "%s analytics:base:receive processing msg from %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(sender)));

    ans = OBJ_NEW(opal_buffer_t);
    if (NULL == ans) {
        return;
    }

    command = orcm_analytics_base_recv_unpack_uint8_t(buffer);

    switch (command) {
        case ORCM_ANALYTICS_WORKFLOW_ADD:
            ret = orcm_analytics_base_workflow_add(buffer, &id);
            if (ORCM_SUCCESS == ret) {
                ret = orcm_analytics_base_recv_pack_int(ans, &id, ANALYTICS_COUNT_DEFAULT);
            }
            break;
        case ORCM_ANALYTICS_WORKFLOW_REMOVE:
            ret = orcm_analytics_base_workflow_remove(buffer);
            break;
        case ORCM_ANALYTICS_WORKFLOW_LIST:
            ret = orcm_analytics_base_workflow_list(ans);
            break;
        case ORCM_ANALYTICS_SENSOR_STORAGE_POLICY:
            storage_command = orcm_analytics_base_recv_unpack_uint8_t(buffer);
            ret = orcm_analytics_base_control_storage(storage_command);
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
