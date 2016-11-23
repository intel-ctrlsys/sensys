/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc.  All rights reserved.
 * Copyright (c) 2014-2015 Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/mca.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"
#include "opal/dss/dss_types.h"
#include "opal/runtime/opal_progress_threads.h"

#include "orte/runtime/orte_globals.h"

#include "orcm/mca/db/base/base.h"
#include "orcm/util/utils.h"
/*
 * The following file was created by configure.  It contains extern
 * dbments and the definition of an array of pointers to each
 * module's public mca_base_module_t struct.
 */

#include "orcm/mca/db/base/static-components.h"

extern char* get_opal_value_as_sql_string(opal_value_t *value);
extern char* build_query_from_view_name_and_filters(const char* view_name, opal_list_t* filters);
extern char* build_query_from_function_name_and_arguments(const char* function_name, opal_list_t* arguments);

orcm_db_API_module_t orcm_db = {
    orcm_db_base_open,
    orcm_db_base_close,
    orcm_db_base_store,
    orcm_db_base_store_new,
    orcm_db_base_record_data_samples,
    orcm_db_base_update_node_features,
    orcm_db_base_record_diag_test,
    orcm_db_base_commit,
    orcm_db_base_rollback,
    orcm_db_base_fetch,
    orcm_db_base_get_num_rows,
    orcm_db_base_get_next_row,
    orcm_db_base_close_result_set,
    orcm_db_base_remove_data,
    orcm_db_base_fetch_function,
    orcm_db_base_open_multi_thread_select,
    orcm_db_base_store_multi_thread_select,
    orcm_db_base_close_multi_thread_select,
    orcm_db_base_commit_multi_thread_select,
    orcm_db_base_rollback_multi_thread_select
};
orcm_db_base_t orcm_db_base;

static bool orcm_db_base_create_evbase;

static int orcm_db_base_register(mca_base_register_flag_t flags)
{
    orcm_db_base_create_evbase = true;
    orcm_db_base.thread_counts = NULL;
    mca_base_var_register("orcm", "db", "base", "create_evbase",
                          "Create a separate event base for processing db operations",
                          MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                          OPAL_INFO_LVL_9,
                          MCA_BASE_VAR_SCOPE_READONLY,
                          &orcm_db_base_create_evbase);
    mca_base_var_register("orcm", "db", "base", "thread_counts",
                          "A comma separated list of integers specifying"
                          "the thread count per data type",
                          MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                          OPAL_INFO_LVL_9,
                          MCA_BASE_VAR_SCOPE_READONLY,
                          &orcm_db_base.thread_counts);
    return ORCM_SUCCESS;
}

static void release_event_queues_per_bucket(orcm_db_bucket_t *bucket)
{
    int thread_id;
    int storage_id;
    char *thread_name = NULL;
    for (thread_id = 0; thread_id < bucket->num_threads; ++thread_id) {
        for (storage_id = 0; storage_id < orcm_db_base.num_storage; ++storage_id) {
            if (0 <= bucket->handle_ids[thread_id][storage_id]) {
                thread_name = make_thread_name(bucket->handle_ids[thread_id][storage_id]);
                opal_progress_thread_finalize(thread_name);
                SAFEFREE(thread_name);
            }
        }
    }
}

static void release_event_queues(opal_hash_table_t *buckets)
{
    orcm_db_bucket_t *value = NULL;
    orcm_db_bucket_t *in_bucket = NULL;
    orcm_db_bucket_t *out_bucket = NULL;
    uint32_t key = 0;
    while (OPAL_SUCCESS == opal_hash_table_get_next_key_uint32(buckets, &key,
           (void**)&value, in_bucket, (void**)&out_bucket)) {
        release_event_queues_per_bucket(value);
        in_bucket = out_bucket;
        out_bucket = NULL;
    }
}

static int orcm_db_base_frame_close(void)
{
    orcm_db_base_active_component_t *active;
    int i;
    orcm_db_handle_t *hdl;
    /* cleanup the globals */
    for (i=0; i < orcm_db_base.handles.size; i++) {
        if (NULL != (hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles, i))) {
            opal_pointer_array_set_item(&orcm_db_base.handles, i, NULL);
            OBJ_RELEASE(hdl);
        }
    }
    OBJ_DESTRUCT(&orcm_db_base.handles);

    /* cycle across all the active db components and let them cleanup - order
     * doesn't matter in this case
     */
    while (NULL != (active = (orcm_db_base_active_component_t*)opal_list_remove_first(&orcm_db_base.actives))) {
        if (NULL != active->component->finalize) {
            active->component->finalize();
        }
        OBJ_RELEASE(active);
    }
    OPAL_LIST_DESTRUCT(&orcm_db_base.actives);

    if (orcm_db_base_create_evbase && orcm_db_base.ev_base_active) {
        orcm_db_base.ev_base_active = false;
        opal_progress_thread_finalize("db");
    }

    SAFEFREE(orcm_db_base.thread_counts);
    opal_argv_free(orcm_db_base.storages);
    release_event_queues(orcm_db_base.buckets);
    opal_hash_table_remove_all(orcm_db_base.buckets);
    OBJ_RELEASE(orcm_db_base.buckets);

    return mca_base_framework_components_close(&orcm_db_base_framework, NULL);
}

static int orcm_db_base_init_buckets(void)
{
    orcm_db_base.buckets = OBJ_NEW(opal_hash_table_t);
    return opal_hash_table_init(orcm_db_base.buckets, NUM_DATA_TYPE);
}

static int fill_thread_counts(char **thread_counts_str_array, int count)
{
    int index = 0;
    int rc = ORCM_SUCCESS;
    for (; index < count; ++index) {
        if (false == orcm_util_convert_str_to_int(thread_counts_str_array[index],
                                                  &(orcm_db_base.num_threads[index]))) {
            opal_output(0, "invalid number of threads:%s", thread_counts_str_array[index]);
            rc = ORCM_ERR_BAD_PARAM;
            break;
        } else if (MAX_NUM_THREAD < orcm_db_base.num_threads[index] ||
                   0 >= orcm_db_base.num_threads[index]) {
            opal_output(0, "number of threads %s is out of range:[1, %d]",
                        thread_counts_str_array[index], MAX_NUM_THREAD);
            rc = ORCM_ERR_BAD_PARAM;
            break;
        }
    }
    if (ORCM_SUCCESS == rc) {
        for (index = count; index < NUM_DATA_TYPE; ++index) {
            orcm_db_base.num_threads[index] = 1;
        }
    }
    return rc;
}

int orcm_db_base_init_thread_count(void)
{
    char **thread_counts_array = opal_argv_split(orcm_db_base.thread_counts, ',');
    int count = opal_argv_count(thread_counts_array);
    int rc = ORCM_SUCCESS;
    if (NUM_DATA_TYPE < (count = opal_argv_count(thread_counts_array))) {
        count = NUM_DATA_TYPE;
    }
    rc = fill_thread_counts(thread_counts_array, count);
    opal_argv_free(thread_counts_array);
    return rc;
}

int orcm_db_base_init_storages(void)
{
    char *storages = getenv("ORCM_MCA_db");
    orcm_db_base.storages = opal_argv_split(storages, ',');
    if (NULL != storages && NULL == orcm_db_base.storages) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    orcm_db_base.num_storage = opal_argv_count(orcm_db_base.storages);
    if (0 == orcm_db_base.num_storage) {
        orcm_db_base.num_storage = 1;
    }
    return ORCM_SUCCESS;
}

static int orcm_db_base_frame_open(mca_base_open_flag_t flags)
{
    OBJ_CONSTRUCT(&orcm_db_base.actives, opal_list_t);
    OBJ_CONSTRUCT(&orcm_db_base.handles, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_db_base.handles, 3, INT_MAX, 1);
    if (ORCM_ERR_BAD_PARAM == orcm_db_base_init_thread_count()) {
        return ORCM_ERR_BAD_PARAM;
    }
    if (ORCM_ERR_OUT_OF_RESOURCE == orcm_db_base_init_storages()) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    orcm_db_base_init_buckets();

    if (orcm_db_base_create_evbase) {
        /* create our own event base */
        orcm_db_base.ev_base_active = true;
        if (NULL == (orcm_db_base.ev_base =
                opal_progress_thread_init("db"))) {
            orcm_db_base.ev_base_active = false;
            return ORCM_ERROR;
        }
    } else {
        /* tie us to the orte_event_base */
        orcm_db_base.ev_base = orte_event_base;
    }

    /* Open up all available components */
    return mca_base_framework_components_open(&orcm_db_base_framework, flags);
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, db, "ORCM Database Framework",
                           orcm_db_base_register,
                           orcm_db_base_frame_open,
                           orcm_db_base_frame_close,
                           mca_db_base_static_components, 0);

static void req_con(orcm_db_request_t *p)
{
    p->cbdata = NULL;

    p->properties = NULL;

    p->hostname = NULL;
    p->time_stamp = NULL;
    p->data_group = NULL;
    p->primary_key = NULL;
    p->key = NULL;

    p->diag_subtype = NULL;
    p->diag_subtype = NULL;
    p->start_time = NULL;
    p->end_time = NULL;
    p->component_index = NULL;
    p->test_result = NULL;
    p->source_data_name = NULL;

    p->kvs = NULL;
}
OBJ_CLASS_INSTANCE(orcm_db_request_t,
                   opal_object_t,
                   req_con, NULL);

OBJ_CLASS_INSTANCE(orcm_db_handle_t,
                   opal_object_t,
                   NULL, NULL);

OBJ_CLASS_INSTANCE(orcm_db_base_active_component_t,
                   opal_list_item_t,
                   NULL, NULL);

static void filter_con(orcm_db_filter_t *p)
{
    p->op = NONE;
}

OBJ_CLASS_INSTANCE(orcm_db_filter_t,
                   opal_value_t,
                   filter_con, NULL);

void bucket_con(orcm_db_bucket_t *bucket)
{
    bucket->current_thread_id = -1;
    bucket->handle_ids = NULL;
    bucket->num_threads = 1;
}

void bucket_des(orcm_db_bucket_t *bucket)
{
    bucket->current_thread_id = -1;
    orcm_util_release_2d_int_array(bucket->handle_ids, bucket->num_threads);
}

OBJ_CLASS_INSTANCE(orcm_db_bucket_t, opal_object_t, bucket_con, bucket_des);
