/*
 * Copyright (c) 2015-2016   Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "opal/dss/dss.h"

#include "orcm/mca/analytics/analytics_types.h"
#include "orcm/tools/octl/common.h"
#include "orcm/util/logical_group.h"
#include "orcm/util/utils.h"
#include "orcm/mca/parser/parser.h"
#include "orcm/mca/parser/base/base.h"

/******************
 * Local Functions
 ******************/
static void workflow_process_error(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer, char **list);
static void workflow_output_setup(orte_rml_recv_cb_t *xfer);
static int workflow_send_buffer(orte_process_name_t *wf_agg,
                                              opal_buffer_t *buf, orte_rml_recv_cb_t *xfer);
static int workflow_add_unpack_buffer(orte_rml_recv_cb_t *xfer);
static opal_list_t* workflow_add_retrieve_workflows_section(const char *file);
static int workflow_add_send_workflow(opal_buffer_t *buf, char **aggregator);
static int workflow_add_process_single_workflow(opal_list_t *result_list, char **aggregator, char *list_head_key);
static int workflow_add_extract_aggregator(char ***aggregator, char *cli_aggregator, char *key, char *value);
static int workflow_add_process_workflows (opal_list_t *result_list, char *cli_aggregator);
static int workflow_add_parse_args(char **value, char **file, char **aggregator);
static int workflow_remove_parse_args(char **value, char ***aggregator, char **workflow_name, char **workflow_id);
static int workflow_remove_pack_buffer(opal_buffer_t *buf, char *workflow_name,
                                           char *workflow_id, orte_rml_recv_cb_t *xfer);
static int workflow_remove_unpack_buffer(orte_rml_recv_cb_t *xfer);
static int workflow_list_parse_args(char **value, char ***aggregator);
static int workflow_list_pack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer);
static int workflow_list_unpack_buffer(orte_rml_recv_cb_t *xfer);



static void workflow_process_error(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer, char **list)
{
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);

    if (NULL != buf) {
        OBJ_RELEASE(buf);
    }
    if (NULL != xfer) {
        OBJ_RELEASE(xfer);
    }

    SAFEFREE (list);
}

static void workflow_output_setup(orte_rml_recv_cb_t *xfer)
{
    /* setup to receive the result */
    xfer->active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_ANALYTICS,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, xfer);

}

static int workflow_send_buffer(orte_process_name_t *wf_agg,
                                              opal_buffer_t *buf, orte_rml_recv_cb_t *xfer)
{
    int rc;

    /* send it to the aggregator */
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(wf_agg, buf,
                                                      ORCM_RML_TAG_ANALYTICS,
                                                      orte_rml_send_callback, NULL))) {
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

static int workflow_add_unpack_buffer(orte_rml_recv_cb_t *xfer)
{
    int n;
    int rc;
    int workflow_id;

    n=1;
    if (ORCM_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &workflow_id, &n, OPAL_INT))) {
        return ORCM_ERROR;
    }

    if (0 > workflow_id) {
        orte_show_help("help-octl.txt", "octl:workflow:workflow-failure",
                       true);
        rc = workflow_id;
        return rc;
    }
    ORCM_UTIL_MSG("Workflow created with id: %i", workflow_id);
    return ORCM_SUCCESS;

}

static opal_list_t* workflow_add_retrieve_workflows_section(const char *file)
{
    opal_list_t *result_list = NULL;
    int rc;

    if (ORTE_SUCCESS != (rc = mca_base_framework_open(&orcm_parser_base_framework, 0))) {
        ORTE_ERROR_LOG(rc);
        return result_list;
    }
    if (ORTE_SUCCESS != (rc = orcm_parser_base_select())) {
        ORTE_ERROR_LOG(rc);
        (void) mca_base_framework_close(&orcm_parser_base_framework);
        return result_list;
    }

    result_list = orcm_util_workflow_add_retrieve_workflows_section(file);

    (void) mca_base_framework_close(&orcm_parser_base_framework);
    return result_list;

}


static int workflow_add_send_workflow(opal_buffer_t *buf, char **aggregator)
{
    orte_process_name_t wf_agg;
    orte_rml_recv_cb_t *xfer = NULL;
    int rc;
    int node_index;

    for (node_index = 0; node_index < opal_argv_count(aggregator); node_index++) {

        xfer = OBJ_NEW(orte_rml_recv_cb_t);
        if (NULL == xfer) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }

        workflow_output_setup(xfer);

        OBJ_RETAIN(buf);

        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(aggregator[node_index],
                                                                   &wf_agg))) {
            orte_show_help("help-octl.txt","octl:hostname-notfound",
                           true, aggregator[node_index]);
            orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
            SAFE_RELEASE(xfer);
            return rc;
        }

        rc = workflow_send_buffer(&wf_agg, buf, xfer);
        if (ORCM_SUCCESS != rc) {
            orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
            SAFE_RELEASE(xfer);
            return rc;
        }

        /* unpack workflow id */
        ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
        if (ORCM_SUCCESS != rc) {
            orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
            SAFE_RELEASE(xfer);
            return rc;
        }

        rc = workflow_add_unpack_buffer(xfer);
        if (ORCM_SUCCESS != rc) {
            orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
            SAFE_RELEASE(xfer);
            return rc;
        }
        orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
        SAFE_RELEASE(xfer);
    }
    return ORCM_SUCCESS;
}

static int workflow_add_process_single_workflow(opal_list_t *result_list, char **aggregator, char *list_head_key)
{
    orcm_analytics_cmd_flag_t command;
    opal_buffer_t *buf = NULL;
    int rc;
    bool is_filter_first_step = false;

    buf = OBJ_NEW(opal_buffer_t);
    if (NULL == buf) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    command = ORCM_ANALYTICS_WORKFLOW_ADD;
    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, OPAL_UINT8))) {
        SAFE_RELEASE(buf);
        return rc;
    }
    rc = orcm_util_workflow_add_extract_workflow_info(result_list, buf, list_head_key, &is_filter_first_step);
    if ((ORCM_SUCCESS != rc) || (false == is_filter_first_step) ) {
        SAFE_RELEASE(buf);
        return rc;
    }
    rc = workflow_add_send_workflow(buf, aggregator);
    if (ORCM_SUCCESS != rc) {
        SAFE_RELEASE(buf);
        return rc;
    }
    SAFE_RELEASE(buf);
    return ORCM_SUCCESS;
}

static int workflow_add_extract_aggregator(char ***aggregator, char *cli_aggregator, char *key, char *value)
{
    int rc;

    if (NULL != cli_aggregator) {
        rc = orcm_logical_group_parse_array_string(cli_aggregator, aggregator);
        if (ORCM_SUCCESS != rc) {
            orte_show_help("help-octl.txt", "octl:workflow:aggregator", true);
            return ORCM_ERROR;
        }
    }
    else if (0 == strcmp("aggregator", key)) {
        rc = orcm_logical_group_parse_array_string(value, aggregator);
        if (ORCM_SUCCESS != rc) {
            orte_show_help("help-octl.txt", "octl:workflow:aggregator", true);
            return ORCM_ERROR;
        }
    }
    else {
        orte_show_help("help-octl.txt", "octl:workflow:aggregator", true);
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

static int workflow_add_process_workflows (opal_list_t *result_list, char *cli_aggregator)
{
    orcm_value_t *list_item = NULL;
    int rc = ORCM_SUCCESS;
    char **aggregator = NULL;

    if (NULL == result_list){
        return ORCM_ERROR;
    }

    OPAL_LIST_FOREACH(list_item, result_list, orcm_value_t) {
        ORCM_UTIL_MSG("Key: %s", list_item->value.key);
        ORCM_UTIL_MSG("Type: %d", list_item->value.type);
        if (list_item->value.type == OPAL_STRING) {
            ORCM_UTIL_MSG("value: %s", list_item->value.data.string);

            SAFEFREE(aggregator);
            rc = workflow_add_extract_aggregator(&aggregator, cli_aggregator, list_item->value.key, list_item->value.data.string);
            if (ORCM_SUCCESS != rc) {
                SAFEFREE(aggregator);
                return rc;
            }
        }
        else if (list_item->value.type == OPAL_PTR) {
            if (NULL == aggregator) {
                if (NULL != cli_aggregator) {
                    rc = orcm_logical_group_parse_array_string(cli_aggregator, &aggregator);
                    if (ORCM_SUCCESS != rc) {
                        orte_show_help("help-octl.txt", "octl:workflow:aggregator", true);
                        SAFEFREE(aggregator);
                        return ORCM_ERROR;
                    }
                }
                else {
                    orte_show_help("help-octl.txt", "octl:workflow:aggregator", true);
                    SAFEFREE(aggregator);
                    return ORCM_ERROR;
                }
            }
            if (0 == strcmp(list_item->value.key, "workflow")) {
                rc = workflow_add_process_single_workflow((opal_list_t*)list_item->value.data.ptr, aggregator, list_item->value.key);
            }
        }
        else {
            ORCM_UTIL_MSG("Unexpected data type from parser framework");
            SAFEFREE(aggregator);
            return ORCM_ERROR;
        }
    }
    SAFEFREE(aggregator);
    return rc;
}

static int workflow_add_parse_args(char **value, char **file, char **aggregator)
{
    if (2 > opal_argv_count(value)) {
        orte_show_help("help-octl.txt",
                       "octl:workflow:add-usage", true, "invalid arguments!");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL != value[2]) {
        *file = value[2];
    }
    else {
        orte_show_help("help-octl.txt",
                       "octl:workflow:workflow-notfound", true, value[2]);
        return ORCM_ERR_BAD_PARAM;
    }


    if (NULL != value[3]) {
        *aggregator = value[3];
    }

    return ORCM_SUCCESS;
}

int orcm_octl_workflow_add(char **value)
{
    opal_list_t *result_list = NULL;
    int rc = ORCM_SUCCESS;
    orcm_value_t *list_item = NULL;
    char *file = NULL;
    char *aggregator = NULL;

    rc = workflow_add_parse_args(value, &file, &aggregator);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    result_list = workflow_add_retrieve_workflows_section(file);
    if (NULL == result_list) {
        SAFEFREE(aggregator);
        return ORCM_ERROR;
    }

    rc= ORCM_ERROR;
    OPAL_LIST_FOREACH(list_item, result_list, orcm_value_t) {
        ORCM_UTIL_MSG("Key: %s", list_item->value.key);
        ORCM_UTIL_MSG("Type: %d", list_item->value.type);

        if (list_item->value.type == OPAL_PTR) {
            if (0 == strcmp(list_item->value.key, "workflows")) {
                rc = workflow_add_process_workflows((opal_list_t*)list_item->value.data.ptr, aggregator);
            }
        }
        else {
            ORCM_UTIL_MSG("Unexpected data type from parser framework");
            SAFE_RELEASE(result_list);
            SAFEFREE(aggregator);
            return ORCM_ERROR;
        }
    }
    SAFEFREE(aggregator);
    SAFE_RELEASE(result_list);
    return rc;
}

static int workflow_remove_parse_args(char **value, char ***aggregator, char **workflow_name, char **workflow_id)
{
    int rc;

    if (5 != opal_argv_count(value)) {
        orte_show_help("help-octl.txt",
                       "octl:workflow:remove-usage", true, "invalid arguments!");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL != value[2]) {
        rc = orcm_logical_group_parse_array_string(value[2], aggregator);
        if (ORCM_SUCCESS != rc) {
            return ORCM_ERROR;
        }

    }
    else {
        orte_show_help("help-octl.txt","octl:aggregator-null", true);
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL != value[3]) {
        *workflow_name = value[3];
    }
    else {
        orte_show_help("help-octl.txt",
                       "octl:workflow:workflow-name", true, value[3]);
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL != value[4]) {
        *workflow_id = value[4];
    }
    else {
        orte_show_help("help-octl.txt",
                       "octl:workflow:workflow-id", true, value[4]);
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;

}

static int workflow_remove_pack_buffer(opal_buffer_t *buf, char *workflow_name,
                                           char *workflow_id, orte_rml_recv_cb_t *xfer)
{
    orcm_analytics_cmd_flag_t command;
    int rc;

    command = ORCM_ANALYTICS_WORKFLOW_REMOVE;
    /* pack the alloc command flag */
    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, OPAL_UINT8))) {
        return ORCM_ERROR;
    }

    /* pack the workflow name */
    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &workflow_name, 1, OPAL_STRING))) {
        return ORCM_ERROR;
    }

    /* pack the workflow id */
    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &workflow_id, 1, OPAL_STRING))) {
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

static int workflow_remove_unpack_buffer(orte_rml_recv_cb_t *xfer)
{
    int n;
    int rc;
    int result;

    n=1;

    if (ORCM_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &result, &n, OPAL_INT))) {
        return ORCM_ERROR;
    }
    if (ORCM_ERROR != result) {
        ORCM_UTIL_MSG("Workflow(s) deleted");
    }
    else {
        orte_show_help("help-octl.txt","octl:workflow:workflow-notfound", true);
    }
    return ORCM_SUCCESS;

}

int orcm_octl_workflow_remove(char **value)
{
    char *workflow_id = NULL;
    orte_rml_recv_cb_t *xfer = NULL;
    opal_buffer_t *buf;
    orte_process_name_t wf_agg;
    char **aggregator=NULL;
    char *workflow_name = NULL;
    int node_index;
    int rc;

    rc = workflow_remove_parse_args(value, &aggregator, &workflow_name, &workflow_id);
    if ((ORCM_SUCCESS != rc) ||(NULL == aggregator) ) {
        SAFEFREE(aggregator);
        return rc;
    }


    buf = OBJ_NEW(opal_buffer_t);
    if (NULL == buf) {
        SAFEFREE (aggregator);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    for (node_index = 0; node_index < opal_argv_count(aggregator); node_index++) {

        xfer = OBJ_NEW(orte_rml_recv_cb_t);
        if (NULL == xfer) {
            SAFEFREE (aggregator);
            OBJ_RELEASE(buf);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        workflow_output_setup(xfer);

        OBJ_RETAIN(buf);

        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(aggregator[node_index],
                                                                   &wf_agg))) {
            workflow_process_error(buf, xfer, aggregator);
            return rc;
        }

        rc = workflow_remove_pack_buffer(buf, workflow_name, workflow_id, xfer);
        if (ORCM_SUCCESS != rc) {
            workflow_process_error(buf, xfer, aggregator);
            return rc;
        }

        rc = workflow_send_buffer(&wf_agg, buf, xfer);
        if (ORCM_SUCCESS != rc) {
            workflow_process_error(buf, xfer, aggregator);
            return rc;
        }

        ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
        if (ORCM_SUCCESS != rc) {
            workflow_process_error(buf, xfer, aggregator);
            return rc;
        }

        rc = workflow_remove_unpack_buffer(xfer);
        if (ORCM_SUCCESS != rc) {
            workflow_process_error(buf, xfer, aggregator);
            return rc;
        }

        OBJ_RELEASE(xfer);
    }
    workflow_process_error(buf, NULL, aggregator);
    return ORCM_SUCCESS;

}

static int workflow_list_parse_args(char **value, char ***aggregator)
{
    int rc;

    if (3 != opal_argv_count(value)) {
        orte_show_help("help-octl.txt",
                       "octl:workflow:get-usage", true, "invalid arguments!");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL != value[2]) {
        rc = orcm_logical_group_parse_array_string(value[2], aggregator);
        if (ORCM_SUCCESS != rc) {
            return ORCM_ERROR;
        }

    }
    else {
        orte_show_help("help-octl.txt",
                       "octl:aggregator-null", true, "invalid arguments!");
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;
}

static int workflow_list_pack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer)
{
    int rc;
    orcm_analytics_cmd_flag_t command;

    command = ORCM_ANALYTICS_WORKFLOW_LIST;
    /* pack the alloc command flag */
    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, OPAL_UINT8))) {

        return rc;
    }
    return ORCM_SUCCESS;
}

static int workflow_list_unpack_buffer(orte_rml_recv_cb_t *xfer)
{
    int workflow_count;
    int n;
    int workflow_id;
    char *workflow_name = NULL;
    int rc;
    int temp;

    n=1;

    if (ORCM_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &workflow_count, &n, OPAL_INT))) {
        return ORCM_ERROR;
    }

    for (temp = 0; temp < workflow_count; temp++) {
        if (ORCM_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &workflow_id, &n, OPAL_INT))) {
            return ORCM_ERROR;
        }
        if (ORCM_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &workflow_name, &n, OPAL_STRING))) {
            return ORCM_ERROR;
        }
        ORCM_UTIL_MSG("workflow name is: %s   workflow id is: %d", workflow_name, workflow_id);
        SAFEFREE(workflow_name);
    }
    if (0 == workflow_count) {
        ORCM_UTIL_MSG("No workflow ids");
    }
    return ORCM_SUCCESS;
}


int orcm_octl_workflow_list(char **value)
{
    orte_rml_recv_cb_t *xfer = NULL;
    opal_buffer_t *buf;
    orte_process_name_t wf_agg;
    char **aggregator=NULL;
    int node_index;
    int rc;

    rc = workflow_list_parse_args(value, &aggregator);
    if (ORCM_SUCCESS != rc) {
        SAFEFREE(aggregator);
        return rc;
    }

    buf = OBJ_NEW(opal_buffer_t);
    if (NULL == buf) {
        SAFEFREE (aggregator);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    for (node_index = 0; node_index < opal_argv_count(aggregator); node_index++) {

        xfer = OBJ_NEW(orte_rml_recv_cb_t);
        if (NULL == xfer) {
            SAFEFREE (aggregator);
            OBJ_RELEASE(buf);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        workflow_output_setup(xfer);

        OBJ_RETAIN(buf);

        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(aggregator[node_index],
                                                                   &wf_agg))) {
            workflow_process_error(buf, xfer, aggregator);
            return rc;
        }

        rc = workflow_list_pack_buffer(buf, xfer);
        if (ORCM_SUCCESS != rc) {
            workflow_process_error(buf, xfer, aggregator);
            return rc;
        }

        rc = workflow_send_buffer(&wf_agg, buf, xfer);
        if (ORCM_SUCCESS != rc) {
            workflow_process_error(buf, xfer, aggregator);
            return rc;
        }

        ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
        if (ORCM_SUCCESS != rc) {
            workflow_process_error(buf, xfer, aggregator);
            return rc;
        }

        rc = workflow_list_unpack_buffer(xfer);
        if (ORCM_SUCCESS != rc) {
            workflow_process_error(buf, xfer, aggregator);
            return rc;
        }

        OBJ_RELEASE(xfer);
    }
    workflow_process_error(buf, NULL, aggregator);
    return ORCM_SUCCESS;
}
