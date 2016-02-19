/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/analytics/spatial/analytics_spatial.h"

#include "orcm/include/orcm_config.h"
#include "orcm/include/orcm/constants.h"

#include <stdio.h>
#include <ctype.h>
#include <float.h>
#include <math.h>

#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/util/utils.h"
#include "orcm/util/logical_group.h"

/* constructor of the spatial_statistics_t data structure */
static void spatial_statistics_con(spatial_statistics_t* spatial_stat)
{
    spatial_stat->interval = 0;
    spatial_stat->size = 0;
    spatial_stat->nodelist = NULL;
    spatial_stat->compute_type = ORCM_ANALYTICS_COMPUTE_UNKNOWN;
    spatial_stat->last_round_time = NULL;
    spatial_stat->buckets = NULL;
    spatial_stat->result = 0.0;
    spatial_stat->num_data_point = 0;
    spatial_stat->timeout_event = NULL;
    spatial_stat->timeout = 0;
}

/* destructor of the spatial_statistics_t data structure */
static void spatial_statistics_des(spatial_statistics_t* spatial_stat)
{
    opal_argv_free(spatial_stat->nodelist);
    spatial_stat->nodelist = NULL;
    SAFEFREE(spatial_stat->last_round_time);
    if (NULL != spatial_stat->buckets) {
        opal_hash_table_remove_all(spatial_stat->buckets);
        OBJ_RELEASE(spatial_stat->buckets);
    }
    if (NULL != spatial_stat->timeout_event) {
        opal_event_free(spatial_stat->timeout_event);
        spatial_stat->timeout_event = NULL;
    }
}

OBJ_CLASS_INSTANCE(spatial_statistics_t, opal_object_t,
                   spatial_statistics_con, spatial_statistics_des);

static int init(orcm_analytics_base_module_t* imod);
static void finalize(orcm_analytics_base_module_t* imod);
static int analyze(int sd, short args, void* cbdata);

mca_analytics_spatial_module_t orcm_analytics_spatial_module = { {init ,finalize, analyze, NULL, NULL} };

/* function to reset the spatial statistics */
static void reset_statistics(spatial_statistics_t* spatial_statistics, struct timeval* current_time);

/* function to fill the rest of the spatial statistics
 * data structure besides reading from workflow file*/
static int fill_spatial_statistics(spatial_statistics_t* spatial_statistics);

/* function to initialize the global spatial statistics when first sample comes */
static int init_spatial_statistics(spatial_statistics_t* spatial_statistics, opal_list_t* attributes);

/* function to add the spatial descriptions to the ras event */
static int set_event_description(spatial_statistics_t* spatial_statistics,
                                 orcm_ras_event_t* event_data);

/* function to send the event data to the evengen framework */
static int send_data_to_evgen(spatial_statistics_t* spatial_statistics,
                              orcm_analytics_value_t* analytics_value);

/* function do the computation when the buckets are full */
static int do_compute(spatial_statistics_t* spatial_statistics, char** units);

/* function to handle the full buckets: do computation, send data to the next plugin */
static orcm_analytics_value_t* handle_full_bucket(spatial_statistics_t* spatial_statistics,
                                                  orcm_workflow_caddy_t* caddy, int* rc);

/* function to insert a timeout event in the event queue */
static int insert_timeout_event(spatial_statistics_t* spatial_statistics,
                                orcm_workflow_caddy_t* caddy);

/* function to fill the bucket when a new data sample is coming */
static orcm_analytics_value_t* fill_bucket(spatial_statistics_t* spatial_statistics,
                                           orcm_workflow_caddy_t* caddy, int* rc);
/* function for collecting a sample */
static orcm_analytics_value_t* collect_sample(spatial_statistics_t* spatial_statistics,
                                              orcm_workflow_caddy_t* caddy, int* rc);

static int init(orcm_analytics_base_module_t* imod)
{
    mca_analytics_spatial_module_t* mod = (mca_analytics_spatial_module_t*)imod;
    if (NULL == mod) {
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == (mod->api.orcm_mca_analytics_data_store = OBJ_NEW(spatial_statistics_t))) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    return ORCM_SUCCESS;
}

static void finalize(orcm_analytics_base_module_t* imod)
{
    mca_analytics_spatial_module_t* mod = (mca_analytics_spatial_module_t*)imod;
    spatial_statistics_t* spatial_data_store = NULL;
    if (NULL != mod) {
        spatial_data_store = (spatial_statistics_t*)(mod->api.orcm_mca_analytics_data_store);
        SAFE_RELEASE(spatial_data_store);
        free(mod);
    }
}

static void reset_statistics(spatial_statistics_t* spatial_statistics, struct timeval* current_time)
{
    opal_hash_table_remove_all(spatial_statistics->buckets);
    spatial_statistics->num_data_point = 0;
    if (ORCM_ANALYTICS_COMPUTE_MIN == spatial_statistics->compute_type) {
        spatial_statistics->result = DBL_MAX;
    } else if (ORCM_ANALYTICS_COMPUTE_MAX == spatial_statistics->compute_type) {
        spatial_statistics->result = DBL_MIN;
    } else {
        spatial_statistics->result = 0.0;
    }

    if (NULL != current_time) {
        if (NULL == spatial_statistics->last_round_time &&
            NULL == (spatial_statistics->last_round_time =
            (struct timeval*)malloc(sizeof(struct timeval)))) {
            return;
        }
        spatial_statistics->last_round_time->tv_sec = current_time->tv_sec;
        spatial_statistics->last_round_time->tv_usec = current_time->tv_usec;
    }
    if (NULL != spatial_statistics->timeout_event) {
        opal_event_free(spatial_statistics->timeout_event);
        spatial_statistics->timeout_event = NULL;
    }
}

static int fill_spatial_statistics(spatial_statistics_t* spatial_statistics)
{
    if (ORCM_ANALYTICS_COMPUTE_AVE > spatial_statistics->compute_type) {
        return ORCM_ERR_BAD_PARAM;
    }

    if (0 == (spatial_statistics->size = opal_argv_count(spatial_statistics->nodelist))) {
        return ORCM_ERR_BAD_PARAM;
    }

    if (0 > spatial_statistics->interval) {
        return ORCM_ERR_BAD_PARAM;
    } else if (0 == spatial_statistics->interval) {
        spatial_statistics->interval = 60;
    }

    if (0 > spatial_statistics->timeout) {
        return ORCM_ERR_BAD_PARAM;
    } else if (0 == spatial_statistics->timeout) {
        spatial_statistics->timeout = 60;
    }

    if (NULL == (spatial_statistics->buckets = OBJ_NEW(opal_hash_table_t))) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    reset_statistics(spatial_statistics, NULL);

    return opal_hash_table_init(spatial_statistics->buckets, spatial_statistics->size);
}

static int init_spatial_statistics(spatial_statistics_t* spatial_statistics, opal_list_t* attributes)
{
    int rc = ORCM_SUCCESS;
    opal_value_t* attribute = NULL;

    if (NULL == attributes) {
        return ORCM_ERR_BAD_PARAM;
    }

    OPAL_LIST_FOREACH(attribute, attributes, opal_value_t) {
        if (NULL == attribute || NULL == attribute->key || NULL == attribute->data.string) {
            return ORCM_ERR_BAD_PARAM;
        } else if (0 == strncmp(attribute->key, "nodelist", strlen(attribute->key) + 1)) {
            if (NULL == spatial_statistics->nodelist) {
                if (ORCM_SUCCESS != (rc = orcm_logical_group_parse_array_string(
                                     attribute->data.string, &spatial_statistics->nodelist))) {
                    return rc;
                }
            }
        } else if (0 == strncmp(attribute->key, "compute", strlen(attribute->key) + 1)) {
            spatial_statistics->compute_type = orcm_analytics_base_get_compute_type(
                                                                   attribute->data.string);
        } else if (0 == strncmp(attribute->key, "interval", strlen(attribute->key) + 1)) {
            spatial_statistics->interval = (int)strtol(attribute->data.string, NULL, 10);
        } else if (0 == strncmp(attribute->key, "timeout", strlen(attribute->key) + 1)) {
            spatial_statistics->timeout = (int)strtol(attribute->data.string, NULL, 10);
        }
    }

    return fill_spatial_statistics(spatial_statistics);
}

static int set_event_description(spatial_statistics_t* spatial_statistics,
                                 orcm_ras_event_t* event_data)
{
    int rc = orcm_analytics_base_event_set_description(event_data, "interval",
                                            &spatial_statistics->interval, OPAL_INT, NULL);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    rc = orcm_analytics_base_event_set_description(event_data, "timeout",
                                            &spatial_statistics->timeout, OPAL_INT, NULL);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    rc = orcm_analytics_base_event_set_description(event_data, "compute_type",
         orcm_analytics_base_set_compute_type(spatial_statistics->compute_type), OPAL_STRING, NULL);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    rc = orcm_analytics_base_event_set_description(event_data, "num_sample_recv",
                                            &spatial_statistics->buckets->ht_size, OPAL_INT, NULL);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    rc = orcm_analytics_base_event_set_description(event_data, "num_data_point",
                                            &spatial_statistics->num_data_point, OPAL_UINT64, NULL);
    return rc;
}

static int send_data_to_evgen(spatial_statistics_t* spatial_statistics,
                              orcm_analytics_value_t* analytics_value)
{
    int rc = ORCM_SUCCESS;
    orcm_ras_event_t* event_data = NULL;

    if (NULL == (event_data = orcm_analytics_base_event_create(analytics_value,
                              ORCM_RAS_EVENT_SENSOR, ORCM_RAS_SEVERITY_INFO))) {
        return ORCM_ERROR;
    }

    rc = orcm_analytics_base_event_set_storage(event_data, ORCM_STORAGE_TYPE_DATABASE);
    if(ORCM_SUCCESS != rc){
        SAFE_RELEASE(event_data);
        return rc;
    }

    if (ORCM_SUCCESS != (rc = set_event_description(spatial_statistics, event_data))) {
        SAFE_RELEASE(event_data);
        return rc;
    }

    ORCM_RAS_EVENT(event_data);
    return rc;
}

static int do_compute(spatial_statistics_t* spatial_statistics, char** units)
{
    int rc = ORCM_SUCCESS;
    int idx = 0;
    double sum = 0.0;
    opal_list_t* bucket_item_value = NULL;
    orcm_value_t* list_item = NULL;
    double num = 0.0;

    for(; idx < spatial_statistics->size; idx++) {
        if (OPAL_SUCCESS != (rc = opal_hash_table_get_value_ptr(spatial_statistics->buckets,
            spatial_statistics->nodelist[idx], strlen(spatial_statistics->nodelist[idx]) + 1,
            (void**)&bucket_item_value))) {
            continue;
        }
        spatial_statistics->num_data_point += bucket_item_value->opal_list_length;
        OPAL_LIST_FOREACH(list_item, bucket_item_value, orcm_value_t) {
            if (NULL == list_item) {
                rc = ORCM_ERR_BAD_PARAM;
                return rc;
            }
            if (NULL == *units) {
                *units = list_item->units;
            }
            num = orcm_util_get_number_orcm_value(list_item);
            if (ORCM_ANALYTICS_COMPUTE_AVE == spatial_statistics->compute_type ||
                ORCM_ANALYTICS_COMPUTE_SD == spatial_statistics->compute_type) {
                spatial_statistics->result += num;
            } else if (ORCM_ANALYTICS_COMPUTE_MIN == spatial_statistics->compute_type) {
                if (spatial_statistics->result > num) {
                    spatial_statistics->result = num;
                }
            } else if (spatial_statistics->result < num) {
                spatial_statistics->result = num;
            }
        }
    }

    if (0 == spatial_statistics->num_data_point) {
        rc = ORCM_ERR_BAD_PARAM;
        return rc;
    }
    if (ORCM_ANALYTICS_COMPUTE_AVE == spatial_statistics->compute_type ||
        ORCM_ANALYTICS_COMPUTE_SD == spatial_statistics->compute_type) {
        spatial_statistics->result /= spatial_statistics->num_data_point;
    }

    if (ORCM_ANALYTICS_COMPUTE_SD == spatial_statistics->compute_type) {
        for(idx = 0; idx < spatial_statistics->size; idx++) {
            if (OPAL_SUCCESS != (rc = opal_hash_table_get_value_ptr(spatial_statistics->buckets,
                spatial_statistics->nodelist[idx], strlen(spatial_statistics->nodelist[idx]) + 1,
                (void**)&bucket_item_value))) {
                continue;
            }
            OPAL_LIST_FOREACH(list_item, bucket_item_value, orcm_value_t) {
                sum += pow((orcm_util_get_number_orcm_value(list_item) -
                            spatial_statistics->result), 2);
            }
        }
        spatial_statistics->result = sqrt(sum / spatial_statistics->num_data_point);
    }
    rc = ORCM_SUCCESS;
    return rc;
}

static orcm_analytics_value_t* handle_full_bucket(spatial_statistics_t* spatial_statistics,
                                                  orcm_workflow_caddy_t* caddy, int* rc)
{
    orcm_analytics_value_t* data_to_next = NULL;
    opal_list_t* next_list = NULL;
    orcm_value_t* next_list_item = NULL;
    char* units = NULL;
    char* data_key = NULL;

    if (ORCM_SUCCESS != (*rc = do_compute(spatial_statistics, &units))) {
        return NULL;
    }

    asprintf(&data_key, "%s_Workflow %d", "Spatial_Result", caddy->wf->workflow_id);
    if (NULL == data_key) {
        return NULL;
    }
    if (NULL == (next_list_item = orcm_util_load_orcm_value(data_key,
                 &spatial_statistics->result, OPAL_DOUBLE, units))) {
        SAFEFREE(data_key);
        return NULL;
    }
    if (NULL == (next_list = OBJ_NEW(opal_list_t))) {
        SAFEFREE(data_key);
        SAFE_RELEASE(next_list_item);
        return NULL;
    }
    opal_list_append(next_list, (opal_list_item_t*)next_list_item);
    data_to_next = orcm_util_load_orcm_analytics_value_compute(caddy->analytics_value->key,
                             caddy->analytics_value->non_compute_data, next_list);
    if (NULL != data_to_next && true == orcm_analytics_base_db_check(caddy->wf_step, false)) {
        if (ORCM_SUCCESS != (*rc = send_data_to_evgen(spatial_statistics, data_to_next))) {
            SAFEFREE(data_key);
            SAFE_RELEASE(data_to_next);
        }
    }
    return data_to_next;
}

static int insert_timeout_event(spatial_statistics_t* spatial_statistics,
                                orcm_workflow_caddy_t* caddy)
{
    orcm_analytics_value_t* analytics_value = caddy->analytics_value;
    orcm_analytics_value_t* timeout_data = NULL;
    orcm_workflow_caddy_t* timeout_caddy = NULL;
    struct timeval time;

    if (NULL == (timeout_data = orcm_util_load_orcm_analytics_value_compute(analytics_value->key,
                 analytics_value->non_compute_data, NULL))) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    if (NULL == (timeout_caddy = orcm_analytics_base_create_caddy(caddy->wf,
                 caddy->wf_step, caddy->hash_key, timeout_data))) {
        SAFE_RELEASE(timeout_data);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    if (NULL == (spatial_statistics->timeout_event = opal_event_new(caddy->wf->ev_base, -1,
                 OPAL_EV_WRITE, caddy->imod->analyze, timeout_caddy))) {
        SAFE_RELEASE(timeout_caddy);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    time.tv_sec = spatial_statistics->timeout;
    time.tv_usec = 0;

    return opal_event_add(spatial_statistics->timeout_event, &time);
}

static orcm_analytics_value_t* fill_bucket(spatial_statistics_t* spatial_statistics,
                                           orcm_workflow_caddy_t* caddy, int* rc)
{
    orcm_value_t* item = NULL;
    opal_list_t* bucket_item_value = NULL;
    int idx = 0;
    orcm_analytics_value_t* analytics_value = caddy->analytics_value;

    for (; idx < spatial_statistics->size; idx++) {
        OPAL_LIST_FOREACH(item, analytics_value->key, orcm_value_t) {
            if (NULL == item || NULL == item->value.key) {
                *rc = ORCM_ERR_BAD_PARAM;
                return NULL;
            }
            if (0 == strncmp("hostname", item->value.key, strlen(item->value.key) + 1)) {
                if (NULL == item->value.data.string) {
                    *rc = ORCM_ERR_BAD_PARAM;
                    return NULL;
                }
                if (0 == strncmp(spatial_statistics->nodelist[idx], item->value.data.string,
                                 strlen(item->value.data.string) + 1)) {
                    *rc = opal_hash_table_get_value_ptr(spatial_statistics->buckets,
                         spatial_statistics->nodelist[idx],
                         strlen(spatial_statistics->nodelist[idx]) + 1, (void**)&bucket_item_value);
                    if (OPAL_SUCCESS == *rc) {
                        SAFE_RELEASE(bucket_item_value);
                    } else if (OPAL_ERR_NOT_FOUND != *rc) {
                        return NULL;
                    }
                    OBJ_RETAIN(analytics_value->compute_data);
                    *rc = opal_hash_table_set_value_ptr(spatial_statistics->buckets,
                                          spatial_statistics->nodelist[idx],
                                          strlen(spatial_statistics->nodelist[idx]) + 1,
                                          analytics_value->compute_data);
                    if (OPAL_SUCCESS != *rc) {
                        SAFE_RELEASE(analytics_value->compute_data);
                        return NULL;
                    }
                    if (spatial_statistics->size == (int)(spatial_statistics->buckets->ht_size)) {
                        return handle_full_bucket(spatial_statistics, caddy, rc);
                    } else {
                        /* first sample comes, then set a timeout event */
                        if (1 == spatial_statistics->buckets->ht_size &&
                            NULL == spatial_statistics->timeout_event) {
                            *rc = insert_timeout_event(spatial_statistics, caddy);
                        }
                        if (ORCM_SUCCESS == *rc) {
                            *rc = ORCM_ERR_RECV_MORE_THAN_POSTED;
                        }
                        return NULL;
                    }
                } else {
                    break;
                }
            }
        }
    }

    *rc = ORCM_ERR_BAD_PARAM;
    return NULL;
}

static orcm_analytics_value_t* collect_sample(spatial_statistics_t* spatial_statistics,
                                              orcm_workflow_caddy_t* caddy, int* rc)
{
    orcm_analytics_value_t* analytics_value = caddy->analytics_value;
    orcm_analytics_value_t* data_to_next = NULL;
    struct timeval current_time;

    if (NULL == analytics_value->key || NULL == analytics_value->non_compute_data ||
        0 == opal_list_get_size(analytics_value->key) ||
        0 == opal_list_get_size(analytics_value->compute_data)) {
        *rc = ORCM_ERR_BAD_PARAM;
        return NULL;
    }

    gettimeofday(&current_time, NULL);
    if (NULL == spatial_statistics->last_round_time || spatial_statistics->interval <=
        orcm_util_time_diff(spatial_statistics->last_round_time, &current_time)) {
        if (NULL != (data_to_next = fill_bucket(spatial_statistics, caddy, rc))) {
            reset_statistics(spatial_statistics, &current_time);
        }
    } else {
        *rc = ORCM_ERR_RECV_MORE_THAN_POSTED;
    }

    return data_to_next;
}

static int analyze(int sd, short args, void* cbdata)
{
    int rc = ORCM_SUCCESS;
    orcm_workflow_caddy_t* caddy = (orcm_workflow_caddy_t*)cbdata;
    mca_analytics_spatial_module_t* mod = NULL;
    spatial_statistics_t* spatial_statistics = NULL;
    orcm_analytics_value_t* data_to_next = NULL;
    struct timeval current_time;

    if (ORCM_SUCCESS != (rc = orcm_analytics_base_assert_caddy_data(cbdata))) {
        SAFE_RELEASE(caddy);
        return rc;
    }
    mod = (mca_analytics_spatial_module_t*)caddy->imod;
    if (NULL == mod->api.orcm_mca_analytics_data_store &&
        NULL == (mod->api.orcm_mca_analytics_data_store = OBJ_NEW(spatial_statistics_t))) {
        SAFE_RELEASE(caddy);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    spatial_statistics = (spatial_statistics_t*)(mod->api.orcm_mca_analytics_data_store);
    if (0 == spatial_statistics->size) {
        rc = init_spatial_statistics(spatial_statistics, &(caddy->wf_step->attributes));
        if (ORCM_SUCCESS != rc) {
            SAFE_RELEASE(caddy);
            return rc;
        }
    }

    /* no compute data meaning it is a timeout event, we are going to do the computation anyway */
    if (NULL == caddy->analytics_value->compute_data) {
        if (NULL != (data_to_next = handle_full_bucket(spatial_statistics, caddy, &rc)) &&
            ORCM_SUCCESS == rc) {
            gettimeofday(&current_time, NULL);
            reset_statistics(spatial_statistics, &current_time);
            ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(caddy->wf, caddy->wf_step,
                                             caddy->hash_key, data_to_next, NULL);
        }
    } else if (NULL != (data_to_next = collect_sample(spatial_statistics, caddy, &rc)) &&
               ORCM_SUCCESS == rc) {
        ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(caddy->wf, caddy->wf_step, caddy->hash_key, data_to_next, NULL);
    }

    SAFE_RELEASE(caddy);
    return rc;
}
