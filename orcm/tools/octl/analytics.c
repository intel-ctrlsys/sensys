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

#define OFLOW_SUCCESS   1
#define OFLOW_FAILURE   0

#define OFLOW_MAX_LINE_LENGTH  4096
#define OFLOW_MAX_ARRAY_SIZE   OFLOW_MAX_LINE_LENGTH

/******************
 * Local Functions
 ******************/
static int orcm_octl_wf_add_parse_line(FILE *fp, int *params_array_length,
                                       opal_value_t tokenized[]);
static void orcm_octl_analytics_process_error(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer, char **list);
static void orcm_octl_analytics_output_setup(orte_rml_recv_cb_t *xfer);
static int orcm_octl_analytics_wf_send_buffer(orte_process_name_t *wf_agg,
                                              opal_buffer_t *buf, orte_rml_recv_cb_t *xfer);
static int orcm_octl_analytics_wf_add_parse(FILE *fp, int *step_size, int *params_array_length,
                                             opal_value_t *input_array, char ***nodelist);
static int orcm_oct_analytics_wf_add_store(int params_array_length, opal_value_t *input_array,
                                           opal_value_t **workflow_params_array);
static void orcm_octl_analytics_wf_add_error(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer,
                                             opal_value_t *input_array, char **nodelist);
static int orcm_octl_analytics_wf_add_pack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer,
                                                  int step_size, int array_length,
                                                  opal_value_t **workflow_params_array);
static int orcm_octl_analytics_wf_add_unpack_buffer(orte_rml_recv_cb_t *xfer);
static int orcm_octl_analytics_wf_remove_parse_args(char **value, int *workflow_id, char ***nodelist);
static int orcm_octl_analytics_wf_remove_pack_buffer(opal_buffer_t *buf, int workflow_id,
                                                     orte_rml_recv_cb_t *xfer);
static int orcm_octl_analytics_wf_remove_unpack_buffer(orte_rml_recv_cb_t *xfer);
static int orcm_octl_analytics_wf_list_parse_args(char **value, char ***nodelist);
static int orcm_octl_analytics_wf_list_pack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer);
static int orcm_octl_analytics_wf_list_unpack_buffer(orte_rml_recv_cb_t *xfer);



static void orcm_octl_analytics_process_error(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer, char **list)
{
    if (NULL != buf) {
        OBJ_RELEASE(buf);
    }
    if (NULL != xfer) {
        OBJ_RELEASE(xfer);
    }

    free (list);

    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
}

static void orcm_octl_analytics_output_setup(orte_rml_recv_cb_t *xfer)
{
    /* setup to receive the result */
    xfer->active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_ANALYTICS,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, xfer);

}

static int orcm_octl_analytics_wf_send_buffer(orte_process_name_t *wf_agg,
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

static int orcm_octl_analytics_wf_add_parse(FILE *fp, int *step_size, int *params_array_length,
                                             opal_value_t *input_array, char ***nodelist)
{
    int set_nodelist = 0;
    int set_filter_plugin = 0;
    int rc;

    while ( OFLOW_FAILURE != orcm_octl_wf_add_parse_line(fp,
                             params_array_length, input_array + *params_array_length) ) {

        if (NULL == input_array[0].key) {
            orte_show_help("help-octl.txt", "octl:analytics:nodelist", true);
            return ORCM_ERROR;
        }

        if (0 == set_nodelist) {
            if (0 == strcmp("nodelist", input_array[0].key)) {
                /*This will reset the array index for next iteration.
                 * Resetting the array index will help in storing only the work steps */
                *params_array_length = 0;
                set_nodelist = 1;
                rc = orcm_logical_group_parse_array_string(input_array[0].data.string, nodelist);
                if (ORCM_SUCCESS != rc) {
                    return ORCM_ERROR;
                }
                continue;
            }
            else {
                orte_show_help("help-octl.txt", "octl:analytics:nodelist", true);
                return ORCM_ERROR;
            }
        }
        else if (0 == set_filter_plugin) {
            if (0 == strcmp("component", input_array[0].key)) {
                if (0 == strncmp("filter", input_array[0].data.string, 6)) {
                    set_filter_plugin = 1;
                    (*step_size)++;
                }
                else {
                    orte_show_help("help-octl.txt", "octl:analytics:filter", true);
                    return ORCM_ERROR;
                }
            }
            else {
                orte_show_help("help-octl.txt", "octl:analytics:filter", true);
                return ORCM_ERROR;
            }
        }
        else {
            (*step_size)++;
        }

        if (*params_array_length >= OFLOW_MAX_ARRAY_SIZE) {
            orte_show_help("help-octl.txt", "octl:analytics:big-file", true);
            return ORCM_ERROR;
        }
    }
    return ORCM_SUCCESS;

}

static int orcm_oct_analytics_wf_add_store(int params_array_length, opal_value_t *input_array,
                                           opal_value_t **workflow_params_array)
{
    int oflow_line_item;

    for (oflow_line_item=0; oflow_line_item < params_array_length ; oflow_line_item++) {

        if (params_array_length > OFLOW_MAX_ARRAY_SIZE) {
            orte_show_help("help-octl.txt", "octl:analytics:many-params",
                           true,  params_array_length);
            return ORCM_ERR_BAD_PARAM;
        }

        ORCM_UTIL_MSG("KEY: %s", input_array[oflow_line_item].key);
        ORCM_UTIL_MSG("VALUE: %s", input_array[oflow_line_item].data.string );
        workflow_params_array[oflow_line_item] = (opal_value_t *)(input_array + oflow_line_item);
   }
   return ORCM_SUCCESS;

}

static void orcm_octl_analytics_wf_add_error(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer,
                                             opal_value_t *input_array, char **nodelist)
{
    free (input_array);
    orcm_octl_analytics_process_error(buf, xfer, nodelist);
}

static int orcm_octl_analytics_wf_add_pack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer,
                                                  int step_size, int array_length,
                                                  opal_value_t **workflow_params_array)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_cmd_flag_t command;

    command = ORCM_ANALYTICS_WORKFLOW_CREATE;
    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, OPAL_UINT8))) {
        return ORCM_ERROR;
    }
    /* pack the length of the array */
    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &step_size, 1, OPAL_INT))) {
        return ORCM_ERROR;
    }
    if (array_length > 0) {
        if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, workflow_params_array,
                                                array_length, OPAL_VALUE))) {
            return ORCM_ERROR;
        }
    }
    else {
        rc = ORCM_ERROR;
    }
    return rc;
}

static int orcm_octl_analytics_wf_add_unpack_buffer(orte_rml_recv_cb_t *xfer)
{
    int n;
    int rc;
    int workflow_id;

    n=1;
    if (ORCM_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &workflow_id, &n, OPAL_INT))) {
        return ORCM_ERROR;
    }

    if (0 > workflow_id) {
        orte_show_help("help-octl.txt", "octl:analytics:workflow-failure",
                       true);
        rc = workflow_id;
        return rc;
    }
    ORCM_UTIL_MSG("Workflow created with id: %i", workflow_id);
    return ORCM_SUCCESS;

}


int orcm_octl_analytics_workflow_add(char *file)
{
    orte_rml_recv_cb_t *xfer = NULL;
    opal_buffer_t *buf = NULL;
    int rc;
    int params_array_length = 0;
    int step_size = 0;
    int node_index;
    FILE *fp;
    opal_value_t *oflow_input_file_array = NULL;
    opal_value_t *workflow_params_array[OFLOW_MAX_ARRAY_SIZE] ;
    orte_process_name_t wf_agg;
    char **nodelist = NULL;


    if (NULL == (fp = fopen(file, "r"))) {
        orte_show_help("help-octl.txt",
                       "octl:analytics:add-usage", true, "Can't open workflow file");
        return ORCM_ERR_BAD_PARAM;
    }

    oflow_input_file_array = (opal_value_t *)malloc(OFLOW_MAX_ARRAY_SIZE * sizeof(opal_value_t));

    if (NULL == oflow_input_file_array) {
        orte_show_help("help-octl.txt","octl:analytics:memory", true);
        fclose(fp);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    params_array_length = 0;

    rc = orcm_octl_analytics_wf_add_parse(fp, &step_size, &params_array_length,
                                          oflow_input_file_array, &nodelist);
    if ((ORCM_SUCCESS != rc) || (NULL == nodelist)) {
        free (oflow_input_file_array);
        free (nodelist);
        fclose(fp);
        return rc;
    }

    fclose(fp);

    rc = orcm_oct_analytics_wf_add_store(params_array_length, oflow_input_file_array,
                                         workflow_params_array);
    if (ORCM_SUCCESS != rc) {
        free (oflow_input_file_array);
        free (nodelist);
        return rc;
    }


    buf = OBJ_NEW(opal_buffer_t);
    if (NULL == buf) {
        free (oflow_input_file_array);
        free (nodelist);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    for (node_index = 0; node_index < opal_argv_count(nodelist); node_index++) {

        xfer = OBJ_NEW(orte_rml_recv_cb_t);
        if (NULL == xfer) {
            free (oflow_input_file_array);
            free (nodelist);
            OBJ_RELEASE(buf);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }

        orcm_octl_analytics_output_setup(xfer);

        OBJ_RETAIN(buf);

        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[node_index],
                                                                   &wf_agg))) {
            orte_show_help("help-octl.txt","octl:hostname-notfound",
                           true, nodelist[node_index]);
            orcm_octl_analytics_wf_add_error(buf, xfer,oflow_input_file_array, nodelist);
            return rc;
        }

        rc = orcm_octl_analytics_wf_add_pack_buffer(buf, xfer, step_size, params_array_length,
                                                workflow_params_array);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_analytics_wf_add_error(buf, xfer,oflow_input_file_array, nodelist);
            return rc;
        }


        rc = orcm_octl_analytics_wf_send_buffer(&wf_agg, buf, xfer);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_analytics_wf_add_error(buf, xfer,oflow_input_file_array, nodelist);
            return rc;
        }

        /* unpack workflow id */
        ORTE_WAIT_FOR_COMPLETION(xfer->active);

        rc = orcm_octl_analytics_wf_add_unpack_buffer(xfer);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_analytics_wf_add_error(buf, xfer,oflow_input_file_array, nodelist);
            return rc;
        }
        OBJ_RELEASE(xfer);
    }
    orcm_octl_analytics_wf_add_error(buf, NULL, oflow_input_file_array, nodelist);
    return ORCM_SUCCESS;

}

static int orcm_octl_analytics_wf_remove_parse_args(char **value, int *workflow_id, char ***nodelist)
{
    int rc;

    if (5 != opal_argv_count(value)) {
        orte_show_help("help-octl.txt",
                       "octl:analytics:remove-usage", true, "invalid arguments!");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL != value[3]) {
        rc = orcm_logical_group_parse_array_string(value[3], nodelist);
        if (ORCM_SUCCESS != rc) {
            return ORCM_ERROR;
        }

    }
    else {
        orte_show_help("help-octl.txt","octl:nodelist-null", true);
        return ORCM_ERR_BAD_PARAM;
    }

    if (0 != isdigit(value[4][strlen(value[4])-1])) {
        *workflow_id = (int)strtol(value[4], NULL, 10);
    }
    else {
        orte_show_help("help-octl.txt",
                       "octl:analytics:workflow-id", true, value[4]);
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;

}

static int orcm_octl_analytics_wf_remove_pack_buffer(opal_buffer_t *buf, int workflow_id,
                                                     orte_rml_recv_cb_t *xfer)
{
    orcm_analytics_cmd_flag_t command;
    int rc;

    command = ORCM_ANALYTICS_WORKFLOW_DELETE;
    /* pack the alloc command flag */
    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, OPAL_UINT8))) {
        return ORCM_ERROR;
    }
    /* pack the length of the array */
    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &workflow_id, 1, OPAL_INT))) {
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

static int orcm_octl_analytics_wf_remove_unpack_buffer(orte_rml_recv_cb_t *xfer)
{
    int n;
    int rc;
    int workflow_id;

    n=1;

    if (ORCM_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &workflow_id, &n, OPAL_INT))) {
        return ORCM_ERROR;
    }
    if (ORCM_ERROR != workflow_id) {
        ORCM_UTIL_MSG("Workflow deleted");
    }
    else {
        orte_show_help("help-octl.txt","octl:analytics:workflow-notfound", true);
    }
    return ORCM_SUCCESS;

}

int orcm_octl_analytics_workflow_remove(char **value)
{
    int workflow_id;
    orte_rml_recv_cb_t *xfer = NULL;
    opal_buffer_t *buf;
    orte_process_name_t wf_agg;
    char **nodelist=NULL;
    int node_index;
    int rc;

    rc = orcm_octl_analytics_wf_remove_parse_args(value, &workflow_id, &nodelist);
    if ((ORCM_SUCCESS != rc) ||(NULL == nodelist) ) {
        free(nodelist);
        return rc;
    }


    buf = OBJ_NEW(opal_buffer_t);
    if (NULL == buf) {
        free (nodelist);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    for (node_index = 0; node_index < opal_argv_count(nodelist); node_index++) {

        xfer = OBJ_NEW(orte_rml_recv_cb_t);
        if (NULL == xfer) {
            free (nodelist);
            OBJ_RELEASE(buf);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        orcm_octl_analytics_output_setup(xfer);

        OBJ_RETAIN(buf);

        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[node_index],
                                                                   &wf_agg))) {
            orcm_octl_analytics_process_error(buf, xfer, nodelist);
            return rc;
        }

        rc = orcm_octl_analytics_wf_remove_pack_buffer(buf, workflow_id, xfer);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_analytics_process_error(buf, xfer, nodelist);
            return rc;
        }

        rc = orcm_octl_analytics_wf_send_buffer(&wf_agg, buf, xfer);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_analytics_process_error(buf, xfer, nodelist);
            return rc;
        }

        ORTE_WAIT_FOR_COMPLETION(xfer->active);

        rc = orcm_octl_analytics_wf_remove_unpack_buffer(xfer);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_analytics_process_error(buf, xfer, nodelist);
            return rc;
        }

        OBJ_RELEASE(xfer);
    }
    orcm_octl_analytics_process_error(buf, NULL, nodelist);
    return ORCM_SUCCESS;

}

static int orcm_octl_analytics_wf_list_parse_args(char **value, char ***nodelist)
{
    int rc;

    if (4 != opal_argv_count(value)) {
        orte_show_help("help-octl.txt",
                       "octl:analytics:get-usage", true, "invalid arguments!");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL != value[3]) {
        rc = orcm_logical_group_parse_array_string(value[3], nodelist);
        if (ORCM_SUCCESS != rc) {
            return ORCM_ERROR;
        }

    }
    else {
        orte_show_help("help-octl.txt",
                       "octl:nodelist-null", true, "invalid arguments!");
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;
}

static int orcm_octl_analytics_wf_list_pack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer)
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

static int orcm_octl_analytics_wf_list_unpack_buffer(orte_rml_recv_cb_t *xfer)
{
    int cnt;
    int n;
    int *workflow_ids = NULL;
    int rc;
    int temp;

    n=1;

    if (ORCM_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &cnt, &n, OPAL_INT))) {
        return ORCM_ERROR;
    }

    if (0 < cnt) {
        workflow_ids = (int *)malloc(cnt * sizeof(int));
        if (NULL == workflow_ids) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        if (ORCM_SUCCESS != (rc = opal_dss.unpack(&xfer->data, workflow_ids, &cnt, OPAL_INT))) {
            free(workflow_ids);
            return ORCM_ERROR;
        }
        for (temp = 0; temp < cnt; temp++) {
            ORCM_UTIL_MSG("workflow id is: %d", workflow_ids[temp]);
        }
        free(workflow_ids);
    }
    else {
        ORCM_UTIL_MSG("No workflow ids");
    }
    return ORCM_SUCCESS;
}


int orcm_octl_analytics_workflow_list(char **value)
{
    orte_rml_recv_cb_t *xfer = NULL;
    opal_buffer_t *buf;
    orte_process_name_t wf_agg;
    char **nodelist=NULL;
    int node_index;
    int rc;

    rc = orcm_octl_analytics_wf_list_parse_args(value, &nodelist);
    if (ORCM_SUCCESS != rc) {
        free(nodelist);
        return rc;
    }

    buf = OBJ_NEW(opal_buffer_t);
    if (NULL == buf) {
        free (nodelist);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    for (node_index = 0; node_index < opal_argv_count(nodelist); node_index++) {

        xfer = OBJ_NEW(orte_rml_recv_cb_t);
        if (NULL == xfer) {
            free (nodelist);
            OBJ_RELEASE(buf);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        orcm_octl_analytics_output_setup(xfer);

        OBJ_RETAIN(buf);

        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[node_index],
                                                                   &wf_agg))) {
            orcm_octl_analytics_process_error(buf, xfer, nodelist);
            return rc;
        }

        rc = orcm_octl_analytics_wf_list_pack_buffer(buf, xfer);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_analytics_process_error(buf, xfer, nodelist);
            return rc;
        }

        rc = orcm_octl_analytics_wf_send_buffer(&wf_agg, buf, xfer);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_analytics_process_error(buf, xfer, nodelist);
            return rc;
        }

        ORTE_WAIT_FOR_COMPLETION(xfer->active);

        rc = orcm_octl_analytics_wf_list_unpack_buffer(xfer);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_analytics_process_error(buf, xfer, nodelist);
            return rc;
        }

        OBJ_RELEASE(xfer);
    }
    orcm_octl_analytics_process_error(buf, NULL, nodelist);
    return ORCM_SUCCESS;
}

/* get key/value from line */
static int orcm_octl_wf_add_parse_line(FILE *fp, int *params_array_length,
                                       opal_value_t tokenized[])
{
    char *ret, *ptr;
    char **tokens = NULL;
    char input[OFLOW_MAX_LINE_LENGTH];
    size_t i;
    int token_index;
    int token_array_length = 0;
    size_t input_str_length;

    ret = fgets(input, OFLOW_MAX_LINE_LENGTH, fp);
    if (NULL != ret) {
        input_str_length = strlen(input);
        input[input_str_length-1] = '\0';
        /* strip leading spaces */
        ptr = input;
        for (i=0; i < input_str_length-1; i++) {
            if (' ' != input[i]) {
                ptr = &input[i];
                break;
            }
        }
        tokens = opal_argv_split(ptr, ':');
        token_array_length = opal_argv_count(tokens);
        *params_array_length += token_array_length/2;

        if (*params_array_length >= OFLOW_MAX_ARRAY_SIZE) {
            opal_argv_free(tokens);
            return OFLOW_FAILURE;
        }

        for (token_index=0; token_index < token_array_length/2; token_index++) {

            tokenized[token_index].type = OPAL_STRING;
            if (NULL != tokens[(token_index*2)] ) {
                tokenized[token_index].key = strdup (tokens[(token_index*2)]);
            }
            if (NULL != tokens[(token_index*2) + 1] ) {
                tokenized[token_index].data.string = strdup (tokens[(token_index*2) + 1]);
            }
        }
        /*Analytics framework expects workstep to have 2 tokens. "Step name" and its "args
         *If user chose to ignore args. OCTL CLI tool has to append args for workstep  */
        if (0 != (*params_array_length)%2 ) {
            tokenized[token_index].type = OPAL_STRING;
            tokenized[token_index].key = strdup ("args");
            tokenized[token_index].data.string = NULL;
            *params_array_length = *params_array_length + 1;
        }

        /*opal_argv_free(tokens);*/
        opal_argv_free(tokens);
        return OFLOW_SUCCESS;
    }
    return OFLOW_FAILURE;
}
