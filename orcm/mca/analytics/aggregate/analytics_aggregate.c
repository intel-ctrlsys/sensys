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
#include <float.h>

#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"
#include "orcm/util/utils.h"

#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/mca/analytics/aggregate/analytics_aggregate.h"


static int init(orcm_analytics_base_module_t *imod);
static void finalize(orcm_analytics_base_module_t *imod);
static int analyze(int sd, short args, void *cbdata);
static orcm_value_t *compute_min(opal_list_t *compute_data, orcm_analytics_aggregate *aggregate);
static orcm_value_t *compute_max(opal_list_t *compute_data, orcm_analytics_aggregate *aggregate);
static orcm_value_t *compute_average(opal_list_t *compute_data, orcm_analytics_aggregate *aggregate);
static char* get_operation_name(opal_list_t* attributes);

static void orcm_analytics_aggregate_con(orcm_analytics_aggregate *value)
{
    value->average = 0.0;
    value->max = -DBL_MAX;
    value->min = DBL_MAX;
    value->num_sample = 0;
}

static void orcm_analytics_aggregate_des(orcm_analytics_aggregate *value)
{
    return;
}
OBJ_CLASS_INSTANCE(orcm_analytics_aggregate,
                   opal_object_t,
                   orcm_analytics_aggregate_con, orcm_analytics_aggregate_des);

mca_analytics_aggregate_module_t orcm_analytics_aggregate_module = {
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
        return ORCM_ERR_BAD_PARAM;
    }
    mca_analytics_aggregate_module_t *mod = (mca_analytics_aggregate_module_t *)imod;
    mod->api.orcm_mca_analytics_data_store = OBJ_NEW(orcm_analytics_aggregate);
    if (NULL == mod->api.orcm_mca_analytics_data_store){
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    return ORCM_SUCCESS;
}

static void finalize(orcm_analytics_base_module_t *imod)
{
    if (NULL != imod) {
        mca_analytics_aggregate_module_t *mod = (mca_analytics_aggregate_module_t *)imod;
        OBJ_RELEASE(mod->api.orcm_mca_analytics_data_store);
        free(mod);
    }
}

static char* get_operation_name(opal_list_t* attributes)
{
    opal_value_t* temp = NULL;
    char* op = NULL;
    if(NULL == attributes) {
        return NULL;
    }
    temp = (opal_value_t*)opal_list_get_first(attributes);
    if(NULL == temp) {
        return NULL;
    }
    if (0 == strcmp(temp->key,"operation")) {
        if(NULL != temp->data.string) {
            op = strdup(temp->data.string);
        }
        return op;
    }
    else {
        return NULL;
    }
}

static orcm_value_t *compute_average(opal_list_t *compute_data, orcm_analytics_aggregate* aggregate)
{
    double sum = 0.0;
    orcm_value_t *temp = NULL;
    orcm_value_t *aggregate_value = NULL;
    orcm_value_t *list_item = NULL;

    if (NULL == compute_data || NULL == aggregate) {
            return NULL;
    }
    size_t size = opal_list_get_size(compute_data);
    temp = (orcm_value_t*)opal_list_get_first(compute_data);
    if(NULL != temp) {
        aggregate_value = orcm_util_load_orcm_value("Aggregate:average", &temp->value.data,OPAL_DOUBLE,temp->units);
    }
    if(NULL == aggregate_value) {
        return NULL;
    }
    OPAL_LIST_FOREACH(list_item, compute_data, orcm_value_t) {
        if (NULL == list_item) {
            return NULL;
        }
        sum += orcm_util_get_number_orcm_value(list_item);
    }
    aggregate_value->value.data.dval = (aggregate->average * aggregate->num_sample + sum) /
                                     (aggregate->num_sample + size);
    aggregate->average = aggregate_value->value.data.dval;
    aggregate->num_sample += size;

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                         "%s analytics:aggregate:AVERAGE is: %f, and the number of sample is:%u",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), aggregate->average,aggregate->num_sample));
    return aggregate_value;
}

static orcm_value_t* compute_min(opal_list_t* compute, orcm_analytics_aggregate* aggregate)
{
    orcm_value_t *current_value = NULL;
    orcm_value_t *temp = NULL;
    orcm_value_t *min_value = NULL;
    double val;
    if(NULL == compute || NULL == aggregate) {
        return NULL;
    }
    temp = (orcm_value_t*)opal_list_get_first(compute);
    if(NULL != temp) {
        min_value = orcm_util_load_orcm_value("Aggregate:MIN", &temp->value.data,OPAL_DOUBLE,temp->units);
    }
    if(NULL == min_value){
        return NULL;
    }
    min_value->value.data.dval = aggregate->min;
    OPAL_LIST_FOREACH(current_value, compute, orcm_value_t) {
        val = orcm_util_get_number_orcm_value(current_value);
        if(val < aggregate->min) {
            aggregate->min = val;
            min_value->value.data.dval = aggregate->min;
        }
    }
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:aggregate:MIN is: %f %s",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), min_value->value.data.dval,min_value->units));
    return min_value;
}

static orcm_value_t* compute_max(opal_list_t* compute, orcm_analytics_aggregate* aggregate)
{
    orcm_value_t *current_value = NULL;
    orcm_value_t *temp = NULL;
    orcm_value_t *max_value = NULL;
    double val;
    if(NULL == compute || NULL == aggregate) {
        return NULL;
    }
    temp = (orcm_value_t*)opal_list_get_first(compute);
    if(NULL != temp) {
        max_value = orcm_util_load_orcm_value("Aggregate:MAX", &temp->value.data,OPAL_DOUBLE,temp->units);
    }
    if(NULL == max_value){
        return NULL;
    }
    max_value->value.data.dval = aggregate->max;
    OPAL_LIST_FOREACH(current_value, compute, orcm_value_t) {
        val = orcm_util_get_number_orcm_value(current_value);
        if(val > aggregate->max) {
            aggregate->max = val;
            max_value->value.data.dval = aggregate->max;
        }
    }
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:aggregate:MAX is: %f %s",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), max_value->value.data.dval,max_value->units));
    return max_value;
}

static int analyze(int sd, short args, void *cbdata)
{
    orcm_workflow_caddy_t *current_caddy = (orcm_workflow_caddy_t *)cbdata;
    orcm_value_t *aggregate_value = NULL;
    orcm_analytics_value_t *analytics_value_to_next = NULL;
    mca_analytics_aggregate_module_t *mod = NULL;
    opal_list_t* aggregate_list = NULL;
    char* operation = NULL;
    int rc = ORCM_SUCCESS;

    if (ORCM_SUCCESS != (rc = orcm_analytics_base_assert_caddy_data(cbdata))) {
        goto cleanup;
    }
    mod = (mca_analytics_aggregate_module_t *)current_caddy->imod;
    operation = get_operation_name(&current_caddy->wf_step->attributes);
    if(NULL == operation) {
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup;
    }
    if(0 == strcmp(operation,"average")) {
        aggregate_value = compute_average(current_caddy->analytics_value->compute_data,
                                       (orcm_analytics_aggregate *)mod->api.orcm_mca_analytics_data_store);
    }
    else if (0 == strcmp(operation, "min")){
        aggregate_value = compute_min(current_caddy->analytics_value->compute_data,
                              (orcm_analytics_aggregate*)mod->api.orcm_mca_analytics_data_store);
    }
    else if (0 == strcmp(operation,"max")){
        aggregate_value = compute_max(current_caddy->analytics_value->compute_data,
                              (orcm_analytics_aggregate*)mod->api.orcm_mca_analytics_data_store);
    }
    else {
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup;
    }
    if(NULL == aggregate_value) {
        rc = ORCM_ERROR;
        goto cleanup;
    }
    aggregate_list = OBJ_NEW(opal_list_t);
    if(NULL == aggregate_list) {
        rc = ORCM_ERR_OUT_OF_RESOURCE;
        goto cleanup;
    }
    opal_list_append(aggregate_list, (opal_list_item_t *)aggregate_value);
    analytics_value_to_next = orcm_util_load_orcm_analytics_value_compute(current_caddy->analytics_value->key,
                                              current_caddy->analytics_value->non_compute_data, aggregate_list);
    if (NULL == analytics_value_to_next) {
        rc = ORCM_ERROR;
        goto cleanup;
    }

    rc = orcm_analytics_base_log_to_database_event(analytics_value_to_next);
    if(ORCM_SUCCESS != rc){
        rc = ORCM_ERROR;
        goto cleanup;
    }
    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(current_caddy->wf, current_caddy->wf_step,
                                     current_caddy->hash_key, analytics_value_to_next);
cleanup:
    SAFEFREE(operation);
    if (NULL != current_caddy) {
        OBJ_RELEASE(current_caddy);
    }
    return rc;
}
