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
static char *generate_data_key(char *operation, int workflow_id, char* hostname);
static void compute_min(orcm_value_t* agg_value, orcm_analytics_aggregate* aggregate, opal_list_t* compute);
static void compute_max(orcm_value_t* agg_value, orcm_analytics_aggregate* aggregate, opal_list_t* compute);
static void compute_average(orcm_value_t* agg_value, orcm_analytics_aggregate* aggregate, opal_list_t* compute);
static orcm_value_t* compute_agg(char* op, char* data_key, opal_list_t* compute, orcm_analytics_aggregate* aggregate);
static char* get_operation_name(opal_list_t* attributes);

#define CHECK_NULL(x, e, label)  if(NULL==x) { e=ORCM_ERR_BAD_PARAM; goto label; }
#define ON_NULL_RETURN(x) if(NULL==x) { return NULL; }

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
        NULL
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
        SAFEFREE(mod);
    }
}

static char* get_operation_name(opal_list_t* attributes)
{
    opal_value_t* temp = NULL;
    char* op = NULL;

    ON_NULL_RETURN(attributes);
    temp = (opal_value_t*)opal_list_get_first(attributes);
    ON_NULL_RETURN(temp);
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

static char *generate_data_key(char *operation, int workflow_id, char* hostname)
{
    char *data_key = NULL;
    asprintf(&data_key, "Aggregate:%s_%s_Workflow %d", operation, hostname, workflow_id);
    return data_key;
}

static orcm_value_t* compute_agg(char* op, char* data_key, opal_list_t* compute, orcm_analytics_aggregate* aggregate)
{
    orcm_value_t *temp = NULL;
    orcm_value_t *agg_value = NULL;
    if(NULL == compute || NULL == aggregate || NULL == data_key) {
        return NULL;
    }
    temp = (orcm_value_t*)opal_list_get_first(compute);
    ON_NULL_RETURN(temp);
    agg_value = orcm_util_load_orcm_value(data_key, &temp->value.data,OPAL_DOUBLE,temp->units);
    ON_NULL_RETURN(agg_value);
    if(0 == strncmp(op,"average", strlen(op))) {
        compute_average(agg_value, aggregate, compute);
    }
    else if (0 == strncmp(op, "min", strlen(op))){
        compute_min(agg_value, aggregate, compute);
    }
    else if (0 == strncmp(op,"max", strlen(op))){
        compute_max(agg_value, aggregate, compute);
    } else {
        SAFEFREE(agg_value);
    }
    return agg_value;
}

static void compute_average(orcm_value_t* agg_value, orcm_analytics_aggregate* aggregate, opal_list_t* compute)
{
    double sum = 0.0;
    orcm_value_t *list_item = NULL;
    size_t size = opal_list_get_size(compute);

    OPAL_LIST_FOREACH(list_item, compute, orcm_value_t) {
        sum += orcm_util_get_number_orcm_value(list_item);
    }
    agg_value->value.data.dval = (aggregate->average * aggregate->num_sample + sum) /
                                     (aggregate->num_sample + size);
    aggregate->average = agg_value->value.data.dval;
    aggregate->num_sample += size;

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output, "%s %s is: %f, and the number of sample is:%u",
        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), agg_value->value.key, aggregate->average,aggregate->num_sample));
}

static void compute_min(orcm_value_t* agg_value, orcm_analytics_aggregate* aggregate, opal_list_t* compute)
{
    double val = 0.0;
    orcm_value_t *current_value = NULL;
    agg_value->value.data.dval = aggregate->min;
    OPAL_LIST_FOREACH(current_value, compute, orcm_value_t)
    {
        val = orcm_util_get_number_orcm_value(current_value);
        if (val < aggregate->min) {
            aggregate->min = val;
            agg_value->value.data.dval = aggregate->min;
        }
    }
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output, "%s %s is: %f %s",
        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), agg_value->value.key, agg_value->value.data.dval,agg_value->units));
}

static void compute_max(orcm_value_t* agg_value, orcm_analytics_aggregate* aggregate, opal_list_t* compute)
{
    orcm_value_t *current_value = NULL;
    double val = 0.0;
    agg_value->value.data.dval = aggregate->max;
    OPAL_LIST_FOREACH(current_value, compute, orcm_value_t) {
        val = orcm_util_get_number_orcm_value(current_value);
        if(val > aggregate->max) {
            aggregate->max = val;
            agg_value->value.data.dval = aggregate->max;
        }
    }
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output, "%s %s is: %f %s",
        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), agg_value->value.key, agg_value->value.data.dval,agg_value->units));
}

static int analyze(int sd, short args, void *cbdata)
{
    orcm_workflow_caddy_t *current_caddy = (orcm_workflow_caddy_t *)cbdata;
    orcm_value_t *aggregate_value = NULL;
    orcm_analytics_value_t *analytics_value_to_next = NULL;
    mca_analytics_aggregate_module_t *mod = NULL;
    opal_list_t* aggregate_list = NULL;
    char* data_key = NULL;
    char* operation = NULL;
    int rc = ORCM_SUCCESS;

    if (ORCM_SUCCESS != (rc = orcm_analytics_base_assert_caddy_data(cbdata))) {
        goto cleanup;
    }
    if(0 == current_caddy->analytics_value->compute_data->opal_list_length) {
        goto cleanup;
    }
    mod = (mca_analytics_aggregate_module_t *)current_caddy->imod;

    operation = get_operation_name(&current_caddy->wf_step->attributes);
    CHECK_NULL(operation, rc, cleanup);
    data_key = generate_data_key(operation, current_caddy->wf->workflow_id, current_caddy->wf->hostname_regex);
    CHECK_NULL_ALLOC(data_key, rc, cleanup);

    aggregate_value = compute_agg(operation, data_key, current_caddy->analytics_value->compute_data,
                                 (orcm_analytics_aggregate *)mod->api.orcm_mca_analytics_data_store);
    CHECK_NULL(aggregate_value, rc, cleanup);
    aggregate_list = OBJ_NEW(opal_list_t);
    CHECK_NULL_ALLOC(aggregate_list, rc, cleanup);

    opal_list_append(aggregate_list, (opal_list_item_t *)aggregate_value);
    analytics_value_to_next = orcm_util_load_analytics_time_compute(current_caddy->analytics_value->key,
                                              current_caddy->analytics_value->non_compute_data, aggregate_list);
    CHECK_NULL(analytics_value_to_next, rc, cleanup);
    if(true == orcm_analytics_base_db_check(current_caddy->wf_step)){
        rc = orcm_analytics_base_log_to_database_event(analytics_value_to_next);
        if(ORCM_SUCCESS != rc){
            goto cleanup;
        }
    }
    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(current_caddy->wf, current_caddy->wf_step,
                                     current_caddy->hash_key, analytics_value_to_next, NULL);
cleanup:
    SAFEFREE(operation);
    SAFEFREE(data_key);
    if(ORCM_SUCCESS != rc){
        SAFE_RELEASE(aggregate_value);
        SAFE_RELEASE(aggregate_list);
        SAFE_RELEASE(analytics_value_to_next);
    }
    SAFE_RELEASE(current_caddy);
    return rc;
}
