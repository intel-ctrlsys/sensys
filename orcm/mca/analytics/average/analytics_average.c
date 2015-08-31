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

static int init(orcm_analytics_base_module_t *imod);
static void finalize(orcm_analytics_base_module_t *imod);
static int analyze(int sd, short args, void *cbdata);

/* analyze function for each data of the sample */
static orcm_metric_value_t* analyze_sample_item(orcm_analytics_value_t *current_value,
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
    value->value_average = OBJ_NEW(orcm_metric_value_t);
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

static int init(orcm_analytics_base_module_t *imod)
{
    if (NULL == imod) {
        return ORCM_ERROR;
    }
    mca_analytics_average_module_t *mod = (mca_analytics_average_module_t *)imod;
    mod->api.orcm_mca_analytics_hash_table = OBJ_NEW(opal_hash_table_t);
    opal_hash_table_init(mod->api.orcm_mca_analytics_hash_table, HASH_TABLE_SIZE);
    return ORCM_SUCCESS;
}

static void finalize(orcm_analytics_base_module_t *imod)
{
    if (NULL != imod) {
        mca_analytics_average_module_t *mod = (mca_analytics_average_module_t *)imod;
        OBJ_RELEASE(mod->api.orcm_mca_analytics_hash_table);
        free(mod);
    }
}

static orcm_mca_analytics_average_item_value* create_first_average(
                                                   orcm_analytics_value_t *current_value)
{
    orcm_mca_analytics_average_item_value *current_item = OBJ_NEW(
                                                  orcm_mca_analytics_average_item_value);
    if (NULL != current_item) {
        current_item->num_sample = 1;
        current_item->value_average->units = strdup(current_value->data.units);
        current_item->value_average->value.key = strdup(current_value->data.value.key);
        current_item->value_average->value.type = current_value->data.value.type;
        current_item->value_average->value.data.fval = current_value->data.value.data.fval;
    }

    return current_item;
}

static void compute_average(orcm_mca_analytics_average_item_value *previous_item,
                            orcm_analytics_value_t *current_value)
{
    unsigned int num_sample = previous_item->num_sample;
    previous_item->num_sample++;
    previous_item->value_average->value.data.fval = (num_sample *
                     previous_item->value_average->value.data.fval +
                     current_value->data.value.data.fval) / previous_item->num_sample;
}

static orcm_metric_value_t* analyze_sample_item(orcm_analytics_value_t *current_value,
                                                opal_hash_table_t *orcm_mca_analytics_hash_table,
                                                int index, int wf_id)
{
    orcm_mca_analytics_average_item_value *current_item = NULL, *previous_item = NULL;
    char *key = NULL;
    int ret = -1;

    if (NULL == current_value) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:the %dth data item passed by the previous workflow step "
            "is NULL", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), index));
        return NULL;
    }
    if (0 > (ret = asprintf(&key, "%s%d%s", current_value->node_regex,
                            index, current_value->sensor_name))) {
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
        current_item->num_sample, current_value->data.value.data.fval, current_value->node_regex,
        index, current_value->sensor_name, wf_id, current_item->value_average->value.data.fval));
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

static int analyze(int sd, short args, void *cbdata)
{
    orcm_workflow_caddy_t *current_caddy = NULL;
    orcm_analytics_value_t *current_value = NULL;
    opal_value_array_t *analytics_average_array = NULL;
    mca_analytics_average_module_t *mod = NULL;
    orcm_metric_value_t *average_metric_value = NULL;
    int index = -1, array_size = -1, rc = -1;

    if (NULL == cbdata) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:NULL caddy data passed by the previous workflow step",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
            return ORCM_ERROR;
    }

    current_caddy = (orcm_workflow_caddy_t *)cbdata;
    array_size = opal_value_array_get_size(current_caddy->data);
    rc = orcm_analytics.array_create(&analytics_average_array, array_size);
    if (ORCM_SUCCESS != rc) {
            return ORCM_ERR_OUT_OF_RESOURCE;
    }
    mod = (mca_analytics_average_module_t *)current_caddy->imod;

    for (index = 0; index < array_size; index++) {
        current_value = (orcm_analytics_value_t*)
                            opal_value_array_get_item(current_caddy->data, index);
        average_metric_value = analyze_sample_item(current_value,
          mod->api.orcm_mca_analytics_hash_table, index, current_caddy->wf->workflow_id);
        if (NULL != average_metric_value) {
            /* fill the data for the next workflow step */
            orcm_analytics.array_append(analytics_average_array, index,
                                        current_value->sensor_name,
                                        current_value->node_regex,
                                        average_metric_value);
        }
    }

    /* load data to database if needed */
    orcm_analytics_base_store(current_caddy->wf,
                              current_caddy->wf_step, analytics_average_array);

    /* if the new caddy is filled right, then go ahead to activate next workflow step */
    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(current_caddy->wf, current_caddy->wf_step,
                                     analytics_average_array);
    OBJ_RELEASE(current_caddy);

    return ORCM_SUCCESS;
}
