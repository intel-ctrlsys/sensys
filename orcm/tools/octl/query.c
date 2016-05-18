/*
 * Copyright (c) 2014-2016  Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "orte/mca/notifier/notifier.h"

#include "orcm/tools/octl/common.h"
#include "orcm/util/logical_group.h"
#include "orcm/mca/db/base/base.h"
#include "orcm/mca/db/db.h"
#include <regex.h>
#include <locale.h>

#define SAFE_FREE(x) if(NULL!=x) { free(x); x = NULL; }
/* Default idle time in seconds */
#define DEFAULT_IDLE_TIME "10"
#define MAX_STREAM_SIZE 10000

/* Macros to help with code coverage */
#define ON_FAILURE_RETURN(x) \
    if(ORCM_SUCCESS!=x){return x;}
#define ON_FAILURE_GOTO(x,label) \
    if(ORCM_SUCCESS!=x){ORTE_ERROR_LOG(x);goto label;}
#define ON_NULL_GOTO(x,label) \
    if(NULL==x){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);goto label;}
#define ON_NULL_RETURN(x,y) \
    if(NULL==x){ORTE_ERROR_LOG(y);return y;}

int query_db(int cmd, opal_list_t *filterlist, opal_list_t** results);
int query_db_stream(int cmd, opal_list_t *filterlist, uint32_t *results_count,
                    int *stream_index);
int create_buffer_from_filters(opal_buffer_t **buffer,
                               opal_list_t *filters_list,
                               orcm_rm_cmd_flag_t cmd);
int create_buffer_for_stream_request(opal_buffer_t **buffer_to_send,
                                     orcm_rm_cmd_flag_t cmd, int stream_index);
int get_nodes_from_args(char **argv, char ***node_list);
opal_list_t *create_query_sensor_filter(int argc, char **argv);
opal_list_t *create_query_idle_filter(int argc, char **argv);
opal_list_t *create_query_log_filter(int argc,char **argv);
opal_list_t *create_query_history_filter(int argc, char **argv);
opal_list_t *create_query_node_filter(int argc, char **argv);
opal_list_t *create_query_event_data_filter(int argc, char **argv);
opal_list_t *create_query_event_snsr_data_filter(int argc, char **argv);
opal_list_t *create_query_event_date_filter(int argc, char **argv);
int split_db_results(char *db_res, char ***db_results);
void free_db_results(int num_elements, char ***db_res_array);
char *add_to_str_date(char *date, int seconds);
void print_results(opal_list_t *results, double start_time, double stop_time);
int print_results_from_stream(uint32_t results_count,
                              int requested_buffer_index, double start_time,
                              double stop_time);
orcm_db_filter_t *create_string_filter(char *field, char *string,
                                       orcm_db_comparison_op_t op);

/* Helper functions */
opal_list_t *build_filters_list(int cmd,char **argv);
orcm_db_filter_t *build_node_item(char **expanded_node_list);
size_t list_nodes_str_size(char **expanded_node_list,int extra_bytes_per_element);
char *assemble_datetime(char *date_str,char *time_str);
double stopwatch(void);
bool replace_wildcard(char **filter_me, bool quit_on_first);

double stopwatch(void)
{
    double time_secs = 0.0;
    struct timeval now;
    gettimeofday(&now, (struct timezone*)0);
    time_secs = (double) (now.tv_sec +now.tv_usec*1.0e-6);
    return time_secs;
}

int query_db(int cmd, opal_list_t *filterlist, opal_list_t** results)
{
    int n = 1;
    int rc = -1;
    orcm_rm_cmd_flag_t command = (orcm_rm_cmd_flag_t)cmd;
    orcm_db_filter_t *tmp_filter = NULL;
    opal_buffer_t *buffer = OBJ_NEW(opal_buffer_t);
    orte_rml_recv_cb_t *xfer = NULL;
    uint16_t filterlist_count = 0;
    uint32_t results_count = 0;
    int returned_status = 0;

    if (NULL == filterlist || NULL == results){
        rc = ORCM_ERR_BAD_PARAM;
        goto query_db_cleanup;
    }
    filterlist_count = (uint16_t)opal_list_get_size(filterlist);
    rc = opal_dss.pack(buffer, &command, 1, ORCM_RM_CMD_T);
    ON_FAILURE_GOTO(rc, query_db_cleanup);
    rc = opal_dss.pack(buffer, &filterlist_count, 1, OPAL_UINT16);
    ON_FAILURE_GOTO(rc, query_db_cleanup);
    OPAL_LIST_FOREACH(tmp_filter, filterlist, orcm_db_filter_t) {
        uint8_t operation = (uint8_t)tmp_filter->op;
        rc = opal_dss.pack(buffer, &tmp_filter->value.key, 1, OPAL_STRING);
        ON_FAILURE_GOTO(rc, query_db_cleanup);
        rc = opal_dss.pack(buffer, &operation, 1, OPAL_UINT8);
        ON_FAILURE_GOTO(rc, query_db_cleanup);
        rc = opal_dss.pack(buffer, &tmp_filter->value.data.string, 1, OPAL_STRING);
        ON_FAILURE_GOTO(rc, query_db_cleanup);
    }
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    ON_NULL_GOTO(ORCM_ERR_OUT_OF_RESOURCE, query_db_cleanup);
    xfer->active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ORCMD_FETCH,
                            ORTE_RML_NON_PERSISTENT, orte_rml_recv_callback,
                            xfer);
    rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buffer,
                                 ORCM_RML_TAG_ORCMD_FETCH,
                                 orte_rml_send_callback, xfer);
    ON_FAILURE_GOTO(rc, query_db_cleanup);
    /* wait for status message */
    ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
    ON_FAILURE_GOTO(rc, query_db_cleanup);
    rc = opal_dss.unpack(&xfer->data, &returned_status, &n, OPAL_INT);
    ON_FAILURE_GOTO(rc, query_db_cleanup);
    if(0 == returned_status) {
        rc = opal_dss.unpack(&xfer->data, &results_count, &n, OPAL_UINT32);
        ON_FAILURE_GOTO(rc, query_db_cleanup);
        if (0 < results_count) {
        (*results) = OBJ_NEW(opal_list_t);
        for(uint32_t i = 0; i < results_count; ++i) {
            char* tmp_str = NULL;
            opal_value_t *tmp_value = NULL;
            n = 1;
            rc = opal_dss.unpack(&xfer->data, &tmp_str, &n, OPAL_STRING);
            ON_FAILURE_GOTO(rc, query_db_cleanup);
            tmp_value = OBJ_NEW(opal_value_t);
            tmp_value->type = OPAL_STRING;
            tmp_value->data.string = tmp_str;
            opal_list_append(*results, &tmp_value->super);
            }
       } else {
        orcm_octl_info("no-results");
        rc = ORCM_SUCCESS;
        *results = NULL;
     }
    } else {
        orcm_octl_error("orcmsched-fail");
        rc = ORCM_ERROR;
        *results = NULL;
    }
query_db_cleanup:
    SAFE_RELEASE(xfer);
    return rc;
}

/**
 * @brief Alternative version of query_db function which had a logic
 *        incompatible with a new protocol that retrieves an error code and the
 *        amount of rows from the DB. This version allows for that.
 * @param cmd is the command to send to the SCD framework, which serves to
 *            select the appropriate view in the DB framework.
 * @param filterslist is the memory address that contains a list with the
 *                    filters created by this tool based on the parameters
 *                    provided by the user at the command line interface.
 * @param results_count is the memory address where number of rows returned by
 *                      the SCD framework should be stored.
 * @param stream_index is the memory address where the stream index value
 *                     received from the SCD framework should be stored. This
 *                     stream index is used to retrieve the data which resulted
 *                     from our query to the SCD.
 * @return rc which is a code to verify that function completed successfully or
 *            to notify the occurrence on an error.
 */
int query_db_stream(int cmd, opal_list_t *filterlist, uint32_t *results_count,
                    int *stream_index)
{
    int rc = ORCM_SUCCESS;
    int n = 1;
    int returned_status = 0;
    orte_rml_recv_cb_t *xfer = NULL;
    opal_buffer_t *buffer = NULL;

    if (NULL == filterlist || NULL == results_count || NULL == stream_index) {
        rc = ORCM_ERR_BAD_PARAM;
        return rc;
    }
    rc = create_buffer_from_filters(&buffer, filterlist, cmd);
    ON_FAILURE_RETURN(rc);
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    ON_NULL_RETURN(xfer, ORCM_ERR_OUT_OF_RESOURCE);
    xfer->active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ORCMD_FETCH,
                            ORTE_RML_NON_PERSISTENT, orte_rml_recv_callback, xfer);
    rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buffer,
                                 ORCM_RML_TAG_ORCMD_FETCH,
                                 orte_rml_send_callback, xfer);
    ON_FAILURE_RETURN(rc);
    ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT_STREAM, &rc);
    ON_FAILURE_RETURN(rc);
    rc = opal_dss.unpack(&xfer->data, &returned_status, &n, OPAL_INT);
    ON_FAILURE_RETURN(rc);
    rc = opal_dss.unpack(&xfer->data, results_count, &n, OPAL_UINT32);
    ON_FAILURE_RETURN(rc);
    rc = opal_dss.unpack(&xfer->data, stream_index, &n, OPAL_INT);
    ON_FAILURE_RETURN(rc);
    SAFE_RELEASE(xfer);

    return rc;
}

int create_buffer_for_stream_request(opal_buffer_t **buffer_to_send,
                                     orcm_rm_cmd_flag_t cmd, int stream_index)
{
    int rc = ORCM_SUCCESS;
    opal_buffer_t *tmp_buffer = NULL;
    orcm_rm_cmd_flag_t command = cmd;
    int requested_stream = stream_index;

    tmp_buffer = OBJ_NEW(opal_buffer_t);
    ON_NULL_RETURN(tmp_buffer, ORCM_ERR_OUT_OF_RESOURCE);
    rc = opal_dss.pack(tmp_buffer, &command, 1, ORCM_RM_CMD_T);
    ON_FAILURE_GOTO(rc, create_buffer_for_stream_cleanup);
    rc = opal_dss.pack(tmp_buffer, &requested_stream, 1, OPAL_INT);
    ON_FAILURE_GOTO(rc, create_buffer_for_stream_cleanup);
    *buffer_to_send = tmp_buffer;

    return rc;

create_buffer_for_stream_cleanup:
    OBJ_RELEASE(tmp_buffer);

    return rc;
}

int create_buffer_from_filters(opal_buffer_t **buffer_to_send,
                               opal_list_t *filters_list,
                               orcm_rm_cmd_flag_t cmd)
{
    int rc = ORCM_SUCCESS;
    opal_buffer_t *tmp_buffer = NULL;
    uint16_t filters_list_size = 0;
    orcm_rm_cmd_flag_t command = cmd;
    orcm_db_filter_t *tmp_filter = NULL;

    if (NULL == filters_list){
        rc = OPAL_ERR_BAD_PARAM;
        return rc;
    }
    tmp_buffer = OBJ_NEW(opal_buffer_t);
    ON_NULL_RETURN(tmp_buffer, ORCM_ERR_OUT_OF_RESOURCE);
    rc = opal_dss.pack(tmp_buffer, &command, 1, ORCM_RM_CMD_T);
    ON_FAILURE_GOTO(rc, create_buffer_from_filters_cleanup);
    filters_list_size = (uint16_t)opal_list_get_size(filters_list);
    if (2 > filters_list_size) {
        rc = OPAL_ERR_BAD_PARAM;
        goto create_buffer_from_filters_cleanup;
    }
    rc = opal_dss.pack(tmp_buffer, &filters_list_size, 1, OPAL_UINT16);
    ON_FAILURE_GOTO(rc, create_buffer_from_filters_cleanup);
    OPAL_LIST_FOREACH(tmp_filter, filters_list, orcm_db_filter_t) {
        uint8_t operation = (uint8_t)tmp_filter->op;
        if (0 != strlen(tmp_filter->value.key)) {
            rc = opal_dss.pack(tmp_buffer, &tmp_filter->value.key, 1,
                               OPAL_STRING);
            ON_FAILURE_GOTO(rc, create_buffer_from_filters_cleanup);
        } else {
            rc = OPAL_ERR_BAD_PARAM;
            goto create_buffer_from_filters_cleanup;
        }
        rc = opal_dss.pack(tmp_buffer, &operation, 1, OPAL_UINT8);
        ON_FAILURE_GOTO(rc, create_buffer_from_filters_cleanup);
        if (tmp_filter->value.type == OPAL_STRING){
            rc = opal_dss.pack(tmp_buffer, &tmp_filter->value.data.string, 1,
                               OPAL_STRING);
            ON_FAILURE_GOTO(rc, create_buffer_from_filters_cleanup);
        } else {
            rc = OPAL_ERR_BAD_PARAM;
            goto create_buffer_from_filters_cleanup;
        }
    }
    *buffer_to_send = tmp_buffer;

    return rc;

create_buffer_from_filters_cleanup:
    OBJ_RELEASE(tmp_buffer);

    return rc;
}

int get_nodes_from_args(char **argv, char ***node_list)
{
    int rc = ORCM_SUCCESS;
    int arg_index = 0;
    int node_count = 0;
    char *raw_node_list = NULL;
    char **argv_node_list = NULL;

    if (NULL == *argv){
        orcm_octl_error("no-args");
        return ORCM_ERR_BAD_PARAM;
    }
    arg_index = opal_argv_count(argv);
    /* Convert argv count to index */
    arg_index--;
    if (2 > arg_index){
        orcm_octl_error("nodelist-null");
        return ORCM_ERR_BAD_PARAM;
    }
    raw_node_list = argv[arg_index];
    if (NULL == raw_node_list ) {
        orcm_octl_error("nodelist-null");
        return ORCM_ERR_BAD_PARAM;
    }
    rc = orcm_logical_group_parse_array_string(raw_node_list, &argv_node_list);
    node_count = opal_argv_count(argv_node_list);
    if (0 == node_count) {
        orcm_octl_error("nodelist-extract", raw_node_list);
        opal_argv_free(argv_node_list);
        return ORCM_ERR_BAD_PARAM;
    }
    *node_list = argv_node_list;
    return rc;
}

bool replace_wildcard(char **filter_me, bool quit_on_first)
{
    /* In-place replacement of first or all wildcards found in the filter_me string */
    char *temp_filter = NULL;
    bool found_wildcard = false;

    if (filter_me == NULL || *filter_me == NULL) {
        return false;
    }

    temp_filter = *filter_me;
    for(size_t i = 0; i < strlen(temp_filter); ++i) {
        if('*' == temp_filter[i]) {
                found_wildcard = true;
                temp_filter[i] = '%';
                /* Once we have found a wildcard ignore the rest of the string */
                if (true == quit_on_first){
                    temp_filter[i+1] = '\0';
                    return true;
                }
        }
    }
    return found_wildcard;
}

char *assemble_datetime(char *date_str, char *time_str)
{
    char *date_time_str = NULL;
    char *new_date_str = NULL;
    char *new_time_str = NULL;
    size_t date_time_length = 0;

    if (NULL == date_str || NULL == time_str) {
        return NULL;
    }

    if (NULL != date_str){
        date_time_length = strlen(date_str);
        new_date_str = strdup(date_str);
    }
    if (NULL != time_str){
        date_time_length += strlen(time_str);
        new_time_str = strdup(time_str);
    }
    if (0 < date_time_length && NULL != (date_time_str = calloc(sizeof(char),
                                                                date_time_length))) {
        if (false == replace_wildcard(&new_date_str, true)){
            if (NULL != new_date_str){
                strncpy(date_time_str, new_date_str, date_time_length - 1);
                date_time_str[date_time_length - 1] = '\0';
                strncat(date_time_str, " ", strlen(" "));
                if (NULL != new_time_str){
                    if (false == replace_wildcard(&new_time_str, true)){
                        strncat(date_time_str, new_time_str, strlen(new_time_str));
                    }
                }
            } else {
               orcm_octl_error("allocate-memory", "datetime string");
            }
        } else {
            SAFE_FREE(date_time_str);
        }
    }
    SAFE_FREE(new_date_str);
    SAFE_FREE(new_time_str);
    return date_time_str;
}

orcm_db_filter_t *create_string_filter(char *field, char *string,
                                    orcm_db_comparison_op_t op){
    orcm_db_filter_t *filter = NULL;
    if (NULL != field && NULL != string) {
        filter = OBJ_NEW(orcm_db_filter_t);
        if (NULL != filter) {
            filter->value.type = OPAL_STRING;
            filter->value.key = strdup(field);
            filter->value.data.string = strdup(string);
            filter->op = op;
        }
    }
    return filter;
}

opal_list_t *create_query_idle_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;

    filters_list = OBJ_NEW(opal_list_t);
    if (3 == argc) {
        filter_item = create_string_filter("idle_time", DEFAULT_IDLE_TIME, GT);
        opal_list_append(filters_list, &filter_item->value.super);
    } else if (4 == argc) {
        filter_item = create_string_filter("idle_time", argv[2], GT);
        opal_list_append(filters_list, &filter_item->value.super);
    } else {
        orcm_octl_usage("query-idle", INVALID_USG);
        return NULL;
    }
    return filters_list;
}

opal_list_t *create_query_log_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    char *date_time_str = NULL;
    char *filter_str = NULL;

    filters_list = OBJ_NEW(opal_list_t);
    if (3 == argc) {
    /* There's no need to create a filter */
        NULL;
    } else if (4 == argc){
        /* Add 3 more chars including the end of the string '\0' */
        if (NULL != (filter_str = calloc(sizeof(char), strlen(argv[2])+3))){
            strncat(filter_str, "%", strlen("%"));
            strncat(filter_str, argv[2], strlen(argv[2]));
            strncat(filter_str, "%", strlen("%"));
        } else {
            orcm_octl_error("allocate-memory", "filter string");
            return NULL;
        }
        filter_item = create_string_filter("log", filter_str, CONTAINS);
        opal_list_append(filters_list, &filter_item->value.super);
        SAFE_FREE(filter_str);
    } else if (7 == argc){
        /* Create filter for start time if necessary */
        if (NULL != (date_time_str = assemble_datetime(argv[2], argv[3]))) {
            filter_item = create_string_filter("time_stamp", date_time_str, GT);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFE_FREE(date_time_str);
        }
        /* Create filter for end time if necessary */
        if (NULL != (date_time_str = assemble_datetime(argv[4], argv[5]))) {
            filter_item = create_string_filter("time_stamp", date_time_str, LT);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFE_FREE(date_time_str);
        }
    } else if (8 == argc){
        /* Add 3 more chars including the end of the string '\0' */
        if (NULL != (filter_str = calloc(sizeof(char), strlen(argv[2])+3))){
            strncat(filter_str, "%", strlen("%"));
            strncat(filter_str, argv[2], strlen(argv[2]));
            strncat(filter_str, "%", strlen("%"));
        } else {
            orcm_octl_error("allocate-memory", "filter string");
            return NULL;
        }
        filter_item = create_string_filter("log", filter_str, CONTAINS);
        opal_list_append(filters_list, &filter_item->value.super);
        SAFE_FREE(filter_str);
        /* Create filter for start time if necessary */
        if (NULL != (date_time_str = assemble_datetime(argv[3], argv[4]))) {
            filter_item = create_string_filter("time_stamp", date_time_str, GT);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFE_FREE(date_time_str);
        }
        /* Create filter for end time if necessary */
        if (NULL != (date_time_str = assemble_datetime(argv[5], argv[6]))) {
            filter_item = create_string_filter("time_stamp", date_time_str, LT);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFE_FREE(date_time_str);
        }
    } else {
        orcm_octl_usage("query-log", INVALID_USG);
        return NULL;
    }
    return filters_list;
}

opal_list_t *create_query_node_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;

    filters_list = OBJ_NEW(opal_list_t);

    if (4 == argc) {
    /* Place holder for functionality when available in the DB */
        NULL;
    } else {
        orcm_octl_usage("query-node-status", INVALID_USG);
        return NULL;
    }
    return filters_list;
}

opal_list_t *create_query_history_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    char *date_time_str = NULL;

    filters_list = OBJ_NEW(opal_list_t);
    if (3 == argc) {
        filter_item = create_string_filter("data_item", "%", CONTAINS);
        opal_list_append(filters_list, &filter_item->value.super);
    } else if (7 == argc){
        /* Create filter for start time if necessary */
        if (NULL != (date_time_str = assemble_datetime(argv[2],argv[3]))) {
            filter_item = create_string_filter("time_stamp", date_time_str, GT);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFE_FREE(date_time_str);
        }
        /* Create filter for end time if necessary */
        if (NULL != (date_time_str = assemble_datetime(argv[4], argv[5]))) {
            filter_item = create_string_filter("time_stamp", date_time_str, LT);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFE_FREE(date_time_str);
        }
    } else {
        orcm_octl_usage("query-history", INVALID_USG);
        return NULL;
    }
    return filters_list;
}

opal_list_t *create_query_sensor_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    char *date_time_str = NULL;
    char *filter_str = NULL;

    filters_list = OBJ_NEW(opal_list_t);
    if (4 == argc){
            /* Create a filter for a sensor */
            filter_str = strdup(argv[2]);
            replace_wildcard(&filter_str, false);
            filter_item = create_string_filter("data_item", filter_str, CONTAINS);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFE_FREE(filter_str);
    } else if (8 == argc){
            /* Create a filter for a sensor */
            filter_str = strdup(argv[2]);
            replace_wildcard(&filter_str, false);
            filter_item = create_string_filter("data_item", filter_str, CONTAINS);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFE_FREE(filter_str);
            /* Create filter for start time if necessary */
            if (NULL != (date_time_str = assemble_datetime(argv[3], argv[4]))) {
                filter_item = create_string_filter("time_stamp", date_time_str, GT);
                opal_list_append(filters_list, &filter_item->value.super);
                SAFE_FREE(date_time_str);
            }
            /* Create filter for end time if necessary */
            if (NULL != (date_time_str = assemble_datetime(argv[5], argv[6]))) {
                filter_item = create_string_filter("time_stamp", date_time_str, LT);
                opal_list_append(filters_list, &filter_item->value.super);
                SAFE_FREE(date_time_str);
            }
    } else if (10 == argc){
            /* Create a filter for a sensor */
            filter_str = strdup(argv[2]);
            replace_wildcard(&filter_str, false);
            filter_item = create_string_filter("data_item", filter_str, CONTAINS);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFE_FREE(filter_str);
            /* Create filter for start time if necessary */
            if (NULL != (date_time_str = assemble_datetime(argv[3], argv[4]))) {
                filter_item = create_string_filter("time_stamp", date_time_str, GT);
                opal_list_append(filters_list, &filter_item->value.super);
                SAFE_FREE(date_time_str);
            }
            /* Create filter for end time if necessary */
            if (NULL != (date_time_str = assemble_datetime(argv[5], argv[6]))) {
                filter_item = create_string_filter("time_stamp", date_time_str, LT);
                opal_list_append(filters_list, &filter_item->value.super);
                SAFE_FREE(date_time_str);
            }
            filter_str = strdup(argv[7]);
            /* Create filter for upper bound necessary */
            if (false == replace_wildcard(&filter_str, true)){
                filter_item = create_string_filter("value_str", filter_str, GT);
                opal_list_append(filters_list, &filter_item->value.super);
            }
            SAFE_FREE(filter_str);
            filter_str = strdup(argv[8]);
            /* Create filter for upper bound necessary */
            if (false == replace_wildcard(&filter_str, true)){
                filter_item = create_string_filter("value_str", filter_str, LT);
                opal_list_append(filters_list, &filter_item->value.super);
            }
            SAFE_FREE(filter_str);
    } else {
        orcm_octl_usage("query-sensor", INVALID_USG);
        return NULL;
    }
    return filters_list;
}

opal_list_t *create_query_event_data_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    char *filter_str = NULL;

    if (NULL == argv) {
        return NULL;
    }

    filters_list = OBJ_NEW(opal_list_t);
    if (4 == argc) {
        time_t current_time;
        struct tm *localdate;
        char current_date[12];
        time(&current_time);
        localdate = localtime(&current_time);
        if (NULL == localdate) {
            orcm_octl_error("allocate-memory", "datatime string");
            return NULL;
        }
        strftime(current_date, sizeof(current_date), "%Y-%m-%d", localdate);
        filter_item = create_string_filter("time_stamp", current_date, GT);
        opal_list_append(filters_list, &filter_item->value.super);
        filter_item = create_string_filter("severity", "INFO", NE);
        opal_list_append(filters_list, &filter_item->value.super);
    } else if (8 == argc) {
        /* Doing nothing here, we want to retrieve all the DB data */
        if (NULL != (filter_str = assemble_datetime(argv[3], argv[4]))){
            filter_item = create_string_filter("time_stamp", filter_str, GT);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFEFREE(filter_str);
        }
        if (NULL != (filter_str = assemble_datetime(argv[5], argv[6]))){
            filter_item = create_string_filter("time_stamp", filter_str, LT);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFEFREE(filter_str);
        }
        /* We create a fixed filter to get all events different from INFO
         * which are less important for the user.
         */
        filter_item = create_string_filter("severity", "INFO", NE);
        opal_list_append(filters_list, &filter_item->value.super);
    } else {
        orcm_octl_usage("query-event-data", INVALID_USG);
        SAFEFREE(filters_list);
        return NULL;
    }
    return filters_list;
}

/**
 * @brief Function that creates the data filters (WHERE clause on
 *        normal SQL queries) for the following report:
 *        "Sensor data around N minutes before/after an event".
 *        Each filter is equivalent to a single comparison on the
 *        WHERE clause.
 *
 * @param argc Number of arguments that are present in the command.
 *             The command words are also counted as arguments. e.g.
 *             > query event sensor-data 1 after 1 coretemp* node_x
 *             Has 8 arguments (from 0 to 7).
 *
 * @param argv Array of strings. Each array position contains an
 *             argument of the command as a string.
 */
opal_list_t *create_query_event_snsr_data_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    char *filter_str = NULL;
    char *end_date;

    if (argv == NULL) {
        return NULL;
    }

    if (8 == argc) {
        filters_list = OBJ_NEW(opal_list_t);

        if ( 0 == strcmp("before", argv[4]) ){
            filter_item = create_string_filter("time_stamp", argv[3], LE);
            opal_list_append(filters_list, &filter_item->value.super);

            end_date = add_to_str_date(argv[3], 0 - (atoi(argv[5]) * 60) );
            filter_item = create_string_filter("time_stamp", end_date, GE);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFE_FREE(end_date);
        } else if ( 0 == strcmp("after", argv[4]) ){
            filter_item = create_string_filter("time_stamp", argv[3], GE);
            opal_list_append(filters_list, &filter_item->value.super);

            end_date = add_to_str_date(argv[3], (atoi(argv[5]) * 60) );
            filter_item = create_string_filter("time_stamp", end_date, LE);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFE_FREE(end_date);
        } else {
            SAFE_RELEASE(filters_list);
            filters_list = NULL;
        }

        if( NULL != filters_list ){
            filter_str = strdup(argv[6]);
            replace_wildcard(&filter_str, false);
            filter_item = create_string_filter("data_item", filter_str, CONTAINS);
            opal_list_append(filters_list, &filter_item->value.super);
            SAFE_FREE(filter_str);
        }
    } else {
        orcm_octl_usage("query-event-sensor-data", INVALID_USG);
    }

    return filters_list;
}

/**
 * @brief Function that creates the data filters (WHERE clause on
 *        normal SQL queries) for the following report:
 *        "Obtain date of an event".
 *        This is not a formal report, instead it's used as a support
 *        report to be able to query the "Sensor data around N minutes
 *        before/after an event" report.
 *        Each filter is equivalent to a single comparison on the
 *        WHERE clause.
 *
 * @param argc Number of arguments that are present in the command.
 *             The command words are also counted as arguments. e.g.
 *             > query event sensor-data 1 after 1 coretemp* node_x
 *             Has 8 arguments (from 0 to 7).
 *
 * @param argv Array of strings. Each array position contains an
 *             argument of the command as a string.
 */
opal_list_t *create_query_event_date_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;

    if( 3 < argc && NULL != argv){
        filters_list = OBJ_NEW(opal_list_t);
        filter_item = create_string_filter("event_id", argv[3], EQ);
        opal_list_append(filters_list, &filter_item->value.super);
    }
    return filters_list;
}

opal_list_t *build_filters_list(int cmd, char **argv)
{
    opal_list_t *filters_list = NULL;
    char argc = 0;

    argc = opal_argv_count(argv);
    switch(cmd){
        case ORCM_GET_DB_QUERY_NODE_COMMAND:
            filters_list = create_query_node_filter(argc, argv);
            break;
        case ORCM_GET_DB_QUERY_IDLE_COMMAND:
            filters_list = create_query_idle_filter(argc, argv);
            break;
        case ORCM_GET_DB_QUERY_LOG_COMMAND:
            filters_list = create_query_log_filter(argc, argv);
            break;
        case ORCM_GET_DB_QUERY_HISTORY_COMMAND:
            filters_list = create_query_history_filter(argc, argv);
            break;
        case ORCM_GET_DB_QUERY_SENSOR_COMMAND:
            filters_list = create_query_sensor_filter(argc, argv);
            break;
        case ORCM_GET_DB_QUERY_EVENT_DATA_COMMAND:
            filters_list = create_query_event_data_filter(argc, argv);
            break;
        case ORCM_GET_DB_QUERY_EVENT_DATE_COMMAND:
            filters_list = create_query_event_date_filter(argc, argv);
            break;
        case ORCM_GET_DB_QUERY_EVENT_SNSR_DATA_COMMAND:
            filters_list = create_query_event_snsr_data_filter(argc, argv);
            break;
        default:
            break;
    }
    return filters_list;
}

size_t list_nodes_str_size(char **expanded_node_list, int extra_bytes_per_element)
{
    size_t str_length = 0;
    int node_count = 0;

    node_count = opal_argv_count(expanded_node_list);
    for(int i = 0; i < node_count; ++i) {
        str_length += strlen(expanded_node_list[i])+extra_bytes_per_element;
    }
    return str_length;
}

orcm_db_filter_t *build_node_item(char **expanded_node_list)
{
    int node_count = 0;
    size_t str_length = 0;
    char *hosts_filter = NULL;
    orcm_db_filter_t *filter_item = NULL;

    str_length = list_nodes_str_size(expanded_node_list, 3);
    /* Add one character for '\0' */
    str_length++;
    if (NULL != (hosts_filter = calloc(sizeof(char), str_length))){
        node_count = opal_argv_count(expanded_node_list);
        for(int i=0; i < node_count; ++i){
            strncat(hosts_filter, "'", strlen("'"));
            strncat(hosts_filter, expanded_node_list[i], strlen(expanded_node_list[i]));
            strncat(hosts_filter, "'", strlen("'"));
            strncat(hosts_filter, ",", strlen(","));
        }
        /* Remove trailing comma */
        hosts_filter[strlen(hosts_filter)-1]= '\0';
        /* Operator for query depends on whether we find a wildcard in the hostname arg */
        if (true == replace_wildcard(&hosts_filter, true)){
            filter_item = create_string_filter("hostname", "%", CONTAINS);
        } else {
            filter_item = create_string_filter("hostname", hosts_filter, IN);
        }
        SAFE_FREE(hosts_filter);
    } else {
        orcm_octl_error("allocate-memory", "node item");
    }
    return filter_item;
}

int orcm_octl_query_sensor(int cmd, char **argv)
{
    int rc = ORCM_SUCCESS;
    char **argv_node_list = NULL;
    double start_time = 0.0;
    double stop_time = 0.0;
    opal_list_t *filter_list = NULL;
    opal_list_t *returned_list = NULL;
    orcm_db_filter_t *nodes_list = NULL;
    uint32_t results_count = 0;
    int stream_from_query = 0;

    /* Build input node list */
    filter_list = build_filters_list(cmd, argv);
    if (NULL == filter_list){
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_sensor_cleanup;
    }
    if (ORCM_SUCCESS != get_nodes_from_args(argv, &argv_node_list)){
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_sensor_cleanup;
    }
    if (NULL != (nodes_list = build_node_item(argv_node_list))){
        opal_list_append(filter_list, &nodes_list->value.super);
    }
    /* Get list of results from scheduler (or other management node) */
    start_time = stopwatch();
    rc = query_db_stream(cmd, filter_list, &results_count, &stream_from_query);
    stop_time = stopwatch();
    if(rc != ORCM_SUCCESS) {
        orcm_octl_info("no-results");
    } else {
        print_results_from_stream(results_count, stream_from_query, start_time,
                                  stop_time);
    }
    SAFE_RELEASE(returned_list);
    SAFE_RELEASE(filter_list);
orcm_octl_query_sensor_cleanup:
    opal_argv_free(argv_node_list);
    return rc;
}

int orcm_octl_query_log(int cmd, char **argv)
{
    int rc = ORCM_SUCCESS;
    char **argv_node_list = NULL;
    double start_time = 0.0;
    double stop_time = 0.0;
    opal_list_t *filter_list = NULL;
    opal_list_t *returned_list = NULL;
    orcm_db_filter_t *nodes_list = NULL;

    /* Build input node list */
    filter_list = build_filters_list(cmd, argv);
    if (NULL == filter_list){
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_log_cleanup;
    }
    if (ORCM_SUCCESS != get_nodes_from_args(argv, &argv_node_list)){
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_log_cleanup;
    }
    if (NULL != (nodes_list = build_node_item(argv_node_list))){
        opal_list_append(filter_list, &nodes_list->value.super);
    }
    /* Get list of results from scheduler (or other management node) */
    start_time = stopwatch();
    rc = query_db(cmd, filter_list, &returned_list);
    stop_time = stopwatch();
    if(rc != ORCM_SUCCESS) {
        orcm_octl_info("no-results");
    } else {
        print_results(returned_list, start_time, stop_time);
    }
    SAFE_RELEASE(returned_list);
    SAFE_RELEASE(filter_list);
orcm_octl_query_log_cleanup:
    opal_argv_free(argv_node_list);
    return rc;
}

int orcm_octl_query_idle(int cmd, char **argv)
{
    int rc = ORCM_SUCCESS;
    char **argv_node_list = NULL;
    double start_time = 0.0;
    double stop_time = 0.0;
    opal_list_t *filter_list = NULL;
    opal_list_t *returned_list = NULL;
    orcm_db_filter_t *nodes_list = NULL;

    /* Build input node list */
    filter_list = build_filters_list(cmd, argv);
    if (NULL == filter_list){
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_idle_cleanup;
    }
    if (ORCM_SUCCESS != get_nodes_from_args(argv, &argv_node_list)){
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_idle_cleanup;
    }
    if (NULL != (nodes_list = build_node_item(argv_node_list))){
        opal_list_append(filter_list, &nodes_list->value.super);
    }
    /* Get list of results from scheduler (or other management node) */
    start_time = stopwatch();
    rc = query_db(cmd, filter_list, &returned_list);
    stop_time = stopwatch();
    if(rc != ORCM_SUCCESS) {
        orcm_octl_info("no-results");
    } else {
        print_results(returned_list, start_time, stop_time);
    }
    SAFE_RELEASE(returned_list);
    SAFE_RELEASE(filter_list);
orcm_octl_query_idle_cleanup:
    opal_argv_free(argv_node_list);
    return rc;
}

int orcm_octl_query_node(int cmd, char **argv)
{
    int rc = ORCM_SUCCESS;
    char **argv_node_list = NULL;
    double start_time = 0.0;
    double stop_time = 0.0;
    opal_list_t *filter_list = NULL;
    opal_list_t *returned_list = NULL;
    orcm_db_filter_t *nodes_list = NULL;

    /* Build input node list */
    filter_list = build_filters_list(cmd, argv);
    if (NULL == filter_list){
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_node_cleanup;
    }
    if (ORCM_SUCCESS != (rc = get_nodes_from_args(argv, &argv_node_list))){
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_node_cleanup;
    }
    if (NULL != (nodes_list = build_node_item(argv_node_list))){
        opal_list_append(filter_list, &nodes_list->value.super);
    }
    /* Get list of results from scheduler (or other management node) */
    start_time = stopwatch();
    rc = query_db(cmd, filter_list, &returned_list);
    stop_time = stopwatch();
    if(rc != ORCM_SUCCESS) {
        orcm_octl_info("no-results");
    } else {
        print_results(returned_list, start_time, stop_time);
    }
    SAFE_RELEASE(returned_list);
    SAFE_RELEASE(filter_list);
orcm_octl_query_node_cleanup:
    opal_argv_free(argv_node_list);
    return rc;
}

int orcm_octl_query_event_data(int cmd, char **argv)
{
    int rc = ORCM_SUCCESS;
    double start_time = 0.0;
    double stop_time = 0.0;
    char **argv_node_list = NULL;
    opal_list_t *filter_list = NULL;
    opal_list_t *returned_list = NULL;
    orcm_db_filter_t *node_list = NULL;

    filter_list = build_filters_list(cmd, argv);
    if (NULL == filter_list) {
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_event_exit;
    }
    rc = get_nodes_from_args(argv, &argv_node_list);
    if (ORCM_SUCCESS != rc) {
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_event_exit;
    }
    node_list = build_node_item(argv_node_list);
    if (NULL != node_list) {
        opal_list_append(filter_list, &node_list->value.super);
    }

    start_time = stopwatch();
    rc = query_db(cmd, filter_list, &returned_list);
    stop_time = stopwatch();
    if (ORCM_SUCCESS != rc) {
        orcm_octl_info("no-results");
    } else {
        print_results(returned_list, start_time, stop_time);
    }
    SAFE_RELEASE(filter_list);
    SAFE_RELEASE(filter_list);
orcm_octl_query_event_exit:
    opal_argv_free(argv_node_list);
    return rc;
}

/**
 * @brief Function that interprets the command making the
 *        query and sending it to the orcm scheduler to be
 *        performed.
 *        Finally it prints the result on the screen.
 *        The report is:
 *        "Sensor data around N minutes before/after an event"
 *
 * @param cmd Command to be run. For this command it must be:
 *            ORCM_GET_DB_QUERY_EVENT_SNSR_DATA_COMMAND
 *
 * @param argv Array of strings. Each array position contains an
 *             argument of the command as a string.
 */
int orcm_octl_query_event_snsr_data(int cmd, char **argv)
{
    int rc = ORCM_SUCCESS;
    double start_time = 0.0;
    double stop_time = 0.0;
    char **argv_node_list = NULL;
    opal_list_t *filter_list = NULL;
    opal_list_t *returned_list = NULL;
    orcm_db_filter_t *node_list = NULL;
    char *date;

    date = get_orcm_octl_query_event_date(ORCM_GET_DB_QUERY_EVENT_DATE_COMMAND, argv);

    if (NULL == date) {
        orcm_octl_usage("query-event-sensor-data", INVALID_USG);
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_event_exit;
    } else {
        SAFE_FREE(argv[3]);
        argv[3] = date;
    }

    filter_list = build_filters_list(cmd, argv);
    if (NULL == filter_list) {
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_event_exit;
    }

    rc = get_nodes_from_args(argv, &argv_node_list);
    if (ORCM_SUCCESS != rc) {
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_event_exit;
    }

    node_list = build_node_item(argv_node_list);
    if (NULL != node_list) {
        opal_list_append(filter_list, &node_list->value.super);
    }

    start_time = stopwatch();
    rc = query_db(cmd, filter_list, &returned_list);
    stop_time = stopwatch();
    if (ORCM_SUCCESS != rc) {
        orcm_octl_info("no-results");
    } else {
        print_results(returned_list, start_time, stop_time);
    }
    SAFE_RELEASE(returned_list);
    SAFE_RELEASE(filter_list);
orcm_octl_query_event_exit:
    opal_argv_free(argv_node_list);
    return rc;
}

/**
 * @brief Function that interprets the command making the
 *        query and sending it to the orcm scheduler to be
 *        performed.
 *        The report is:
 *        "Obtain date of an event"
 *        As this is not a formal report it doesn't outputs
 *        to the screen, instead it returns the event date.
 *
 * @param cmd Command to be run. For this command it must be:
 *            ORCM_GET_DB_QUERY_EVENT_DATE_COMMAND
 *
 * @param argv Array of strings. Each array position contains an
 *             argument of the command as a string.
 *
 * @return char* Date of the event. DON'T FORGET TO FREE!.
 */
char* get_orcm_octl_query_event_date(int cmd, char **argv){
    int rc = ORCM_SUCCESS;
    uint32_t rows_retrieved = 0;
    opal_list_t *filter_list = NULL;
    opal_list_t *returned_list = NULL;
    opal_value_t *line = NULL;
    char *date = NULL;
    char **db_results = NULL;
    int num_db_results = 0;

    filter_list = build_filters_list(cmd, argv);
    if (NULL == filter_list) {
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_event_exit;
    }

    rc = query_db(cmd, filter_list, &returned_list);
    if (ORCM_SUCCESS == rc) {
        if (NULL != returned_list) {
            rows_retrieved = (uint32_t)opal_list_get_size(returned_list);
            if (1 < rows_retrieved){
                line = (opal_value_t*)opal_list_get_last(returned_list);
                num_db_results = split_db_results(line->data.string, &db_results);
                if ( NULL != db_results ) {
                    date = strndup(db_results[1], strlen(db_results[1]));
                    free_db_results(num_db_results, &db_results);
                }
            }
            OBJ_RELEASE(returned_list);
        }
    }

    SAFE_RELEASE(filter_list);
orcm_octl_query_event_exit:

    return date;
}

/**
 * @brief Currently, the query results that are returned from
 *        the DB through the ORCM Scheduler are in a comma
 *        separated string.
 *        This function allows to split them again into an array
 *        of results, something that consumes some valuable
 *        processing time but for now it's necessary.
 *        Be careful with this function because values are not
 *        being enclosed into quotation marks ("value,1" like in
 *        CSV format) so, if you have a comma in your value
 *        (value,1) you will end with an unexpected result for
 *        sure.
 *
 * @param db_res Comma separated string that contains the query
 *               results.
 *
 * @param db_results Pointer to an array of strings in which the
 *                   array of results will be stored.
 *
 * @return int Size of the array of results.
 */
int split_db_results(char *db_res, char ***db_results){
    regex_t regex_comp_db_res;
    int regex_res;
    regmatch_t db_res_matches[2];
    char *str_aux = NULL;
    int str_pos = 0;
    int db_res_length;
    int res_size=0;

    if (NULL == db_res || NULL == db_results) {
        return 0;
    }

    db_res_length = strlen(db_res);
    *db_results = (char **)malloc(res_size * sizeof(char *));

    regcomp(&regex_comp_db_res, "([^,]+)", REG_EXTENDED);
    while(str_pos < db_res_length) {
        str_aux = strndup(&db_res[str_pos], db_res_length - str_pos);
        regex_res = regexec(&regex_comp_db_res, str_aux, 2, db_res_matches,0);
        if (!regex_res) {
            res_size++;
            *db_results = (char **)realloc( *db_results, res_size * sizeof(char *) );
            if ( NULL != *db_results ) {
                (*db_results)[res_size - 1] = strndup(&str_aux[db_res_matches[1].rm_so],
                                              (int)(db_res_matches[1].rm_eo - db_res_matches[1].rm_so));
                str_pos += db_res_matches[1].rm_eo;
            }
            SAFEFREE(str_aux);
        } else {
            str_pos++;
        }
    }

    return res_size;
}

/**
 * @brief Function that frees the memory used to split a
 *        query-comma-separated-string of results.
 *
 * @param num_elements Size of the array to free.
 *
 * @param db_results Pointer to an array of strings that
 *                   contains the query results to free.
 */
void free_db_results(int num_elements, char ***db_res_array)
{
    for(int act_elem=0; act_elem < num_elements; act_elem++){
        SAFEFREE( (*db_res_array)[act_elem] );
    }

    SAFEFREE(*db_res_array);
}

/**
 * @brief Currently, all the data retrieved from the DB
 *        through the ORCM Scheduler for queries is in
 *        a comma separated string format.
 *        This function enables you to add time to a date
 *        in string format and retrieve it again as an
 *        string.
 *
 * @param date Date in string format "%Y-%m-%d %H:%M:%S"
 *             to which the time will be added.
 *
 * @param seconds Time to add to the date in seconds.
 *
 * @return char* Date after the time is added in string
 *               format. DON'T FORGET TO FREE!.
 */
char *add_to_str_date(char *date, int seconds){
    struct tm tm_date;
    time_t t_res_date;
    struct tm *tm_res_date;
    char *res_date = NULL;

    if( NULL != date ) {
        res_date = (char *)malloc(20);

        if( NULL != res_date ) {
            setlocale(LC_TIME, "UTC");
            strptime(date, "%Y-%m-%d %H:%M:%S", &tm_date);
            tm_date.tm_isdst = -1;
            t_res_date = mktime(&tm_date) + seconds;
            tm_res_date = localtime(&t_res_date);

            if ( NULL != tm_res_date ) {
                strftime(res_date, 20, "%Y-%m-%d %H:%M:%S", tm_res_date);
            }
        }
    }

    return res_date;
}



void print_results(opal_list_t *results, double start_time, double stop_time) {
    uint32_t rows = 0;
    opal_value_t *line = NULL;

    if (NULL != results) {
            rows = (uint32_t)opal_list_get_size(results);
            rows--;
            OPAL_LIST_FOREACH(line, results, opal_value_t) {
                printf("%s\n", line->data.string);
            }
        }
    orcm_octl_info("rows-found", rows, stop_time - start_time);
}

int print_results_from_stream(uint32_t results_count, int stream_index,
                              double start_time, double stop_time)
{
    int rc = ORCM_SUCCESS;
    int n = 1;
    int stream_index_recv = 0;
    uint32_t stream_size_recv = 0;
    opal_buffer_t *stream_buffer = NULL;
    orte_rml_recv_cb_t *xfer = NULL;
    char *tmp_str = NULL;

    rc = create_buffer_for_stream_request(&stream_buffer, ORCM_GET_DB_STREAM,
                                          stream_index);
    ON_FAILURE_GOTO(rc, print_from_stream_cleanup);
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    ON_NULL_GOTO(ORCM_ERR_OUT_OF_RESOURCE, print_from_stream_cleanup);
    xfer->active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ORCMD_FETCH,
                            ORTE_RML_NON_PERSISTENT, orte_rml_recv_callback,
                            xfer);
    rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, stream_buffer,
                                                      ORCM_RML_TAG_ORCMD_FETCH,
                                                      orte_rml_send_callback,
                                                      xfer);
    ON_FAILURE_GOTO(rc, print_from_stream_cleanup);
    ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
    ON_FAILURE_GOTO(rc, print_from_stream_cleanup);
    n = 1;
    rc = opal_dss.unpack(&xfer->data, &stream_index_recv, &n, OPAL_INT);
    ON_FAILURE_GOTO(rc, print_from_stream_cleanup);
    if (stream_index != stream_index_recv) {
        rc = OPAL_ERR_DATA_VALUE_NOT_FOUND;
        goto print_from_stream_cleanup;
    }
    n = 1;
    rc = opal_dss.unpack(&xfer->data, &stream_size_recv, &n, OPAL_UINT32);
    ON_FAILURE_GOTO(rc, print_from_stream_cleanup);
    for(uint32_t i = 0; i < stream_size_recv; ++i) {
        n = 1;
        rc = opal_dss.unpack(&xfer->data, &tmp_str, &n, OPAL_STRING);
        ON_FAILURE_GOTO(rc, print_from_stream_cleanup);
        printf("%s\n", tmp_str);
    }
    if (MAX_STREAM_SIZE > results_count) {
        orcm_octl_info("rows-found", results_count, stop_time-start_time);
    } else {
        orcm_octl_info("rows-found-limited", results_count, stop_time-start_time);
    }

    SAFE_RELEASE(xfer);
    return rc;

print_from_stream_cleanup:
    SAFE_RELEASE(stream_buffer);
    SAFE_RELEASE(xfer);

    return rc;
}
