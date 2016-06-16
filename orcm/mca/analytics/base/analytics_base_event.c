/*
 * Copyright (c) 2014-2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/util/utils.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/dispatch/dispatch.h"

#include <ctype.h>

#define ARRAY_SIZE(a) sizeof(a)/sizeof(*a)
#define KEY_SIZE 1000

#define ON_NULL_RETURN(x, e)  if(NULL==x) { return e; }

/*function to generate event based on user policies*/
static void generate_events(opal_hash_table_t* ts_table, opal_list_t* event_list,
                            event_filter* filter, bool suppress);
/*function to check for a repeat event*/
static bool is_repeat_event(opal_hash_table_t* ts_table, orcm_ras_event_t* ev,
                            event_filter* filter);
/*function to get timestamp of the last event*/
static int get_last_event_timestamp(opal_hash_table_t* ts_table, uint64_t key,
                                    time_t** ts, time_t current_ts);
/*function to generate a unique key based on event type, severity, reporter and key value*/
static uint64_t generate_event_hashkey(int ev_type, int severity,
                                       opal_list_t* reporter, opal_list_t* compute_list);
/*function to check if event matches the user defined filter criteria and return the suppress interval*/
static unsigned int filter_max_rate(orcm_ras_event_t* ev, event_filter* filter);

/*function to get filter values from user policies*/
static int assign_filter_value(char* key, char* value, event_filter* filter);



static void event_filter_con(event_filter *filter)
{
    filter->category = NULL;
    filter->severity = ORCM_RAS_SEVERITY_UNKNOWN;
    filter->time = orcm_util_get_time_in_sec(orcm_analytics_base.suppress_repeat);
}

static void event_filter_des(event_filter *filter)
{
    SAFEFREE(filter->category);
}

OBJ_CLASS_INSTANCE(event_filter, opal_object_t, event_filter_con, event_filter_des);

typedef struct {
    const char *cat_string;
    unsigned int cat_int;
} orcm_analytics_event_cat_types_t;


static uint64_t generate_event_hashkey(int ev_type, int severity, opal_list_t* reporter, opal_list_t* compute_list)
{
    char *hash_key = NULL;
    char *type = NULL;
    char *sev = NULL;
    opal_value_t *attribute = NULL;
    opal_value_t *compute_data = NULL;
    uint64_t key = 0;

    if (NULL == (hash_key = (char*)calloc(KEY_SIZE, sizeof(char)))) {
        goto cleanup;
    }

    OPAL_LIST_FOREACH(attribute, reporter, opal_value_t) {
        if(NULL != attribute->data.string){
            strncat(hash_key, attribute->data.string, strlen(attribute->data.string));
        }
    }
    if(NULL == hash_key){
        goto cleanup;
    }
    OPAL_LIST_FOREACH(compute_data, compute_list, opal_value_t) {
        if(NULL != compute_data->key){
            strncat(hash_key, compute_data->key, strlen(compute_data->key));
        }
    }

    if (-1 == (asprintf(&type, "%d", ev_type))) {
        goto cleanup;
    }
    if (-1 == (asprintf(&sev, "%d", severity))) {
        goto cleanup;
    }
    strncat(hash_key, type, strlen(type));
    strncat(hash_key, sev, strlen(sev));

    key = orcm_util_create_hash_key(hash_key, strlen(hash_key) + 1);

cleanup:
    SAFEFREE(hash_key);
    SAFEFREE(type);
    SAFEFREE(sev);
    return key;
}

static int get_last_event_timestamp(opal_hash_table_t* ts_table, uint64_t key, time_t** ts, time_t current_ts)
{
    int rc = ORCM_SUCCESS;
    time_t* setval = NULL;
    if(OPAL_ERR_NOT_FOUND == (rc = opal_hash_table_get_value_uint64(ts_table, key, (void**)ts))){
        setval = (time_t*) malloc (sizeof(time_t));
        if(NULL == setval){
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        memcpy(setval, &current_ts, sizeof(time_t));
        if(OPAL_SUCCESS == (rc == opal_hash_table_set_value_uint64(ts_table, key, setval))) {
            return ORCM_ERR_NOT_FOUND;
        } else {
            SAFEFREE(setval);
        }
    }
    if(rc != OPAL_SUCCESS){
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

static unsigned int filter_max_rate(orcm_ras_event_t* ev, event_filter* filter)
{
    opal_value_t* category = NULL;
    bool match_category = true;
    bool match_severity = true;

    if(NULL == filter){
        goto done;
    }
    if(NULL != filter->category){
        OPAL_LIST_FOREACH(category, &ev->description, opal_value_t) {
            if(NULL == category->key || NULL == category->data.string){
                continue;
            }
            if(0 == strncmp(category->key, "event_category", strlen(category->key))){
                if(0 != strncmp(filter->category, category->data.string, strlen(filter->category))){
                    match_category = false;
                }
            }
        }
    }
    if(ORCM_RAS_SEVERITY_UNKNOWN != ev->severity && ev->severity > filter->severity){
        match_severity = false;
    }
    if(match_category && match_severity){
        return filter->time;
    }
done:
    return orcm_util_get_time_in_sec(orcm_analytics_base.suppress_repeat);
}


static bool is_repeat_event(opal_hash_table_t* ts_table, orcm_ras_event_t* ev, event_filter* filter)
{
    int rc = ORCM_SUCCESS;
    uint64_t key;
    time_t* ts_add = NULL;
    unsigned int repeat_interval;
    key = generate_event_hashkey(ev->type, ev->severity, &ev->reporter, &ev->data);
    if(0 == key){
        return false;
    }
    rc = get_last_event_timestamp(ts_table, key, &ts_add, ev->timestamp);
    repeat_interval = filter_max_rate(ev, filter);
    if(ORCM_SUCCESS == rc){
        if(ev->timestamp < *ts_add + repeat_interval){
            return true;
        } else {
            memcpy(ts_add, &ev->timestamp, sizeof(time_t));
            return false;
        }
    }
    return false;
}

static orcm_ras_event_t* get_next_event(opal_list_t* event_list)
{
    orcm_ras_event_t* ras_event = NULL;
    opal_value_t* ev_val = (opal_value_t*) opal_list_remove_first(event_list);
    if(NULL != ev_val && NULL != ev_val->data.ptr){
        ras_event = (orcm_ras_event_t*)ev_val->data.ptr;
    }
    SAFE_RELEASE(ev_val);
    return ras_event;
}

static void generate_events(opal_hash_table_t* ts_table, opal_list_t* event_list, event_filter* filter, bool suppress)
{
    orcm_ras_event_t* ras_event = NULL;
    if(!suppress){
        while(0 < event_list->opal_list_length){
            if(NULL != (ras_event = get_next_event(event_list))){
                ORCM_RAS_EVENT(ras_event);
            }
        }
        goto done;
    }
    while(0 < event_list->opal_list_length) {
        if(NULL != (ras_event = get_next_event(event_list))){
            if(!is_repeat_event(ts_table, ras_event, filter)){
                ORCM_RAS_EVENT(ras_event);
            } else {
                SAFE_RELEASE(ras_event);
            }
        }
    }
done:
     SAFE_RELEASE(event_list);
}

int orcm_analytics_event_get_severity(char* severity)
{
    int sev;
    if ( 0 == strcmp(severity, "emerg") ) {
        sev = ORCM_RAS_SEVERITY_EMERG;
    } else if ( 0 == strcmp(severity, "alert") ) {
        sev = ORCM_RAS_SEVERITY_ALERT;
    } else if ( 0 == strcmp(severity, "crit") ) {
        sev = ORCM_RAS_SEVERITY_CRIT;
    } else if ( 0 == strcmp(severity, "error") ) {
        sev = ORCM_RAS_SEVERITY_ERROR;
    } else if ( 0 == strcmp(severity, "warn") ) {
        sev = ORCM_RAS_SEVERITY_WARNING;
    } else if ( 0 == strcmp(severity, "notice") ) {
        sev = ORCM_RAS_SEVERITY_NOTICE;
    } else if ( 0 == strcmp(severity, "info") ) {
        sev = ORCM_RAS_SEVERITY_INFO;
    } else if ( 0 == strcmp(severity, "debug") ) {
        sev = ORCM_RAS_SEVERITY_DEBUG;
    } else {
        sev = ORCM_RAS_SEVERITY_UNKNOWN;
    }
    return sev;
}

static int assign_filter_value(char* key, char* value, event_filter* filter)
{
    int rc = ORCM_SUCCESS;
    if(NULL == key || NULL == value || NULL == filter){
        return ORCM_ERROR;
    }
    if(0 == strncmp(key, "category", strlen(key))){
        filter->category = strdup(value);
    }
    else if(0 == strncmp(key, "severity", strlen(key))){
        filter->severity = orcm_analytics_event_get_severity(value);
    }
    else if (0 == strncmp(key, "time", strlen(key))){
        if(0 == (filter->time = orcm_util_get_time_in_sec(value))){
            filter->time = orcm_util_get_time_in_sec(orcm_analytics_base.suppress_repeat);
        }
    } else {
        rc = ORCM_ERROR;
    }
    return rc;
}

void orcm_analytics_base_filter_events (void* event_list, orcm_workflow_step_t *wf_step)
{
    bool suppress = false;
    opal_list_t *list_attributes = &wf_step->attributes;
    opal_value_t* attribute = NULL;
    event_filter* filter = NULL;

    if(NULL == list_attributes){
        goto done;
    }
    OPAL_LIST_FOREACH(attribute, list_attributes, opal_value_t) {
        if (NULL == attribute || NULL == attribute->key) {
            break;
        }
        if (0 == strncmp(attribute->key, "suppress_repeat", strlen(attribute->key) + 1)) {
            if (0 == strncmp(attribute->data.string, "yes",
                strlen(attribute->data.string) + 1)) {
                suppress = true;
            }
        }
        else if(0 == strncmp(attribute->key, "category", strlen(attribute->key) + 1) ||
                0 == strncmp(attribute->key, "severity", strlen(attribute->key) + 1) ||
                0 == strncmp(attribute->key, "time", strlen(attribute->key) + 1)) {
            if(!suppress){
                break;
            }
            if(NULL == filter){
                filter = OBJ_NEW(event_filter);
            }
            if(ORCM_SUCCESS != assign_filter_value(attribute->key, attribute->data.string, filter)){
                break;
            }
        }
    }
done:
    generate_events(wf_step->mod->orcm_mca_analytics_event_store, (opal_list_t*)event_list, filter, suppress);
    SAFE_RELEASE(filter);
}

int event_list_append(opal_list_t* event_list, orcm_ras_event_t* ev)
{
    opal_value_t* opal_ev = NULL;

    if(NULL == event_list || NULL == ev){
        return ORCM_ERR_BAD_PARAM;
    }

    opal_ev = OBJ_NEW(opal_value_t);
    ORCM_ON_NULL_RETURN_ERROR(opal_ev, ORCM_ERR_OUT_OF_RESOURCE);
    opal_ev->data.ptr = ev;
    ev = NULL;
    opal_list_append(event_list, (opal_list_item_t*)opal_ev);
    return ORCM_SUCCESS;
}

int orcm_analytics_base_event_set_reporter(orcm_ras_event_t *analytics_event_data, char *key,
                                           void *data, opal_data_type_t type, char *units)
{
    orcm_value_t *analytics_orcm_value = NULL;

    analytics_orcm_value = orcm_util_load_orcm_value(key, data, type, units);
    if (NULL == analytics_orcm_value) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    opal_list_append(&(analytics_event_data->reporter), (opal_list_item_t *)analytics_orcm_value);

    return ORCM_SUCCESS;
}

int orcm_analytics_base_event_set_description(orcm_ras_event_t *analytics_event_data, char *key,
                                              void *data, opal_data_type_t type, char *units)
{
    orcm_value_t *analytics_orcm_value = NULL;

    analytics_orcm_value = orcm_util_load_orcm_value(key, data, type, units);
    if (NULL == analytics_orcm_value) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    opal_list_append(&(analytics_event_data->description), (opal_list_item_t *)analytics_orcm_value);

    return ORCM_SUCCESS;
}

static void orcm_analytics_base_event_cleanup(void *cbdata)
{
    orcm_ras_event_t *analytics_event_data;

    if (NULL == cbdata) {
        return;
    }

    analytics_event_data = (orcm_ras_event_t *)cbdata;
    OBJ_RELEASE(analytics_event_data);
}

static time_t orcm_analytics_base_event_set_time(opal_list_t *src_list)
{
    orcm_value_t *list_item = NULL;

    OPAL_LIST_FOREACH(list_item, src_list, orcm_value_t) {
        if (NULL == list_item) {
            return 0;
        }
        if (0 == strcmp("ctime", list_item->value.key )) {
            return list_item->value.data.tv.tv_sec;
        }
    }
    return 0;
}


orcm_ras_event_t* orcm_analytics_base_event_create(orcm_analytics_value_t *analytics_data, int type, int severity)
{
    orcm_ras_event_t *analytics_event_data = OBJ_NEW(orcm_ras_event_t);

    if (NULL != analytics_event_data) {

        if (ORCM_SUCCESS != orcm_util_copy_list_items(analytics_data->key, &(analytics_event_data->reporter))) {
            OBJ_RELEASE(analytics_event_data);
            return NULL;
        }

        if (ORCM_SUCCESS != orcm_util_copy_list_items(analytics_data->compute_data, &(analytics_event_data->data))) {
            OBJ_RELEASE(analytics_event_data);
            return NULL;
        }

        analytics_event_data->timestamp = orcm_analytics_base_event_set_time(analytics_data->non_compute_data);

        /*Don't copy the timestamp again */
        if (!(analytics_event_data->timestamp > 0 && analytics_data->non_compute_data->opal_list_length == 1)) {
            if (ORCM_SUCCESS != orcm_util_copy_list_items(analytics_data->non_compute_data,
                                                          &(analytics_event_data->description))) {
                OBJ_RELEASE(analytics_event_data);
                return NULL;
            }
        }
        analytics_event_data->type = type;
        analytics_event_data->severity = severity;
        analytics_event_data->cbfunc = orcm_analytics_base_event_cleanup;
    }
    return analytics_event_data;
}


int orcm_analytics_base_event_set_category(orcm_ras_event_t *analytics_event_data, unsigned int event_category)
{
    unsigned int index = 0;
    const orcm_analytics_event_cat_types_t types[] = {
        {"SOFT_FAULT", ORCM_EVENT_SOFT_FAULT},
        {"HARD_FAULT", ORCM_EVENT_HARD_FAULT},
        {"UNKOWN_FAULT", ORCM_EVENT_UNKOWN_FAULT},
    };

    if (NULL == analytics_event_data) {
        return ORCM_ERROR;
    }

    for (index = 0; index < ARRAY_SIZE(types); index++) {
        if (types[index].cat_int== event_category) {
            return orcm_analytics_base_event_set_description(analytics_event_data, "event_category",
                                                             (void *) types[index].cat_string, OPAL_STRING, NULL);
        }
    }
    return ORCM_ERROR;
}

int orcm_analytics_base_event_set_storage(orcm_ras_event_t *analytics_event_data, unsigned int storage_type)
{
    if (NULL == analytics_event_data) {
        return ORCM_ERROR;
    }
    if (ORCM_STORAGE_TYPE_UNDEFINED <= storage_type) {
        return ORCM_ERROR;
    }

    return orcm_analytics_base_event_set_description(analytics_event_data, "storage_type",
                                                     &storage_type, OPAL_UINT, NULL);
}

static int orcm_analytics_base_set_event_attr(orcm_ras_event_t *ras_event,
                                              opal_value_t *attr_item, char *key)
{
    int rc = ORCM_SUCCESS;

    if (NULL == attr_item->data.string) {
        return ORCM_ERR_BAD_PARAM;
    }

    rc = orcm_analytics_base_event_set_description(ras_event, key,
                                                   attr_item->data.string, OPAL_STRING, NULL);

    return rc;
}

static int orcm_analytics_base_event_set_exec_attr(opal_list_t *wf_step_attr,
                                                   orcm_ras_event_t *ras_event)
{
    int rc = ORCM_SUCCESS;
    opal_value_t *attr_item = NULL;
    bool exec_name_set = false;
    bool exec_argv_set = false;
    char *exec_name_key = "exec_name";
    char *exec_argv_key = "exec_argv";

    OPAL_LIST_FOREACH(attr_item, wf_step_attr, opal_value_t) {
        if (NULL == attr_item || NULL == attr_item->key) {
            return ORCM_ERR_BAD_PARAM;
        }
        if (0 == strncmp(attr_item->key, exec_name_key, strlen(attr_item->key))) {
            rc = orcm_analytics_base_set_event_attr(ras_event, attr_item, exec_name_key);
            exec_name_set = true;
        } else if (0 == strncmp(attr_item->key, exec_argv_key, strlen(attr_item->key))) {
            rc = orcm_analytics_base_set_event_attr(ras_event, attr_item, exec_argv_key);
            exec_argv_set = true;
        }
        if (ORCM_SUCCESS != rc) {
            return rc;
        }
        if (exec_name_set && exec_argv_set) {
            break;
        }
    }

    if (!exec_name_set) {
        return ORCM_ERR_BAD_PARAM;
    }

    return rc;
}

static int orcm_analytics_base_event_set_notifier_attr(opal_list_t *wf_step_attr,
                                                       orcm_ras_event_t *ras_event,
                                                       char *msg, char *action)
{
    int rc = orcm_analytics_base_event_set_storage(ras_event, ORCM_STORAGE_TYPE_NOTIFICATION);
    if(ORCM_SUCCESS != rc){
        return rc;
    }

    if (ORCM_SUCCESS != (rc = orcm_analytics_base_event_set_description(ras_event,
                                  "notifier_msg", msg, OPAL_STRING,NULL))) {
       return rc;
    }
    if (ORCM_SUCCESS != (rc = orcm_analytics_base_event_set_description(ras_event,
                                  "notifier_action", action, OPAL_STRING, NULL))) {
        return rc;
    }

    if (0 == strncmp(action, "exec", strlen(action))) {
        return orcm_analytics_base_event_set_exec_attr(wf_step_attr, ras_event);
    }

    return ORCM_SUCCESS;
}

static orcm_analytics_value_t* orcm_analytics_base_get_analytics_value(orcm_value_t *current_value,
                                                                       orcm_workflow_caddy_t *caddy)
{
    orcm_analytics_value_t *analytics_value = NULL;
    opal_list_t* compute_data = OBJ_NEW(opal_list_t);
    orcm_value_t* analytics_orcm_value = NULL;

    ON_NULL_RETURN(compute_data, NULL);
    analytics_orcm_value = orcm_util_copy_orcm_value(current_value);
    if(NULL != analytics_orcm_value) {
        opal_list_append(compute_data, (opal_list_item_t *)analytics_orcm_value);
    }
    analytics_value = orcm_util_load_orcm_analytics_value_compute(caddy->analytics_value->key,
                      caddy->analytics_value->non_compute_data, compute_data);
    return analytics_value;
}

int orcm_analytics_base_gen_notifier_event(orcm_value_t* current_value,
                                           orcm_workflow_caddy_t* caddy,
                                           int severity, char *msg, char *action,
                                           opal_list_t *event_list)
{
    int rc = ORCM_SUCCESS;
    orcm_ras_event_t *ras_event = NULL;
    orcm_analytics_value_t* analytics_value = NULL;

    if (NULL != msg) {
        opal_output_verbose(5, orcm_analytics_base_framework.framework_output,
            "%s analytics:event message:%s",ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), msg);
    }

    ON_NULL_RETURN(action, ORCM_ERROR);
    if (0 == strncmp(action, "none", strlen(action))) {
        return ORCM_SUCCESS;
    }

    analytics_value = orcm_analytics_base_get_analytics_value(current_value, caddy);
    ON_NULL_RETURN(analytics_value, ORCM_ERR_OUT_OF_RESOURCE);
    if (NULL == (ras_event = orcm_analytics_base_event_create(analytics_value,
                                 ORCM_RAS_EVENT_EXCEPTION, severity))) {
        SAFE_RELEASE(analytics_value);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    rc = orcm_analytics_base_event_set_notifier_attr(&caddy->wf_step->attributes,
                                                     ras_event, msg, action);
    if (ORCM_SUCCESS != rc) {
        SAFE_RELEASE(analytics_value);
        SAFE_RELEASE(ras_event);
    } else {
        rc = event_list_append(event_list, ras_event);
    }
    ORCM_RELEASE(analytics_value);
    return rc;
}
