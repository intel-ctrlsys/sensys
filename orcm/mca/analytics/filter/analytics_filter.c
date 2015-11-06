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
#include "orcm/mca/analytics/filter/analytics_filter.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"
#include "orcm/util/logical_group.h"

#define HASH_KEY_LENGTH 1000

static int init(orcm_analytics_base_module_t *imod);
static void finalize(orcm_analytics_base_module_t *imod);
static int analyze(int sd, short args, void *cbdata);

mca_analytics_filter_module_t orcm_analytics_filter_module = { { init, finalize,
        analyze } };

/* given a target, find it in the source candidate */
static int find_match(char *target, char ** candidate);

/* match the attribute to a key from the key list */
static int filter_match_value(char* key_value, char* attribute_value, char* hash_key);

/* match the attribute to the compute data list,
 * this only happens when the attribute does not match the key list */
static int filter_match_compute(orcm_value_t *attribute_value,
                                opal_list_t *compute_data, char* hash_key);

/* filter the data and fill the hash_key is data matches */
static int analytics_filter_data( orcm_workflow_caddy_t *filter_analyze_caddy, char* hash_key);

static int init(orcm_analytics_base_module_t *imod)
{
    return ORCM_SUCCESS;
}

static void finalize(orcm_analytics_base_module_t *imod)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
        "%s analytics:filter:finalize", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
}

static int find_match(char *target, char ** candidate)
{
    int index = 0, count = opal_argv_count(candidate);

    for (index = 0; index < count; index++) {
        if(0 == strncmp(target, candidate[index], strlen(target) + 1)) {
            OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                "%s analytics:Filtered Value:%s", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), target));
            return ORCM_SUCCESS;
        }
    }

    return ORCM_ERR_NOT_FOUND;
}

static int filter_match_value(char* key_value, char* attribute_value, char* hash_key)
{
    int rc = -1;
    char** source_value = NULL;

    rc = orcm_logical_group_node_names(attribute_value, &source_value);
    if(ORCM_SUCCESS != rc) {
        goto cleanup;
    }

    rc = find_match(key_value, source_value);
    if (ORCM_SUCCESS == rc) {
        strncat(hash_key, attribute_value, strlen(attribute_value));
    }

cleanup:
    opal_argv_free(source_value);
    return rc;
}

static int filter_match_compute(orcm_value_t *attribute_value,
                                opal_list_t *compute_data, char* hash_key)
{
    int rc = -1;
    orcm_value_t *compute_info = NULL;
    char** wf_attri_value = NULL;

    rc = orcm_logical_group_node_names(attribute_value->value.data.string, &wf_attri_value);
    if (ORCM_SUCCESS != rc) {
        goto cleanup;
    }

    if (NULL == compute_data) {
        rc = ORCM_ERROR;
        goto cleanup;
    }
    OPAL_LIST_FOREACH(compute_info, compute_data, orcm_value_t) {
        rc = find_match(compute_info->value.key, wf_attri_value);
        if (ORCM_SUCCESS == rc) {
            strncat(hash_key, attribute_value->value.data.string,
                    strlen(attribute_value->value.data.string));
            goto cleanup;
        }
    }

cleanup:
    opal_argv_free(wf_attri_value);
    return rc;
}

static int analytics_filter_data( orcm_workflow_caddy_t *filter_analyze_caddy, char* hash_key)
{
    orcm_value_t *attribute_value = NULL, *key_value = NULL;
    int found = 0, rc = -1;

    OPAL_LIST_FOREACH(attribute_value, &filter_analyze_caddy->wf_step->attributes, orcm_value_t) {
        found = 0;
        OPAL_LIST_FOREACH(key_value, filter_analyze_caddy->analytics_value->key, orcm_value_t) {
            if (0 == (strncmp(key_value->value.key, attribute_value->value.key,
                      strlen(key_value->value.key)))){
                rc = filter_match_value(key_value->value.data.string,
                                        attribute_value->value.data.string, hash_key);
                if(ORCM_SUCCESS != rc){
                    return rc;
                }
                found = 1;
                break;
            }
        }

        if(0 == found) {
            rc = filter_match_compute(attribute_value,
                                      filter_analyze_caddy->analytics_value->compute_data,
                                      hash_key);
            if (ORCM_SUCCESS != rc) {
                return rc;
            }
            found = 1;
        }
    }

    if(!found) {
       return ORCM_ERR_NOT_FOUND;
    }

    return ORCM_SUCCESS;
}

static int analyze(int sd, short args, void *cbdata)
{
    orcm_workflow_caddy_t *filter_analyze_caddy = (orcm_workflow_caddy_t *) cbdata;
    int rc = -1;
    uint64_t unique_id = 0;
    char *wf_id = NULL, *hash_key = NULL;
    orcm_analytics_value_t *analytics_value_to_next = NULL;

    if (ORCM_SUCCESS != (rc = orcm_analytics_base_assert_caddy_data(cbdata))) {
        goto cleanup;
    }

    if (NULL == (hash_key = (char *)malloc(sizeof(char) * HASH_KEY_LENGTH))) {
        rc = ORCM_ERR_OUT_OF_RESOURCE;
        goto cleanup;
    }
    *hash_key = '\0';

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                        "%s analytics:%s:analyze ", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        filter_analyze_caddy->wf_step->analytic));

    if (ORCM_SUCCESS != (rc = analytics_filter_data(filter_analyze_caddy, hash_key))) {
        goto cleanup;
    }

    /* add workflow id to the hash_key */
    if (-1 == asprintf(&wf_id, "%d", filter_analyze_caddy->wf->workflow_id)) {
        rc = ORCM_ERR_OUT_OF_RESOURCE;
        goto cleanup;
    }
    strncat(hash_key, wf_id, strlen(wf_id));
    unique_id = orcm_util_create_hash_key(hash_key, strlen(hash_key) + 1);

    if (NULL == (analytics_value_to_next = orcm_util_load_orcm_analytics_value(
                                 filter_analyze_caddy->analytics_value->key,
                                 filter_analyze_caddy->analytics_value->non_compute_data,
                                 filter_analyze_caddy->analytics_value->compute_data))) {
        rc = ORCM_ERR_OUT_OF_RESOURCE;
        goto cleanup;
    }

    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(filter_analyze_caddy->wf, filter_analyze_caddy->wf_step,
                                     unique_id, analytics_value_to_next);
cleanup:
    SAFEFREE(hash_key);
    SAFEFREE(wf_id);
    if (NULL != filter_analyze_caddy) {
        OBJ_RELEASE(filter_analyze_caddy);
    }
    return rc;
}
