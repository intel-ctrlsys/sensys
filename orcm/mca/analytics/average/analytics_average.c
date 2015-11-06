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

/* create the average value for the first sample data*/
static orcm_analytics_average_item_value* create_first_value(orcm_value_t *list_item);

/* computing the running average: A(N+1) = (N * A(N) + new_sample) / (N + 1) */
static int computing_the_average(orcm_analytics_average_item_value *new_average_value,
                                 opal_list_t *compute_data);

/* an internal function to compute the average*/
static
orcm_analytics_average_item_value *compute_average_internal(opal_list_t *compute_data,
                                   orcm_analytics_average_item_value *prev_average_value);

/* compute the average of the sample data */
static orcm_analytics_average_item_value *compute_average(opal_hash_table_t *hash_table,
                                                          uint64_t hash_key,
                                                          opal_list_t *compute_data);

/* fill the computation data list to be passed to the next plugin */
static
orcm_analytics_value_t *fill_analytics_value(orcm_analytics_average_item_value *average_value,
                                             orcm_analytics_value_t *analytics_value);

static void average_item_value_con(orcm_analytics_average_item_value *value)
{
    value->num_sample = 0;
    value->value_average = NULL;
}

static void average_item_value_des(orcm_analytics_average_item_value *value)
{
    if (NULL != value->value_average) {
        OBJ_RELEASE(value->value_average);
    }
}

OBJ_CLASS_INSTANCE(orcm_analytics_average_item_value,
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
        return ORCM_ERR_BAD_PARAM;
    }

    mca_analytics_average_module_t *mod = (mca_analytics_average_module_t *)imod;
    mod->api.orcm_mca_analytics_data_store = OBJ_NEW(opal_hash_table_t);
    if (NULL == mod->api.orcm_mca_analytics_data_store) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    return opal_hash_table_init((opal_hash_table_t*)(mod->api.orcm_mca_analytics_data_store),
                                HASH_TABLE_SIZE);
}

static void finalize(orcm_analytics_base_module_t *imod)
{
    if (NULL != imod) {
        mca_analytics_average_module_t *mod = (mca_analytics_average_module_t *)imod;
        opal_hash_table_t *average_hash_table = (opal_hash_table_t *)
                                                mod->api.orcm_mca_analytics_data_store;
        if (NULL != average_hash_table) {
            OBJ_RELEASE(average_hash_table);
        }
        free(mod);
    }
}

static orcm_analytics_average_item_value* create_first_value(orcm_value_t *list_item)
{
    orcm_analytics_average_item_value *current_item = NULL;

    if (NULL == list_item) {
        return NULL;
    }

    current_item = OBJ_NEW(orcm_analytics_average_item_value);
    if (NULL != current_item) {
        current_item->num_sample = 0;
        current_item->value_average = OBJ_NEW(orcm_value_t);
        if (NULL == current_item->value_average) {
            OBJ_RELEASE(current_item);
            return NULL;
        }
        if (NULL != list_item->units) {
            current_item->value_average->units = strdup(list_item->units);
        }
        if (NULL != list_item->value.key) {
            current_item->value_average->value.key = strdup(list_item->value.key);
        }
        current_item->value_average->value.type = OPAL_DOUBLE;
        current_item->value_average->value.data.dval = 0.0;
    }

    return current_item;
}

static int computing_the_average(orcm_analytics_average_item_value *new_average_value,
                                 opal_list_t *compute_data)
{
    double sum = 0.0;
    orcm_value_t *list_item = NULL;
    size_t size = opal_list_get_size(compute_data);

    OPAL_LIST_FOREACH(list_item, compute_data, orcm_value_t) {
        if (NULL == list_item) {
            return ORCM_ERR_BAD_PARAM;
        }
        sum += orcm_util_get_number_orcm_value(list_item);
    }

    new_average_value->value_average->value.data.dval =
       (new_average_value->value_average->value.data.dval * new_average_value->num_sample + sum) /
       (new_average_value->num_sample + size);
    new_average_value->num_sample += size;

    return ORCM_SUCCESS;
}

static
orcm_analytics_average_item_value *compute_average_internal(opal_list_t *compute_data,
                                   orcm_analytics_average_item_value *prev_average_value)
{
    orcm_analytics_average_item_value *new_average_value = NULL;

    if (NULL == compute_data) {
        return NULL;
    }

    if (NULL == prev_average_value) {
        new_average_value = create_first_value((orcm_value_t*)opal_list_get_first(compute_data));
        if (NULL == new_average_value) {
            return NULL;
        }
    } else {
        new_average_value = prev_average_value;
    }

    if (ORCM_SUCCESS != computing_the_average(new_average_value, compute_data)) {
        goto cleanup;
    }

    return new_average_value;

cleanup:
    if (NULL != new_average_value) {
        OBJ_RELEASE(new_average_value);
    }
    return NULL;
}

static orcm_analytics_average_item_value *compute_average(opal_hash_table_t *hash_table,
                                                          uint64_t hash_key,
                                                          opal_list_t *compute_data)
{
    int rc = -1;
    orcm_analytics_average_item_value *prev_average_value = NULL, *new_average_value = NULL;

    rc = opal_hash_table_get_value_uint64(hash_table, hash_key, (void**)&prev_average_value);
    if (OPAL_ERR_NOT_FOUND == rc || OPAL_SUCCESS == rc) {
        new_average_value = compute_average_internal(compute_data, prev_average_value);
    }

    if (NULL != new_average_value) {
        rc = opal_hash_table_set_value_uint64(hash_table, hash_key, new_average_value);
        if (OPAL_SUCCESS != rc) {
            goto cleanup;
        }
        return new_average_value;
    }

cleanup:
    if (NULL != new_average_value && NULL == prev_average_value) {
        OBJ_RELEASE(new_average_value);
    }
    return NULL;
}

static
orcm_analytics_value_t *fill_analytics_value(orcm_analytics_average_item_value *average_value,
                                             orcm_analytics_value_t *analytics_value)
{
    opal_list_t *compute_list_to_next = NULL;
    orcm_value_t *compute_list_item = NULL;
    orcm_analytics_value_t *analytics_value_to_next = NULL;

    compute_list_to_next = OBJ_NEW(opal_list_t);
    if (NULL == compute_list_to_next) {
        return NULL;
    }
    compute_list_item = orcm_util_load_orcm_value(average_value->value_average->value.key,
                                                  &(average_value->value_average->value.data),
                                                  average_value->value_average->value.type,
                                                  average_value->value_average->units);
    if (NULL == compute_list_item) {
        OPAL_LIST_RELEASE(compute_list_to_next);
        return NULL;
    }
    opal_list_append(compute_list_to_next, (opal_list_item_t *)compute_list_item);
    analytics_value_to_next = orcm_util_load_orcm_analytics_value_compute(analytics_value->key,
                                  analytics_value->non_compute_data, compute_list_to_next);

    return analytics_value_to_next;
}

static int analyze(int sd, short args, void *cbdata)
{
    orcm_workflow_caddy_t *current_caddy = (orcm_workflow_caddy_t *)cbdata;
    orcm_analytics_average_item_value *average_value = NULL;
    orcm_analytics_value_t *analytics_value_to_next = NULL;
    mca_analytics_average_module_t *mod = NULL;
    int rc = -1;

    if (ORCM_SUCCESS != (rc = orcm_analytics_base_assert_caddy_data(cbdata))) {
        goto cleanup;
    }
    mod = (mca_analytics_average_module_t *)current_caddy->imod;

    average_value = compute_average((opal_hash_table_t*)mod->api.orcm_mca_analytics_data_store,
                                    current_caddy->hash_key,
                                    current_caddy->analytics_value->compute_data);
    if (NULL == average_value) {
        rc = ORCM_ERROR;
        goto cleanup;
    }

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                         "%s analytics:average:AVERAGE is: %f %s, and the number of sample is:%u",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), average_value->value_average->value.data.dval,
                         average_value->value_average->units, average_value->num_sample));

    analytics_value_to_next = fill_analytics_value(average_value, current_caddy->analytics_value);
    if (NULL == analytics_value_to_next) {
        rc = ORCM_ERROR;
        goto cleanup;
    }

    /* if the new caddy is filled right, then go ahead to activate next workflow step */
    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(current_caddy->wf, current_caddy->wf_step,
                                     current_caddy->hash_key, analytics_value_to_next);
cleanup:
    if (NULL != current_caddy) {
        OBJ_RELEASE(current_caddy);
    }
    return rc;
}
