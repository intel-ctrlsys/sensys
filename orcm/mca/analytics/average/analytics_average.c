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

#include <stdio.h>
#include <ctype.h>

#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/mca/analytics/average/analytics_average.h"

#define HASH_TABLE_SIZE 10000

static int init(struct orcm_analytics_base_module_t *imod);
static void finalize(struct orcm_analytics_base_module_t *imod);
static void analyze(int sd, short args, void *cbdata);

/* functions related to the opal_value_array_t */
static int analytics_average_array_create(opal_value_array_t **analytics_average_array,
                                          int size);
static int analytics_average_array_set(opal_value_array_t *analytics_average_array,
                                       int index, char *host_name,
                                       char *sensor_name, opal_value_t *average_value);

/* analyze function for each data of the sample */
static opal_value_t* analyze_sample_item(orcm_analytics_value_t *current_value,
                                         opal_hash_table_t *orcm_mca_analytics_hash_table,
                                         int index, int wf_id);

/* handle the first sample data */
static orcm_mca_analytics_average_item_value* create_first_average(
                                                   orcm_analytics_value_t *current_value);

/* compute the running average: A(N+1) = (N * A(N) + new_sample) / (N + 1) */
static void compute_average(orcm_mca_analytics_average_item_value *previous_item,
                            orcm_analytics_value_t *current_value);

static void average_item_value_con(orcm_mca_analytics_average_item_value *value)
{
    value->num_sample = 0;
    value->value_average = OBJ_NEW(opal_value_t);
}

static void average_item_value_des(orcm_mca_analytics_average_item_value *value)
{
    OBJ_RELEASE(value->value_average);
}

OBJ_CLASS_INSTANCE(orcm_mca_analytics_average_item_value,
                   opal_object_t,
                   average_item_value_con, average_item_value_des);

mca_analytics_average_module_t orcm_analytics_average_module = {
    {
        init,
        finalize,
        analyze,
        NULL,
    },
};

static int init(struct orcm_analytics_base_module_t *imod)
{
    if (NULL == imod) {
        return ORCM_ERROR;
    }
    mca_analytics_average_module_t *mod;
    mod = (mca_analytics_average_module_t *)imod;
    mod->api.orcm_mca_analytics_hash_table = OBJ_NEW(opal_hash_table_t);
    opal_hash_table_init(mod->api.orcm_mca_analytics_hash_table, HASH_TABLE_SIZE);
    return ORCM_SUCCESS;
}

static void finalize(struct orcm_analytics_base_module_t *imod)
{
    if (NULL != imod) {
        mca_analytics_average_module_t *mod;
        mod = (mca_analytics_average_module_t *)imod;
        OBJ_RELEASE(mod->api.orcm_mca_analytics_hash_table);
        free(mod);
    }
}

static int analytics_average_array_create(opal_value_array_t **analytics_average_array,
                                          int size)
{
    int rc;

    /*Init the analytics average array and set its size */
    *analytics_average_array = OBJ_NEW(opal_value_array_t);

    rc = opal_value_array_init(*analytics_average_array, sizeof(orcm_analytics_value_t));
    if (OPAL_SUCCESS != rc) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    if (OPAL_SUCCESS != (rc = opal_value_array_reserve(*analytics_average_array, size))) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    return ORCM_SUCCESS;
}

static int analytics_average_array_set(opal_value_array_t *analytics_average_array,
                                       int index, char *host_name,
                                       char *sensor_name, opal_value_t *average_value)
{
    orcm_analytics_value_t analytics_average_value;
    int rc;

    /*fill the analytics structure with the sensor data */
    analytics_average_value.sensor_name = strdup(sensor_name);
    analytics_average_value.node_regex = strdup(host_name);
    memcpy(&analytics_average_value.data, average_value, sizeof(opal_value_t));

    rc = opal_value_array_set_item(analytics_average_array, index, &analytics_average_value);
    if (OPAL_SUCCESS != rc) {
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;
}

static orcm_mca_analytics_average_item_value* create_first_average(
                                                   orcm_analytics_value_t *current_value)
{
    orcm_mca_analytics_average_item_value *current_item;
    current_item = OBJ_NEW(orcm_mca_analytics_average_item_value);
    current_item->num_sample = 1;
    current_item->value_average->key = strdup(current_value->data.key);
    current_item->value_average->type = OPAL_FLOAT;
    current_item->value_average->data.fval = current_value->data.data.fval;
    return current_item;
}

static void compute_average(orcm_mca_analytics_average_item_value *previous_item,
                            orcm_analytics_value_t *current_value)
{
    unsigned int num_sample;
    num_sample = previous_item->num_sample;
    previous_item->num_sample++;
    previous_item->value_average->data.fval = (num_sample *
                     previous_item->value_average->data.fval +
                     current_value->data.data.fval) / previous_item->num_sample;
}

static opal_value_t* analyze_sample_item(orcm_analytics_value_t *current_value,
                                         opal_hash_table_t *orcm_mca_analytics_hash_table,
                                         int index, int wf_id)
{
    orcm_mca_analytics_average_item_value *current_item, *previous_item;
    char *key;
    int ret;

    if (NULL == current_value) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:the %dth data item passed by the previous workflow step "
            "is NULL", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), index));
        return NULL;
    }
    if ((ret = asprintf(&key, "%s%d%s", current_value->node_regex,
                        index, current_value->sensor_name)) < 0) {
        /* the key is the combination of node name, core id (index), and sensor name */
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:OUT OF RESOURCE", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return NULL;
    }
    ret = opal_hash_table_get_value_ptr(orcm_mca_analytics_hash_table,
                           key, strlen(key), (void **)&previous_item);
    if (OPAL_ERR_NOT_FOUND == ret) {
        /* not found this key, this is the first data */
        current_item = create_first_average(current_value);
    } else if (OPAL_SUCCESS == ret) {
        /* found this key, more data sample is coming */
        if (NULL == previous_item) {
            OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                "%s analytics:average:Hash Table getting NULL value for the key:%s",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), key));
            free(key);
            return NULL;
        }
        compute_average(previous_item, current_value);
        current_item = previous_item;
    } else {
        /* Hash table lookup failed */
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:getting item from Hash Table error, key is:%s",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), key));
        free(key);
        return NULL;
    }

#if OPAL_ENABLE_DEBUG
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
        "%s analytics:average:this is the %dth data:%f, node id:%s, core id:%d, sensor "
        "name:%s, workflow:%d, and the AVERAGE:%f", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
        current_item->num_sample, current_value->data.data.fval, current_value->node_regex,
        index, current_value->sensor_name, wf_id, current_item->value_average->data.fval));
#endif

    ret = opal_hash_table_set_value_ptr(orcm_mca_analytics_hash_table,
                                        key, strlen(key), current_item);
    free(key);
    if (OPAL_SUCCESS != ret) {
        /* Hash table insert failed */
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:inserting in Hash Table error",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return NULL;
    }
    return current_item->value_average;
}

static void analyze(int sd, short args, void *cbdata)
{
    orcm_workflow_caddy_t *current_caddy;
    orcm_analytics_value_t *current_value;
    opal_value_array_t *analytics_average_array;
    mca_analytics_average_module_t *mod;
    opal_value_t *average_opal_value;
    int index, array_size;

    if (NULL == cbdata) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:NULL caddy data passed by the previous workflow step",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
            return;
    }

    current_caddy = (orcm_workflow_caddy_t *)cbdata;
    array_size = opal_value_array_get_size(current_caddy->data);
    analytics_average_array_create(&analytics_average_array, array_size);
    mod = (mca_analytics_average_module_t *)current_caddy->imod;

    for (index = 0; index < array_size; index++) {
        current_value = (orcm_analytics_value_t*)
                            opal_value_array_get_item(current_caddy->data, index);
        average_opal_value = analyze_sample_item(current_value,
          mod->api.orcm_mca_analytics_hash_table, index, current_caddy->wf->workflow_id);
        if (NULL != average_opal_value) {
            /* fill the data for the next workflow step */
            analytics_average_array_set(analytics_average_array, index,
                current_value->node_regex, current_value->sensor_name, average_opal_value);
        }
    }

    /* if the new caddy is filled right, then go ahead to activate next workflow step */
    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(current_caddy->wf, current_caddy->wf_step,
                                     analytics_average_array);
    OBJ_RELEASE(current_caddy);
}
