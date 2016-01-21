/*
 * Copyright (c) 2014-2015  Intel, Inc.  All rights reserved.
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

#define SAFE_FREE(x) if(NULL!=x) { free(x); x = NULL; }
/* Default idle time in seconds */
#define DEFAULT_IDLE_TIME "10"

int query_db(int cmd, opal_list_t *filterlist, opal_list_t** results);
int get_nodes_from_args(char **argv, char ***node_list);
opal_list_t *create_query_sensor_filter(int argc, char **argv);
opal_list_t *create_query_idle_filter(int argc, char **argv);
opal_list_t *create_query_log_filter(int argc,char **argv);
opal_list_t *create_query_history_filter(int argc, char **argv);
opal_list_t *create_query_node_filter(int argc, char **argv);

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
    uint16_t results_count = 0;
    int returned_status = 0;

    if (NULL == filterlist || NULL == results){
        rc = ORCM_ERR_BAD_PARAM;
        goto query_db_cleanup;
    }
    filterlist_count = (uint16_t)opal_list_get_size(filterlist);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &command, 1, ORCM_RM_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto query_db_cleanup;
                        }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &filterlist_count, 1, OPAL_UINT16))) {
        ORTE_ERROR_LOG(rc);
        goto query_db_cleanup;
    }
    OPAL_LIST_FOREACH(tmp_filter, filterlist, orcm_db_filter_t) {
        uint8_t operation = (uint8_t)tmp_filter->op;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &tmp_filter->value.key, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            goto query_db_cleanup;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &operation, 1, OPAL_UINT8))) {
            ORTE_ERROR_LOG(rc);
            goto query_db_cleanup;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &tmp_filter->value.data.string, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            goto query_db_cleanup;
        }
    }
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    xfer->active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ORCMD_FETCH, ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, xfer);

    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buffer,
                                                      ORCM_RML_TAG_ORCMD_FETCH,
                                                      orte_rml_send_callback, xfer))) {
        ORTE_ERROR_LOG(rc);
        goto query_db_cleanup;
    }
    /* wait for status message */
    ORTE_WAIT_FOR_COMPLETION(xfer->active);

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &returned_status, &n, OPAL_INT))) {
        goto query_db_cleanup;
    }
    if(0 == returned_status) {
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &results_count, &n, OPAL_UINT16))) {
            goto query_db_cleanup;
        }
        (*results) = OBJ_NEW(opal_list_t);
        for(uint16_t i = 0; i < results_count; ++i) {
            char* tmp_str = NULL;
            opal_value_t *tmp_value = NULL;
            n = 1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &tmp_str, &n, OPAL_STRING))) {
                goto query_db_cleanup;
            }
            tmp_value = OBJ_NEW(opal_value_t);
            tmp_value->type = OPAL_STRING;
            tmp_value->data.string = tmp_str;
            opal_list_append(*results, &tmp_value->super);
            }
    } else {
        printf("* No Results Returned *\n");
        rc = ORCM_SUCCESS;
        *results = NULL;
    }
query_db_cleanup:
    if (NULL != xfer){
        OBJ_RELEASE(xfer);
    }
    if (NULL != filterlist){
        OBJ_RELEASE(filterlist);
    }
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
        fprintf(stdout, "ERROR: an array without arguments was received\n");
        return ORCM_ERR_BAD_PARAM;
    }
    arg_index = opal_argv_count(argv);
    /* Convert argv count to index */
    arg_index--;
    if (2 > arg_index){
        fprintf(stdout, "\nIt is necessary to provide a host or group to perform a query!\n");
        return ORCM_ERR_BAD_PARAM;
    }
    raw_node_list = argv[arg_index];
    if (NULL == raw_node_list ) {
        fprintf(stdout, "ERROR: An empty list of nodes was specified\n");
        return ORCM_ERR_BAD_PARAM;
    }
    rc = orcm_logical_group_parse_array_string(raw_node_list, &argv_node_list);
    node_count = opal_argv_count(argv_node_list);
    if (0 == node_count) {
        fprintf(stdout, "ERROR: unable to extract nodelist or an empty list was specified\n");
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
                    } else {
                        fprintf(stdout, "\nWARNING: time input was ignored due the presence of the * character\n");
                    }
                }
           } else {
               fprintf(stderr, "\nERROR: could not allocate memory for datetime string\n");
           }
        } else {
            SAFE_FREE(date_time_str);
        }
    }
    SAFE_FREE(new_date_str);
    SAFE_FREE(new_time_str);
    return date_time_str;
}

opal_list_t *create_query_idle_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    char *error = NULL;
    char error_code = 0;

    filters_list = OBJ_NEW(opal_list_t);
    if (3 == argc) {
        filter_item = OBJ_NEW(orcm_db_filter_t);
        filter_item->value.type = OPAL_STRING;
        filter_item->value.key = strdup("idle_time");
        filter_item->value.data.string = strdup(DEFAULT_IDLE_TIME);
        filter_item->op = GT;
        opal_list_append(filters_list, &filter_item->value.super);
    } else if (4 == argc) {
        filter_item = OBJ_NEW(orcm_db_filter_t);
        filter_item->value.type = OPAL_STRING;
        filter_item->value.key = strdup("idle_time");
        filter_item->value.data.string = strdup(argv[2]);
        filter_item->op = GT;
        opal_list_append(filters_list, &filter_item->value.super);
    } else {
        error = "incorrect arguments!";
        error_code = ORCM_ERR_BAD_PARAM;
        orte_show_help("help-octl.txt",
                       "octl:query:idle",
                        true, error, ORTE_ERROR_NAME(error_code), error_code);
        return NULL;
    }
    return filters_list;
}

opal_list_t *create_query_log_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    char *date_time_str = NULL;
    char *error = NULL;
    char error_code = 0;
    char *filter_str = NULL;

    filters_list = OBJ_NEW(opal_list_t);
    if (3 == argc) {
    /* There's no need to create a filter */
        NULL;
    } else if (4 == argc){
        filter_item = OBJ_NEW(orcm_db_filter_t);
        filter_item->value.type = OPAL_STRING;
        filter_item->value.key = strdup("log");
        /* Add 3 more chars including the end of the string '\0' */
        if (NULL != (filter_str = calloc(sizeof(char), strlen(argv[2])+3))){
            strncat(filter_str, "%", strlen("%"));
            strncat(filter_str, argv[2], strlen(argv[2]));
            strncat(filter_str, "%", strlen("%"));
            filter_item->value.data.string = filter_str;
        } else {
            fprintf(stderr, "\nERROR: could not allocate memory for filter string\n");
        }
        filter_item->op = CONTAINS;
        opal_list_append(filters_list, &filter_item->value.super);
    } else if (7 == argc){
        /* Create filter for start time if necessary */
        if (NULL != (date_time_str = assemble_datetime(argv[2], argv[3]))) {
            filter_item = OBJ_NEW(orcm_db_filter_t);
            filter_item->value.type = OPAL_STRING;
            filter_item->value.key = strdup("time_stamp");
            filter_item->value.data.string = date_time_str;
            filter_item->op = GT;
            opal_list_append(filters_list, &filter_item->value.super);
        }
        /* Create filter for end time if necessary */
        if (NULL != (date_time_str = assemble_datetime(argv[4], argv[5]))) {
            filter_item = OBJ_NEW(orcm_db_filter_t);
            filter_item->value.type = OPAL_STRING;
            filter_item->value.key = strdup("time_stamp");
            filter_item->value.data.string = date_time_str;
            filter_item->op = LT;
            opal_list_append(filters_list, &filter_item->value.super);
            }
    } else if (8 == argc){
        filter_item = OBJ_NEW(orcm_db_filter_t);
        filter_item->value.type = OPAL_STRING;
        filter_item->value.key = strdup("log");
        /* Add 3 more chars including the end of the string '\0' */
        if (NULL != (filter_str = calloc(sizeof(char), strlen(argv[2])+3))){
            strncat(filter_str, "%", strlen("%"));
            strncat(filter_str, argv[2], strlen(argv[2]));
            strncat(filter_str, "%", strlen("%"));
            filter_item->value.data.string = filter_str;
        } else {
            fprintf(stderr, "\nERROR: could not allocate memory for filter string\n");
        }
        filter_item->op = CONTAINS;
        opal_list_append(filters_list, &filter_item->value.super);
        /* Create filter for start time if necessary */
        if (NULL != (date_time_str = assemble_datetime(argv[3], argv[4]))) {
            filter_item = OBJ_NEW(orcm_db_filter_t);
            filter_item->value.type = OPAL_STRING;
            filter_item->value.key = strdup("time_stamp");
            filter_item->value.data.string = date_time_str;
            filter_item->op = GT;
            opal_list_append(filters_list, &filter_item->value.super);
        }
        /* Create filter for end time if necessary */
        if (NULL != (date_time_str = assemble_datetime(argv[5], argv[6]))) {
            filter_item = OBJ_NEW(orcm_db_filter_t);
            filter_item->value.type = OPAL_STRING;
            filter_item->value.key = strdup("time_stamp");
            filter_item->value.data.string = date_time_str;
            filter_item->op = LT;
            opal_list_append(filters_list, &filter_item->value.super);
            }
    } else {
        error = "incorrect arguments!";
        error_code = ORCM_ERR_BAD_PARAM;
        orte_show_help("help-octl.txt",
                       "octl:query:log",
                        true, error, ORTE_ERROR_NAME(error_code), error_code);
        return NULL;
    }
    return filters_list;
}

opal_list_t *create_query_node_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    char *error = NULL;
    char error_code = 0;

    filters_list = OBJ_NEW(opal_list_t);

    if (4 == argc) {
    /* Place holder for functionality when available in the DB */
        NULL;
    } else {
        error = "incorrect arguments!";
        error_code = ORCM_ERR_BAD_PARAM;
        orte_show_help("help-octl.txt",
                       "octl:query:node:status",
                        true, error, ORTE_ERROR_NAME(error_code), error_code);
        return NULL;
    }
    return filters_list;
}

opal_list_t *create_query_history_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    char *date_time_str = NULL;
    char *error = NULL;
    char error_code = 0;

    filters_list = OBJ_NEW(opal_list_t);
    if (3 == argc) {
        filter_item = OBJ_NEW(orcm_db_filter_t);
        filter_item->value.type = OPAL_STRING;
        filter_item->value.key = strdup("data_item");
        filter_item->value.data.string = strdup("%");
        filter_item->op = CONTAINS;
        opal_list_append(filters_list, &filter_item->value.super);
    } else if (7 == argc){
        /* Create filter for start time if necessary */
        if (NULL != (date_time_str = assemble_datetime(argv[2],argv[3]))) {
                filter_item = OBJ_NEW(orcm_db_filter_t);
                filter_item->value.type = OPAL_STRING;
                filter_item->value.key = strdup("time_stamp");
                filter_item->value.data.string = date_time_str;
                filter_item->op = GT;
                opal_list_append(filters_list, &filter_item->value.super);
            }
            /* Create filter for end time if necessary */
            if (NULL != (date_time_str = assemble_datetime(argv[4], argv[5]))) {
                filter_item = OBJ_NEW(orcm_db_filter_t);
                filter_item->value.type = OPAL_STRING;
                filter_item->value.key = strdup("time_stamp");
                filter_item->value.data.string = date_time_str;
                filter_item->op = LT;
                opal_list_append(filters_list, &filter_item->value.super);
            }
        } else {
            error = "incorrect arguments!";
            error_code = ORCM_ERR_BAD_PARAM;
            orte_show_help("help-octl.txt",
                           "octl:query:history",
                            true, error, ORTE_ERROR_NAME(error_code), error_code);
            return NULL;
        }
    return filters_list;
}

opal_list_t *create_query_sensor_filter(int argc, char **argv)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    char *date_time_str = NULL;
    char *error = NULL;
    char error_code = 0;
    char *filter_str = NULL;

    filters_list = OBJ_NEW(opal_list_t);
    if (4 == argc){
            /* Create a filter for a sensor */
            filter_str = strdup(argv[2]);
            replace_wildcard(&filter_str, false);
            filter_item = OBJ_NEW(orcm_db_filter_t);
            filter_item->value.type = OPAL_STRING;
            filter_item->value.key = strdup("data_item");
            filter_item->value.data.string = filter_str;
            filter_item->op = CONTAINS;
            opal_list_append(filters_list, &filter_item->value.super);
    } else if (8 == argc){
            /* Create a filter for a sensor */
            filter_str = strdup(argv[2]);
            replace_wildcard(&filter_str, false);
            filter_item = OBJ_NEW(orcm_db_filter_t);
            filter_item->value.type = OPAL_STRING;
            filter_item->value.key = strdup("data_item");
            filter_item->value.data.string = filter_str;
            filter_item->op = CONTAINS;
            opal_list_append(filters_list, &filter_item->value.super);
            /* Create filter for start time if necessary */
            if (NULL != (date_time_str = assemble_datetime(argv[3], argv[4]))) {
                filter_item = OBJ_NEW(orcm_db_filter_t);
                filter_item->value.type = OPAL_STRING;
                filter_item->value.key = strdup("time_stamp");
                filter_item->value.data.string = date_time_str;
                filter_item->op = GT;
                opal_list_append(filters_list, &filter_item->value.super);
            }
            /* Create filter for end time if necessary */
            if (NULL != (date_time_str = assemble_datetime(argv[5], argv[6]))) {
               filter_item = OBJ_NEW(orcm_db_filter_t);
               filter_item->value.type = OPAL_STRING;
               filter_item->value.key = strdup("time_stamp");
               filter_item->value.data.string = date_time_str;
               filter_item->op = LT;
               opal_list_append(filters_list, &filter_item->value.super);
           }
    } else if (10 == argc){
            /* Create a filter for a sensor */
            filter_str = strdup(argv[2]);
            replace_wildcard(&filter_str, false);
            filter_item = OBJ_NEW(orcm_db_filter_t);
            filter_item->value.type = OPAL_STRING;
            filter_item->value.key = strdup("data_item");
            filter_item->value.data.string = filter_str;
            filter_item->op = CONTAINS;
            opal_list_append(filters_list, &filter_item->value.super);
            /* Create filter for start time if necessary */
            if (NULL != (date_time_str = assemble_datetime(argv[3], argv[4]))) {
                filter_item = OBJ_NEW(orcm_db_filter_t);
                filter_item->value.type = OPAL_STRING;
                filter_item->value.key = strdup("time_stamp");
                filter_item->value.data.string = date_time_str;
                filter_item->op = GT;
                opal_list_append(filters_list, &filter_item->value.super);
            }
            /* Create filter for end time if necessary */
            if (NULL != (date_time_str = assemble_datetime(argv[5], argv[6]))) {
               filter_item = OBJ_NEW(orcm_db_filter_t);
               filter_item->value.type = OPAL_STRING;
               filter_item->value.key = strdup("time_stamp");
               filter_item->value.data.string = date_time_str;
               filter_item->op = LT;
               opal_list_append(filters_list, &filter_item->value.super);
            }
            filter_str = strdup(argv[7]);
            /* Create filter for upper bound necessary */
            if (false == replace_wildcard(&filter_str, true)){
               filter_item = OBJ_NEW(orcm_db_filter_t);
               filter_item->value.type = OPAL_STRING;
               filter_item->value.key = strdup("value_str");
               filter_item->value.data.string = filter_str;
               filter_item->op = GT;
               opal_list_append(filters_list, &filter_item->value.super);
            }
            filter_str = strdup(argv[8]);
            /* Create filter for upper bound necessary */
            if (false == replace_wildcard(&filter_str, true)){
               filter_item = OBJ_NEW(orcm_db_filter_t);
               filter_item->value.type = OPAL_STRING;
               filter_item->value.key = strdup("value_str");
               filter_item->value.data.string = filter_str;
               filter_item->op = LT;
               opal_list_append(filters_list, &filter_item->value.super);
            }
       } else {
            error = "incorrect arguments!";
            error_code = ORCM_ERR_BAD_PARAM;
            orte_show_help("help-octl.txt",
                           "octl:query:sensor",
                            true, error, ORTE_ERROR_NAME(error_code), error_code);
            return NULL;
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
        filter_item = OBJ_NEW(orcm_db_filter_t);
        /* Operator for query depends on whether we find a wildcard in the hostname arg */
        if (true == replace_wildcard(&hosts_filter, true)){
            filter_item->value.type = OPAL_STRING;
            filter_item->value.key = strdup("hostname");
            filter_item->value.data.string = strdup("%");
            filter_item->op = CONTAINS;
        } else {
            filter_item->value.type = OPAL_STRING;
            filter_item->value.key = strdup("hostname");
            filter_item->value.data.string = strdup(hosts_filter);
            filter_item->op = IN;
        }
        SAFE_FREE(hosts_filter);
    } else {
        fprintf(stderr, "\nERROR:: Unable to build node item\n");
    }
    return filter_item;
}

int orcm_octl_query_sensor(int cmd, char **argv)
{
    int rc = ORCM_SUCCESS;
    uint16_t rows_retrieved = 0;
    char **argv_node_list = NULL;
    double start_time = 0.0;
    double stop_time = 0.0;
    opal_list_t *filter_list = NULL;
    opal_list_t *returned_list = NULL;
    opal_value_t *line = NULL;
    opal_value_t *nodes_list = NULL;

    if(ORCM_GET_DB_QUERY_SENSOR_COMMAND != cmd &&
       ORCM_GET_DB_QUERY_HISTORY_COMMAND !=cmd) {
        fprintf(stdout, "\nERROR: incorrect command argument: %d", cmd);
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_sensor_cleanup;
    }
    if (ORCM_SUCCESS != get_nodes_from_args(argv, &argv_node_list)){
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_sensor_cleanup;
    }
    /* Build input node list */
    filter_list = build_filters_list(cmd, argv);
    if (NULL == filter_list){
        fprintf(stderr, "\nERROR: unable to generate a filter list from command provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_sensor_cleanup;
    }
    if (NULL != (nodes_list = (opal_list_item_t*)build_node_item(argv_node_list))){
        opal_list_append(filter_list, &nodes_list->super);
    }
    /* Get list of results from scheduler (or other management node) */
    start_time = stopwatch();
    rc = query_db(cmd, filter_list, &returned_list);
    stop_time = stopwatch();
    if(rc != ORCM_SUCCESS) {
        fprintf(stdout, "\nNo results found!\n");
    } else {
        /* Raw CSV output for now... */
        if(NULL != returned_list) {
            rows_retrieved = (uint16_t)opal_list_get_size(returned_list);
            /* Actual number includes the header so we remove it*/
            rows_retrieved--;
            printf("\n");
            OPAL_LIST_FOREACH(line, returned_list, opal_value_t) {
                printf("%s\n", line->data.string);
            }
            OBJ_RELEASE(returned_list);
            printf("\n%u rows were found (%0.3f seconds)\n", rows_retrieved, stop_time-start_time);
        }
    }
orcm_octl_query_sensor_cleanup:
    opal_argv_free(argv_node_list);
    return rc;
}

int orcm_octl_query_log(int cmd, char **argv)
{
    int rc = ORCM_SUCCESS;
    uint16_t rows_retrieved = 0;
    char **argv_node_list = NULL;
    double start_time = 0.0;
    double stop_time = 0.0;
    opal_list_t *filter_list = NULL;
    opal_list_t *returned_list = NULL;
    opal_value_t *line = NULL;
    opal_value_t *nodes_list = NULL;

    if(ORCM_GET_DB_QUERY_LOG_COMMAND != cmd ) {
        fprintf(stdout, "\nERROR: incorrect command argument: %d", cmd);
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_log_cleanup;
    }
    if (ORCM_SUCCESS != get_nodes_from_args(argv, &argv_node_list)){
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_log_cleanup;
    }
    /* Build input node list */
    filter_list = build_filters_list(cmd, argv);
    if (NULL == filter_list){
        fprintf(stderr, "\nERROR: unable to generate a filter list from command provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_log_cleanup;
    }
    if (NULL != (nodes_list = (opal_list_item_t*)build_node_item(argv_node_list))){
        opal_list_append(filter_list, &nodes_list->super);
    }
    /* Get list of results from scheduler (or other management node) */
    start_time = stopwatch();
    rc = query_db(cmd, filter_list, &returned_list);
    stop_time = stopwatch();
    if(rc != ORCM_SUCCESS) {
        fprintf(stdout, "\nNo results found!\n");
    } else {
        /* Raw CSV output for now... */
        if(NULL != returned_list) {
            rows_retrieved = (uint16_t)opal_list_get_size(returned_list);
            /* Actual number includes the header so we remove it*/
            rows_retrieved--;
            printf("\n");
            OPAL_LIST_FOREACH(line, returned_list, opal_value_t) {
                printf("%s\n", line->data.string);
            }
            OBJ_RELEASE(returned_list);
            printf("\n%u rows were found (%0.3f seconds)\n", rows_retrieved, stop_time-start_time);
        }
    }
orcm_octl_query_log_cleanup:
    opal_argv_free(argv_node_list);
    return rc;
}

int orcm_octl_query_idle(int cmd, char **argv)
{
    int rc = ORCM_SUCCESS;
    uint16_t rows_retrieved = 0;
    char **argv_node_list = NULL;
    double start_time = 0.0;
    double stop_time = 0.0;
    opal_list_t *filter_list = NULL;
    opal_list_t *returned_list = NULL;
    opal_value_t *line = NULL;
    opal_value_t *nodes_list = NULL;

    if(ORCM_GET_DB_QUERY_IDLE_COMMAND != cmd ) {
        fprintf(stdout, "\nERROR: incorrect command argument: %d", cmd);
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_idle_cleanup;
    }
    if (ORCM_SUCCESS != get_nodes_from_args(argv, &argv_node_list)){
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_idle_cleanup;
    }
    /* Build input node list */
    filter_list = build_filters_list(cmd, argv);
    if (NULL == filter_list){
        fprintf(stderr, "\nERROR: unable to generate a filter list from command provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_idle_cleanup;
    }
    if (NULL != (nodes_list = (opal_list_item_t*)build_node_item(argv_node_list))){
        opal_list_append(filter_list, &nodes_list->super);
    }
    /* Get list of results from scheduler (or other management node) */
    start_time = stopwatch();
    rc = query_db(cmd, filter_list, &returned_list);
    stop_time = stopwatch();
    if(rc != ORCM_SUCCESS) {
        fprintf(stdout, "\nNo results found!\n");
    } else {
        /* Raw CSV output for now... */
        if(NULL != returned_list) {
            rows_retrieved = (uint16_t)opal_list_get_size(returned_list);
            /* Actual number includes the header so we remove it*/
            rows_retrieved--;
            printf("\n");
            OPAL_LIST_FOREACH(line, returned_list, opal_value_t) {
                printf("%s\n", line->data.string);
            }
            OBJ_RELEASE(returned_list);
            printf("\n%u rows were found (%0.3f seconds)\n", rows_retrieved, stop_time-start_time);
        }
    }
orcm_octl_query_idle_cleanup:
    opal_argv_free(argv_node_list);
    return rc;
}

int orcm_octl_query_node(int cmd, char **argv)
{
    int rc = ORCM_SUCCESS;
    uint16_t rows_retrieved = 0;
    char **argv_node_list = NULL;
    double start_time = 0.0;
    double stop_time = 0.0;
    opal_list_t *filter_list = NULL;
    opal_list_t *returned_list = NULL;
    opal_value_t *line = NULL;
    opal_value_t *nodes_list = NULL;

    if(ORCM_GET_DB_QUERY_NODE_COMMAND != cmd ) {
        fprintf(stdout, "\nERROR: incorrect command argument: %d", cmd);
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_node_cleanup;
    }
    if (ORCM_SUCCESS != (rc = get_nodes_from_args(argv, &argv_node_list))){
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_node_cleanup;
    }
    /* Build input node list */
    filter_list = build_filters_list(cmd, argv);
    if (NULL == filter_list){
        fprintf(stderr, "\nERROR: unable to generate a filter list from command provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_query_node_cleanup;
    }
    if (NULL != (nodes_list = (opal_list_item_t*)build_node_item(argv_node_list))){
        opal_list_append(filter_list, &nodes_list->super);
    }
    /* Get list of results from scheduler (or other management node) */
    start_time = stopwatch();
    rc = query_db(cmd, filter_list, &returned_list);
    stop_time = stopwatch();
    if(rc != ORCM_SUCCESS) {
        fprintf(stdout, "\nNo results found!\n");
    } else {
        /* Raw CSV output for now... */
        if(NULL != returned_list) {
            rows_retrieved = (uint16_t)opal_list_get_size(returned_list);
            /* Actual number includes the header so we remove it*/
            rows_retrieved--;
            printf("\n");
            OPAL_LIST_FOREACH(line, returned_list, opal_value_t) {
                printf("%s\n", line->data.string);
            }
            OBJ_RELEASE(returned_list);
            printf("\n%u rows were found (%0.3f seconds)\n", rows_retrieved, stop_time-start_time);
        }
    }
orcm_octl_query_node_cleanup:
    opal_argv_free(argv_node_list);
    return rc;
}
