/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "opal/dss/dss.h"

#include "orcm/mca/analytics/analytics_types.h"
#include "orcm/tools/octl/common.h"

#define OFLOW_SUCCESS   1
#define OFLOW_FAILURE   0

#define OFLOW_MAX_LINE_LENGTH  4096
#define OFLOW_MAX_ARRAY_SIZE   128

/******************
 * Local Functions
 ******************/
static int orcm_octl_wf_add_parse_line(FILE *fp, int *params_array_length,
                                       opal_value_t tokenized[]);
static void orcm_octl_analytics_process_error(int rc, opal_buffer_t *buf,
                                              orte_rml_recv_cb_t *xfer);
static void orcm_octl_analytics_output_setup(orte_rml_recv_cb_t *xfer);
static int orcm_octl_analytics_wf_send_buffer(orte_process_name_t *wf_agg,
                                              opal_buffer_t *buf, orte_rml_recv_cb_t *xfer);
static void orcm_octl_analytics_wf_add_parse(FILE *fp, int *step_size, int *params_array_length,
                                             opal_value_t *input_array, orte_process_name_t *wf_agg);
static int orcm_oct_analytics_wf_add_store(int params_array_length, opal_value_t *input_array,
                                           opal_value_t **workflow_params_array);
static int orcm_octl_analytics_wf_add_pack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer,
                                                  int step_size, int array_length,
                                                  opal_value_t **workflow_params_array);
static int orcm_octl_analytics_wf_add_unpack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer);
static int orcm_octl_analytics_wf_remove_parse_args(char **value, int *workflow_id,
                                                    orte_process_name_t *wf_agg);
static int orcm_octl_analytics_wf_remove_pack_buffer(opal_buffer_t *buf, int workflow_id,
                                                     orte_rml_recv_cb_t *xfer);
static int orcm_octl_analytics_wf_remove_unpack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer);
static int orcm_octl_analytics_wf_list_parse_args(char **value, orte_process_name_t *wf_agg);
static int orcm_octl_analytics_wf_list_pack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer);
static int orcm_octl_analytics_wf_list_unpack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer);



static void orcm_octl_analytics_process_error(int rc, opal_buffer_t *buf,
                                              orte_rml_recv_cb_t *xfer)
{
    ORTE_ERROR_LOG(rc);
    OBJ_RELEASE(buf);
    OBJ_DESTRUCT(xfer);
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
        orcm_octl_analytics_process_error(rc, buf, xfer);
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

static void orcm_octl_analytics_wf_add_parse(FILE *fp, int *step_size, int *params_array_length,
                                             opal_value_t *input_array, orte_process_name_t *wf_agg)
{
    while ( OFLOW_FAILURE != orcm_octl_wf_add_parse_line(fp,
                             params_array_length, input_array + *params_array_length) ) {

        if (0 == strncmp("VPID", input_array[0].key, OFLOW_MAX_LINE_LENGTH)) {
            wf_agg->jobid = 0;
            wf_agg->vpid = (orte_vpid_t)strtol(input_array[0].data.string,
                                               (char **)NULL, 10);
            printf("Sending to %s\n", ORTE_NAME_PRINT(wf_agg));
            *params_array_length = 0;
            continue;
        }
        else {
            (*step_size)++;
        }
    }

}

static int orcm_oct_analytics_wf_add_store(int params_array_length, opal_value_t *input_array,
                                           opal_value_t **workflow_params_array)
{
    int oflow_line_item;

    for (oflow_line_item=0; oflow_line_item < params_array_length ; oflow_line_item++) {

        if (params_array_length > OFLOW_MAX_ARRAY_SIZE) {
            printf("Too many params %d in the OFLOW file are being sent, OFLOW exiting",
                    params_array_length);
            return ORCM_ERR_BAD_PARAM;
        }

        printf("KEY: %s \n\tVALUE: %s\n", input_array[oflow_line_item].key,
                input_array[oflow_line_item].data.string );
        workflow_params_array[oflow_line_item] = (opal_value_t *)(input_array + oflow_line_item);
   }
   return ORCM_SUCCESS;

}

static int orcm_octl_analytics_wf_add_pack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer,
                                                  int step_size, int array_length,
                                                  opal_value_t **workflow_params_array)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_cmd_flag_t command;

    command = ORCM_ANALYTICS_WORKFLOW_CREATE;
    /* pack the alloc command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, OPAL_UINT8))) {
        orcm_octl_analytics_process_error(rc, buf, xfer);
        return ORCM_ERROR;
    }
    /* pack the length of the array */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &step_size, 1, OPAL_INT))) {
        orcm_octl_analytics_process_error(rc, buf, xfer);
        return ORCM_ERROR;
    }
    if (array_length > 0) {
        /* pack the array */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, workflow_params_array,
                                                array_length, OPAL_VALUE))) {
            orcm_octl_analytics_process_error(rc, buf, xfer);
            return ORCM_ERROR;
        }
    }
    else {
        orcm_octl_analytics_process_error(rc, buf, xfer);
        return ORCM_ERROR;
    }
    return rc;
}

static int orcm_octl_analytics_wf_add_unpack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer)
{
    int n;
    int rc;
    int workflow_id;

    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &workflow_id, &n, OPAL_INT))) {
        orcm_octl_analytics_process_error(rc, buf, xfer);
        return ORCM_ERROR;
    }
    printf("Workflow created with id: %i\n", workflow_id);
    return ORCM_SUCCESS;

}


int orcm_octl_analytics_workflow_add(char *file)
{
    orte_rml_recv_cb_t *xfer = NULL;
    opal_buffer_t *buf;
    int rc;
    int params_array_length = 0;
    int step_size = 0;
    FILE *fp;
    opal_value_t *oflow_input_file_array;
    opal_value_t *workflow_params_array[OFLOW_MAX_ARRAY_SIZE] ;
    orte_process_name_t wf_agg;


    if (NULL == (fp = fopen(file, "r"))) {
        perror("Can't open workflow file");
        return ORCM_ERR_BAD_PARAM;
    }
    
    oflow_input_file_array = (opal_value_t *)malloc(OFLOW_MAX_ARRAY_SIZE * sizeof(opal_value_t));

    params_array_length = 0;

    orcm_octl_analytics_wf_add_parse(fp, &step_size, &params_array_length,
                                     oflow_input_file_array, &wf_agg);

    fclose(fp);

    rc = orcm_oct_analytics_wf_add_store(params_array_length, oflow_input_file_array,
                                         workflow_params_array);
    if (ORCM_SUCCESS != rc) {
        free (oflow_input_file_array);
        return rc;
    }

    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    orcm_octl_analytics_output_setup(xfer);
    
    buf = OBJ_NEW(opal_buffer_t);

    rc = orcm_octl_analytics_wf_add_pack_buffer(buf, xfer, step_size, params_array_length,
                                                workflow_params_array);
    if (ORCM_SUCCESS != rc) {
        free (oflow_input_file_array);
        return rc;
    }


    rc = orcm_octl_analytics_wf_send_buffer(&wf_agg, buf, xfer);
    if (ORCM_SUCCESS != rc) {
        free (oflow_input_file_array);
        return rc;
    }

    /* unpack workflow id */
    ORTE_WAIT_FOR_COMPLETION(xfer->active);

    rc = orcm_octl_analytics_wf_add_unpack_buffer(buf, xfer);
    if (ORCM_SUCCESS != rc) {
        free (oflow_input_file_array);
        return rc;
    }

    OBJ_DESTRUCT(xfer);
    free (oflow_input_file_array);
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
    return ORTE_SUCCESS;
    
}

static int orcm_octl_analytics_wf_remove_parse_args(char **value, int *workflow_id,
                                                    orte_process_name_t *wf_agg)
{
    if (5 != opal_argv_count(value)) {
        fprintf(stderr, "incorrect arguments! \n usage: \"analytics workflow remove vpid workflow_id\"\n");
        return ORCM_ERR_BAD_PARAM;
    }

    if (isdigit(value[3][strlen(value[3])-1])) {
        wf_agg->jobid = 0;
        wf_agg->vpid = (orte_vpid_t)strtol(value[3], NULL, 10);
        printf("Sending to %s\n", ORTE_NAME_PRINT(wf_agg));
    }
    else {
        printf("incorrect argument VPID id!\n \"%s\" is not an integer \n", value[3]);
        return ORCM_ERR_BAD_PARAM;
    }

    if (isdigit(value[4][strlen(value[4])-1])) {
        *workflow_id = (int)strtol(value[4], NULL, 10);
    }
    else {
        printf("incorrect argument workflow id!\n \"%s\" is not an integer \n", value[4]);
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
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, OPAL_UINT8))) {
        orcm_octl_analytics_process_error(rc, buf, xfer);
        return ORCM_ERROR;
    }
    /* pack the length of the array */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &workflow_id, 1, OPAL_INT))) {
        orcm_octl_analytics_process_error(rc, buf, xfer);
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

static int orcm_octl_analytics_wf_remove_unpack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer)
{
    int n;
    int rc;
    int workflow_id;

    n=1;

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &workflow_id, &n, OPAL_INT))) {
        orcm_octl_analytics_process_error(rc, buf, xfer);
        return ORCM_ERROR;
    }
    if (ORCM_ERROR != workflow_id) {
        printf("Workflow deleted %d\n", workflow_id);
    }
    else {
        printf("workflow not found\n");
    }
    return ORCM_SUCCESS;

}

int orcm_octl_analytics_workflow_remove(char **value)
{
    int workflow_id;
    orte_rml_recv_cb_t *xfer = NULL;
    opal_buffer_t *buf;
    orte_process_name_t wf_agg;
    int rc;

    rc = orcm_octl_analytics_wf_remove_parse_args(value, &workflow_id, &wf_agg);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    orcm_octl_analytics_output_setup(xfer);

    buf = OBJ_NEW(opal_buffer_t);

    rc = orcm_octl_analytics_wf_remove_pack_buffer(buf, workflow_id, xfer);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    rc = orcm_octl_analytics_wf_send_buffer(&wf_agg, buf, xfer);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    ORTE_WAIT_FOR_COMPLETION(xfer->active);

    rc = orcm_octl_analytics_wf_remove_unpack_buffer(buf, xfer);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    OBJ_DESTRUCT(xfer);
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
    return ORTE_SUCCESS;

}

static int orcm_octl_analytics_wf_list_parse_args(char **value, orte_process_name_t *wf_agg)
{
    if (4 != opal_argv_count(value)) {
        fprintf(stderr, "incorrect arguments! \n usage: \"analytics workflow list vpid \"\n");
        return ORCM_ERR_BAD_PARAM;
    }

    if (isdigit(value[3][strlen(value[3])-1])) {
        wf_agg->jobid = 0;
        wf_agg->vpid = (orte_vpid_t)strtol(value[3], NULL, 10);
        printf("Sending to %s\n", ORTE_NAME_PRINT(wf_agg));
    }
    else {
        printf("incorrect argument VPID id!\n \"%s\" is not an integer \n", value[3]);
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
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, OPAL_UINT8))) {
        orcm_octl_analytics_process_error(rc, buf, xfer);
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

static int orcm_octl_analytics_wf_list_unpack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer)
{
    int cnt;
    int n;
    int *workflow_ids;
    int rc;
    int temp;

    n=1;

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &cnt, &n, OPAL_INT))) {
        orcm_octl_analytics_process_error(rc, buf, xfer);
        return ORCM_ERROR;
    }

    if (cnt > 0) {
        workflow_ids = (int *)malloc(cnt * sizeof(int));
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, workflow_ids, &cnt, OPAL_INT))) {
            orcm_octl_analytics_process_error(rc, buf, xfer);
            return ORCM_ERROR;
        }
        for (temp = 0; temp < cnt; temp++) {
            printf("workflow id is: %d\n", workflow_ids[temp] );
        }

    }
    else {
        printf("No workflow ids\n");
    }
    return ORCM_SUCCESS;
}


int orcm_octl_analytics_workflow_list(char **value)
{
    orte_rml_recv_cb_t *xfer = NULL;
    opal_buffer_t *buf;
    orte_process_name_t wf_agg;
    int rc;

    rc = orcm_octl_analytics_wf_list_parse_args(value, &wf_agg);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    orcm_octl_analytics_output_setup(xfer);

    buf = OBJ_NEW(opal_buffer_t);

    rc = orcm_octl_analytics_wf_list_pack_buffer(buf, xfer);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    rc = orcm_octl_analytics_wf_send_buffer(&wf_agg, buf, xfer);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    ORTE_WAIT_FOR_COMPLETION(xfer->active);

    rc = orcm_octl_analytics_wf_list_unpack_buffer(buf, xfer);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    OBJ_DESTRUCT(xfer);
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
    return ORTE_SUCCESS;
}

/* get key/value from line */
static int orcm_octl_wf_add_parse_line(FILE *fp, int *params_array_length,
                                       opal_value_t tokenized[])
{
    char *ret, *ptr;
    char **tokens = NULL;
    char input[OFLOW_MAX_LINE_LENGTH];
    size_t i;
    int token_index, token_array_length = 0;

    ret = fgets(input, OFLOW_MAX_LINE_LENGTH, fp);
    if (NULL != ret) {
        /* remove newline */
        input[strlen(input)-1] = '\0';
        /* strip leading spaces */
        ptr = input;
        for (i=0; i < strlen(input)-1; i++) {
            if (' ' != input[i]) {
                ptr = &input[i];
                break;
            }
        }
        tokens = opal_argv_split(ptr, ':');
        token_array_length = opal_argv_count(tokens);
        *params_array_length += token_array_length/2;

        if (*params_array_length > OFLOW_MAX_ARRAY_SIZE) {
            return OFLOW_FAILURE;
        }

        for (token_index=0; token_index < token_array_length/2;
                   token_index=token_index+1) {
            tokenized[token_index].type = OPAL_STRING;
            tokenized[token_index].key = strdup (tokens[(token_index*2)]);
            tokenized[token_index].data.string = strdup (tokens[(token_index*2) + 1]);
        }
        /*opal_argv_free(tokens);*/
        opal_argv_free(tokens);
        return OFLOW_SUCCESS;
    }
    return OFLOW_FAILURE;
}
