/*
 * Copyright (c) 2014-2016 Intel, Inc. All rights reserved.
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
#include "orcm/mca/analytics/filter/analytics_filter.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"
#include "orcm/util/logical_group.h"

#define HASH_KEY_LENGTH 1000

static int init(orcm_analytics_base_module_t *imod);
static void finalize(orcm_analytics_base_module_t *imod);
static int analyze(int sd, short args, void *cbdata);

mca_analytics_filter_module_t orcm_analytics_filter_module = {{init, finalize, analyze, NULL, NULL}};

/* given a target, find it in the source candidate */
static int find_match(char *target, char ** candidate);

/* match the attribute to a key from the key list */
static int filter_match_value(char* key_value, char* attribute_value);

/* match the attribute to the compute data list,
 * this only happens when the attribute does not match the key list */
static int filter_match_compute(orcm_value_t *attribute, opal_list_t *compute_data,
                                opal_list_t *filter_list);

/* filter the data and fill the hash_key is data matches */
static int analytics_filter_data(opal_list_t *attributes, orcm_analytics_value_t *filter_data,
                                 opal_list_t **filter_list);

/* generate the hash key for the filter attribute list */
static int generate_hash_key(opal_list_t *attributes, int wf_id, filter_key_t *filter_key);

/* generate the data to be passed to the next plugin */
static orcm_analytics_value_t *generate_data_to_next(orcm_analytics_value_t *filter_data,
                                                     opal_list_t *filter_list);

static int init(orcm_analytics_base_module_t *imod)
{
    mca_analytics_filter_module_t *mod = NULL;
    filter_key_t *filter_key = NULL;
    if (NULL == (mod = (mca_analytics_filter_module_t*)imod)) {
        return ORCM_ERR_BAD_PARAM;
    }
    if (NULL == (filter_key = (filter_key_t*)malloc(sizeof(filter_key_t)))) {
        return ORCM_ERR_BAD_PARAM;
    }
    filter_key->unique_id = 0;
    filter_key->unique_id_generated = false;
    mod->api.orcm_mca_analytics_data_store = filter_key;
    return ORCM_SUCCESS;
}

static void finalize(orcm_analytics_base_module_t *imod)
{
    mca_analytics_filter_module_t *mod = (mca_analytics_filter_module_t*)imod;
    if (NULL != mod) {
        SAFEFREE(mod->api.orcm_mca_analytics_data_store);
        SAFEFREE(mod);
    }
}

static int find_match(char *target, char ** candidate)
{
    int index = 0, count = opal_argv_count(candidate);

    for (index = 0; index < count; index++) {
        if(0 == strncmp(target, candidate[index], strlen(target) + 1)) {
            opal_output_verbose(5, orcm_analytics_base_framework.framework_output,
                "%s analytics:Filtered Value:%s", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), target);
            return ORCM_SUCCESS;
        }
    }

    return ORCM_ERR_NOT_FOUND;
}

static int filter_match_value(char* key_value, char* attribute_value)
{
    int rc = -1;
    char** source_value = NULL;

    rc = orcm_logical_group_parse_array_string(attribute_value, &source_value);
    if(ORCM_SUCCESS != rc) {
        goto cleanup;
    }

    rc = find_match(key_value, source_value);

cleanup:
    opal_argv_free(source_value);
    return rc;
}

static int filter_match_compute(orcm_value_t *attribute,
                                opal_list_t *compute_data, opal_list_t *filter_list)
{
    int rc = -1;
    orcm_value_t *compute_item = NULL;
    orcm_value_t *compute_item_copy = NULL;
    char** wf_attri_value = NULL;
    bool found_data = false;

    rc = orcm_logical_group_parse_array_string(attribute->value.data.string, &wf_attri_value);
    if (ORCM_SUCCESS != rc) {
        goto cleanup;
    }

    if (NULL == compute_data) {
        rc = ORCM_ERROR;
        goto cleanup;
    }
    OPAL_LIST_FOREACH(compute_item, compute_data, orcm_value_t) {
        rc = find_match(compute_item->value.key, wf_attri_value);
        if (ORCM_SUCCESS == rc) {
            found_data = true;
            if (NULL == (compute_item_copy = orcm_util_copy_orcm_value(compute_item))) {
                rc = ORCM_ERR_OUT_OF_RESOURCE;
                goto cleanup;
            }
            opal_list_append(filter_list, (opal_list_item_t *)compute_item_copy);
        } else if (found_data) {
            rc = ORCM_SUCCESS;
        }
    }

cleanup:
    opal_argv_free(wf_attri_value);
    return rc;
}

static int analytics_filter_data(opal_list_t *attributes, orcm_analytics_value_t *filter_data,
                                 opal_list_t **filter_list)
{
    orcm_value_t *attribute = NULL, *key_value = NULL;
    bool found = false;
    int rc = ORCM_ERROR;

    OPAL_LIST_FOREACH(attribute, attributes, orcm_value_t) {
        if(0 == strcmp(attribute->value.key,"db")){
            continue;
        }
        found = false;
        OPAL_LIST_FOREACH(key_value, filter_data->key, orcm_value_t) {
            if (0 == (strncmp(key_value->value.key, attribute->value.key,
                      strlen(key_value->value.key)))){
                if (ORCM_SUCCESS != (rc = filter_match_value(key_value->value.data.string,
                                     attribute->value.data.string))) {
                    return rc;
                }
                found = true;
                break;
            }
        }
        if(!found) {
            if (NULL == *filter_list && NULL == (*filter_list = OBJ_NEW(opal_list_t))) {
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            if (ORCM_SUCCESS != (rc = filter_match_compute(attribute,
                                 filter_data->compute_data, *filter_list))) {
                return rc;
            }
            found = true;
        }
    }

    if(!found) {
       return ORCM_ERR_NOT_FOUND;
    }

    return ORCM_SUCCESS;
}

static int generate_hash_key(opal_list_t *attributes, int wf_id, filter_key_t *filter_key)
{
    char *hash_key = NULL;
    char *wf_id_str = NULL;
    orcm_value_t *attribute = NULL;
    int rc = ORCM_SUCCESS;

    if (NULL == (hash_key = (char*)calloc(HASH_KEY_LENGTH, sizeof(char)))) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    OPAL_LIST_FOREACH(attribute, attributes, orcm_value_t) {
        strncat(hash_key, attribute->value.data.string, strlen(attribute->value.data.string));
    }

    if (-1 == (asprintf(&wf_id_str, "%d", wf_id))) {
        rc = ORCM_ERR_OUT_OF_RESOURCE;
        goto cleanup;
    }
    strncat(hash_key, wf_id_str, strlen(wf_id_str));

    filter_key->unique_id = orcm_util_create_hash_key(hash_key, strlen(hash_key) + 1);
    filter_key->unique_id_generated = true;

cleanup:
    SAFEFREE(hash_key);
    SAFEFREE(wf_id_str);
    return rc;
}

static orcm_analytics_value_t *generate_data_to_next(orcm_analytics_value_t *filter_data,
                                                     opal_list_t *filter_list)
{
    orcm_analytics_value_t *analytics_value_to_next = NULL;

    if (NULL == filter_list) {
        analytics_value_to_next = orcm_util_load_orcm_analytics_value(filter_data->key,
                                 filter_data->non_compute_data, filter_data->compute_data);
    } else {
        analytics_value_to_next = orcm_util_load_orcm_analytics_value_compute(filter_data->key,
                                 filter_data->non_compute_data, filter_list);
    }

    return analytics_value_to_next;
}

static int analyze(int sd, short args, void *cbdata)
{
    orcm_workflow_caddy_t *filter_caddy = (orcm_workflow_caddy_t *) cbdata;
    int rc = ORCM_SUCCESS;
    orcm_analytics_value_t *data_to_next = NULL;
    orcm_analytics_value_t *filter_data = NULL;
    opal_list_t *filter_list = NULL;
    mca_analytics_filter_module_t *mod = NULL;
    filter_key_t *filter_key = NULL;

    if (ORCM_SUCCESS != (rc = orcm_analytics_base_assert_caddy_data(cbdata))) {
        goto cleanup;
    }
    filter_data = filter_caddy->analytics_value;

    if (ORCM_SUCCESS != (rc = analytics_filter_data(&filter_caddy->wf_step->attributes,
                                                    filter_data, &filter_list))) {
        goto cleanup;
    }

    mod = (mca_analytics_filter_module_t*)filter_caddy->imod;
    filter_key = (filter_key_t*)mod->api.orcm_mca_analytics_data_store;
    if (!filter_key->unique_id_generated) {
        rc = generate_hash_key(&filter_caddy->wf_step->attributes,
                               filter_caddy->wf->workflow_id, filter_key);
        if (ORCM_SUCCESS != rc) {
            goto cleanup;
        }

    }

    data_to_next = generate_data_to_next(filter_data, filter_list);
    if (NULL == data_to_next) {
        rc = ORCM_ERR_OUT_OF_RESOURCE;
        goto cleanup;
    }

    if(true == orcm_analytics_base_db_check(filter_caddy->wf_step)){
        rc = orcm_analytics_base_log_to_database_event(data_to_next);
        if(ORCM_SUCCESS != rc){
            goto cleanup;
        }
    }
    if(NULL == filter_caddy->wf->hostname_regex) {
        filter_caddy->wf->hostname_regex = orcm_analytics_get_hostname_from_attributes(&filter_caddy->wf_step->attributes);
    }
    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(filter_caddy->wf, filter_caddy->wf_step,
                                     filter_key->unique_id, data_to_next, NULL);
cleanup:
    if (NULL != filter_caddy) {
        OBJ_RELEASE(filter_caddy);
    }
    if (ORCM_SUCCESS != rc && NULL != filter_list) {
        OBJ_RELEASE(filter_list);
    }
    return rc;
}
