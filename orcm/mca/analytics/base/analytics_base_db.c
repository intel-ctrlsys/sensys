/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/mca/db/db.h"

/* check whether this workflow step needs to load the data results to store */
static bool orcm_analytics_base_db_check(orcm_workflow_step_t *wf_step);

/* create a opal_value_t according to the key and data type*/
static opal_value_t *orcm_analytics_create_opal_value(char *key, opal_data_type_t type);

/* fill the primary key of the record to the database */
static int orcm_analytics_base_db_fill_primary_key(opal_list_t *db_input,
                                                   orcm_analytics_value_t *data_item,
                                                   char *plugin_name, int wf_id);

/* fill the data of the record to the database*/
static int orcm_analytics_db_fill_data(opal_list_t *db_input,
                                       opal_value_array_t *data_results, int data_size);

/* storing data to the database */
static int orcm_analytics_base_db_store(orcm_workflow_t *wf,
                                        orcm_workflow_step_t *wf_step,
                                        opal_value_array_t *data_results);

/* call back function to clean up for the db store function */
static void orcm_analytics_db_cleanup(int db_handle, int status, opal_list_t *list,
                                      opal_list_t *ret, void *cbdata);

orcm_analytics_base_db_t orcm_analytics_base_db = {-1, false};

void orcm_analytics_base_db_open_cb(int handle, int status, opal_list_t *props,
                                    opal_list_t *ret, void *cbdata)
{
    if (0 == status) {
        /* database open successfully! */
        orcm_analytics_base_db.db_handle = handle;
        orcm_analytics_base_db.db_handle_acquired = true;
    } else {
        opal_output(0, "Analytics framework: DB Open failed");
        orcm_analytics_base_db.db_handle_acquired = false;
    }

    if (NULL != props) {
        OPAL_LIST_RELEASE(props);
    }
}

void orcm_analytics_base_open_db(void)
{
    if (false == orcm_analytics_base_db.db_handle_acquired) {
        orcm_db.open("analytics", NULL, orcm_analytics_base_db_open_cb, NULL);
    }
}

void orcm_analytics_base_close_db(void)
{
    if (true == orcm_analytics_base_db.db_handle_acquired) {
        orcm_db.close(orcm_analytics_base_db.db_handle, NULL, NULL);
        orcm_analytics_base_db.db_handle = -1;
        orcm_analytics_base_db.db_handle_acquired = false;
    }
}

static bool orcm_analytics_base_db_check(orcm_workflow_step_t *wf_step)
{
    bool load_to_db = false;
    opal_value_t *attribute = NULL;
    char *db_attribute_key = "db";
    char *db_attribute_value_check = "yes";

    OPAL_LIST_FOREACH(attribute, &(wf_step->attributes), opal_value_t) {
        if (NULL == attribute) {
            break;
        }
        if (0 == strncmp(attribute->key, db_attribute_key, strlen(attribute->key) + 1)) {
            if (0 == strncmp(attribute->data.string, db_attribute_value_check,
                strlen(attribute->data.string) + 1)) {
                load_to_db = true;
            }
            break;
        }
    }

    return load_to_db;
}

static opal_value_t *orcm_analytics_create_opal_value(char *key, opal_data_type_t type)
{
    opal_value_t *kv = OBJ_NEW(opal_value_t);
    if (NULL != kv) {
        kv->key = strdup(key);
        kv->type = type;
    }
    return kv;
}

static void orcm_analytics_db_cleanup(int db_handle, int status, opal_list_t *list,
                                      opal_list_t *ret, void *cbdata)
{
    OPAL_LIST_RELEASE(list);
}

static int orcm_analytics_base_db_fill_primary_key(opal_list_t *db_input,
                                                   orcm_analytics_value_t *data_item,
                                                   char *plugin_name, int wf_id)
{
    struct timeval current_time;
    opal_value_t *kv = NULL;
    int rc = -1;

    /* fill the time to the db input */
    gettimeofday(&current_time, NULL);
    kv = orcm_analytics_create_opal_value("ctime", OPAL_TIMEVAL);
    if (NULL == kv) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    kv->data.tv = current_time;
    opal_list_append(db_input, &(kv->super));

    /* fill the hostname to the db input */
    kv = orcm_analytics_create_opal_value("hostname", OPAL_STRING);
    if (NULL == kv) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    kv->data.string = strdup(data_item->node_regex);
    opal_list_append(db_input, &(kv->super));

    /* fill the data_group to the db input: sensorname_pluginname_workflowid */
    kv = orcm_analytics_create_opal_value("data_group", OPAL_STRING);
    if (NULL == kv) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    rc = asprintf(&(kv->data.string), "%s_%s_workflow %d",
                  data_item->sensor_name, plugin_name, wf_id);
    if (0 > rc) {
        OBJ_RELEASE(kv);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    opal_list_append(db_input, &(kv->super));

    return ORCM_SUCCESS;
}

static int orcm_analytics_db_fill_data(opal_list_t *db_input,
                                       opal_value_array_t *data_results, int data_size)
{
    orcm_analytics_value_t *data_item = NULL;
    orcm_value_t *analytics_metric = NULL;
    int index = -1, rc = -1;

    /* fill the actual data results to the db input */
    for (index = 0; index < data_size; index++) {
        data_item = (orcm_analytics_value_t*)opal_value_array_get_item(data_results, index);
        if (NULL == data_item) {
            OPAL_LIST_RELEASE(db_input);
            return ORCM_ERR_DATA_VALUE_NOT_FOUND;
        }
        analytics_metric = OBJ_NEW(orcm_value_t);
        if (NULL == analytics_metric) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        analytics_metric->units = strdup(data_item->data.units);
        rc = asprintf(&(analytics_metric->value.key), "core %d", index);
        if (0 > rc) {
            OBJ_RELEASE(analytics_metric);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        analytics_metric->value.type = data_item->data.value.type;
        analytics_metric->value.data.fval = data_item->data.value.data.fval;
        opal_list_append(db_input, (opal_list_item_t*)(analytics_metric));
    }

    return ORCM_SUCCESS;

}

static int orcm_analytics_base_db_store(orcm_workflow_t *wf,
                                        orcm_workflow_step_t *wf_step,
                                        opal_value_array_t *data_results)
{
    opal_list_t *db_input = NULL;
    orcm_analytics_value_t *data_item = NULL;
    int data_size = -1, rc = -1;

    /* if the size of the data results is 0, simply return */
    data_size = opal_value_array_get_size(data_results);
    if (0 >= data_size) {
        return ORCM_ERR_BAD_PARAM;
    }

    /* try to fill the primary key of the record */
    data_item = (orcm_analytics_value_t*)opal_value_array_get_item(data_results, 0);
    if (NULL == data_item) {
        return ORCM_ERR_DATA_VALUE_NOT_FOUND;
    }
    db_input = OBJ_NEW(opal_list_t);
    if (NULL == db_input) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    rc = orcm_analytics_base_db_fill_primary_key(db_input, data_item,
                                                 wf_step->analytic, wf->workflow_id);
    if (ORCM_ERR_OUT_OF_RESOURCE == rc) {
        goto cleanup;
    }

    /* try to fill the actual data of the record */
    rc = orcm_analytics_db_fill_data(db_input, data_results, data_size);
    if (ORCM_ERR_DATA_VALUE_NOT_FOUND == rc) {
        return rc;
    }
    if (ORCM_ERR_OUT_OF_RESOURCE == rc) {
        goto cleanup;
    }

    /* try to load the data to database */
    if (0 <= orcm_analytics_base_db.db_handle) {
        orcm_db.store_new(orcm_analytics_base_db.db_handle, ORCM_DB_ENV_DATA,
                          db_input, NULL, orcm_analytics_db_cleanup, NULL);
        return ORCM_SUCCESS;
    }

    OPAL_LIST_RELEASE(db_input);
    return ORCM_ERR_NO_CONNECTION_ALLOWED;

cleanup:
    OPAL_LIST_RELEASE(db_input);
    return ORCM_ERR_OUT_OF_RESOURCE;
}

int orcm_analytics_base_store(orcm_workflow_t *wf,
                              orcm_workflow_step_t *wf_step,
                              opal_value_array_t *data_results)
{
    /* If any parameter is NULL, then return bad parameter */
    if (NULL == wf || NULL == wf_step || NULL == data_results) {
        return ORCM_ERR_BAD_PARAM;
    }

    /* Database connection is open */
    if (true == orcm_analytics_base_db.db_handle_acquired) {
        /* DB attribute matches "yes", load data to db */
        if (true == orcm_analytics_base_db_check(wf_step)) {
            return orcm_analytics_base_db_store(wf, wf_step, data_results);
        }

        /* DB attribute does not match "yes", no need to load data */
        return ORCM_ERR_NO_MATCH_YET;
    }

    /* Database connection is not allowed */
    return ORCM_ERR_NO_CONNECTION_ALLOWED;
}
