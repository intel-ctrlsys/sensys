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
#include "orcm/util/utils.h"

#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/mca/analytics/average/analytics_average.h"

#define HASH_TABLE_SIZE 10000

static int init(orcm_analytics_base_module_t *imod);
static void finalize(orcm_analytics_base_module_t *imod);
static int analyze(int sd, short args, void *cbdata);

/* analyze function for each data of the sample */
static orcm_value_t* analyze_sample_item(orcm_value_t *current_value, char *key,
                                                opal_hash_table_t *orcm_mca_analytics_hash_table);

/* handle the first sample data */
static orcm_mca_analytics_average_item_value* create_first_average(orcm_value_t *current_value);

/* compute the running average: A(N+1) = (N * A(N) + new_sample) / (N + 1) */
static void compute_average(orcm_mca_analytics_average_item_value *previous_item,
                            orcm_value_t *current_value);

static void average_item_value_con(orcm_mca_analytics_average_item_value *value)
{
    value->num_sample = 0;
    value->value_average = OBJ_NEW(orcm_value_t);
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

static orcm_mca_analytics_average_item_value* create_first_average(orcm_value_t *current_value)
{
    orcm_mca_analytics_average_item_value *current_item = OBJ_NEW(
                                                  orcm_mca_analytics_average_item_value);
    if (NULL != current_item) {
        current_item->num_sample = 1;
        current_item->value_average->units = strdup(current_value->units);
        current_item->value_average->value.key = strdup(current_value->value.key);
        current_item->value_average->value.type = OPAL_FLOAT;
        current_item->value_average->value.data.fval = current_value->value.data.fval;
    }

    return current_item;
}

static void compute_average(orcm_mca_analytics_average_item_value *previous_item,
                            orcm_value_t *current_value)
{
    unsigned int num_sample = previous_item->num_sample;
    previous_item->num_sample++;
    previous_item->value_average->value.data.fval = (num_sample *
                     previous_item->value_average->value.data.fval +
                     current_value->value.data.fval) / previous_item->num_sample;
}

static orcm_value_t* analyze_sample_item(orcm_value_t *current_value, char *key,
                                                opal_hash_table_t *orcm_mca_analytics_hash_table)
{
    orcm_mca_analytics_average_item_value *average_item = NULL;
    orcm_value_t *average_orcm_value = NULL;

    int ret = -1;

    if (NULL == current_value) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:the data item passed by the workflow step "
            "is NULL", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return NULL;
    }
    ret = opal_hash_table_get_value_ptr(orcm_mca_analytics_hash_table,
                           key, strlen(key), (void **)&average_item);
    if (OPAL_ERR_NOT_FOUND == ret) {
        /* not found this key, this is the first data */
        average_item = create_first_average(current_value);
    } else if (OPAL_SUCCESS == ret) {
        /* found this key, more data sample is coming */
        if (NULL == average_item) {
            OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                "%s analytics:average:Hash Table getting NULL value for the key:%s",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), key));
            return NULL;
        }
        compute_average(average_item, current_value);
    } else {
        /* Hash table lookup failed */
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:getting item from Hash Table error, key is:%s",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), key));
        return NULL;
    }

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                            "%s analytics: key is %s", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            average_item->value_average->value.key));

    ret = opal_hash_table_set_value_ptr(orcm_mca_analytics_hash_table,
                                        key, strlen(key), (void *) average_item);
    if (OPAL_SUCCESS != ret) {
        /* Hash table insert failed */
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:inserting in Hash Table error",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return NULL;
    }
    /*orcm_metric_value returned by this function will be freed. So, a copy of value
     * stored in hashtable is provided*/
    average_orcm_value = orcm_util_copy_orcm_value(average_item->value_average);
    return average_orcm_value;
}

static int analyze(int sd, short args, void *cbdata)
{
    orcm_workflow_caddy_t *current_caddy = NULL;
    orcm_value_t *current_value = NULL;
    opal_value_t *analytics_opal_value = NULL;
    opal_list_t *analytics_average_list = NULL;
    mca_analytics_average_module_t *mod = NULL;
    orcm_value_t *average_metric_value = NULL;
    opal_list_t *sample_data_list = NULL;
    int rc = -1;
    int index = 0;
    const int NUM_PARAMS = 3;
    const char *params[] = {
        "data_group",
        "hostname",
        "ctime"
    };
    opal_value_t *param_items[] = {NULL, NULL, NULL};
    opal_bitmap_t item_bm;
    char *key = NULL;
    int num_items;

    if (NULL == cbdata) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:NULL caddy data passed by the previous workflow step",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
            return ORCM_ERROR;
    }
    current_caddy = (orcm_workflow_caddy_t *)cbdata;

    analytics_average_list= OBJ_NEW(opal_list_t);
    if (NULL == analytics_average_list) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    mod = (mca_analytics_average_module_t *)current_caddy->imod;

    sample_data_list = current_caddy->data;
    if (NULL == sample_data_list) {
        return ORCM_ERROR;
    }

    /* Get the main parameters form the list */
    num_items = opal_list_get_size(sample_data_list);
    OBJ_CONSTRUCT(&item_bm, opal_bitmap_t);
    opal_bitmap_init(&item_bm, (int)num_items);
    orcm_util_find_items(params, NUM_PARAMS, sample_data_list, param_items, &item_bm);

    OPAL_LIST_FOREACH(current_value, sample_data_list, orcm_value_t) {
        /* Ignore the items that have already been processed */
        if (opal_bitmap_is_set_bit(&item_bm, index)) {
            index++;
            analytics_opal_value = orcm_util_copy_opal_value((opal_value_t *)current_value);
            opal_list_append(analytics_average_list, &analytics_opal_value->super);
            continue;
        }
        if (0 > (rc = asprintf(&key, "%s%d%s", param_items[1]->data.string, index,
                                param_items[0]->data.string))) {
            /* the key is the combination of node name, core id (index), and sensor name */
            OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                "%s analytics:average:OUT OF RESOURCE", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
            return ORCM_ERROR;
        }
        average_metric_value = analyze_sample_item(current_value, key,
                                                   mod->api.orcm_mca_analytics_hash_table);
        if (NULL != average_metric_value) {
            opal_list_append(analytics_average_list, (opal_list_item_t *)average_metric_value);
        }
        free(key);
        key = NULL;
        index++;
    }


    /* load data to database if needed */
    if (true == orcm_analytics_base_db_check(current_caddy->wf_step)) {
        OBJ_RETAIN(analytics_average_list);
        rc = orcm_analytics_base_store(analytics_average_list);
        if (ORCM_SUCCESS != rc) {
             OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                                  "%s analytics:average:Data can't be written to DB",
                                  ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
             OPAL_LIST_RELEASE(analytics_average_list);
         }
    }


    /* if the new caddy is filled right, then go ahead to activate next workflow step */
    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(current_caddy->wf, current_caddy->wf_step,
                                     analytics_average_list);
    OBJ_RELEASE(current_caddy);
    OBJ_DESTRUCT(&item_bm);

    return ORCM_SUCCESS;
}
