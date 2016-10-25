/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc.  All rights reserved.
 * Copyright (c) 2013-2015 Intel Corporation. All rights reserved
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal_stdint.h"
#include "opal/mca/mca.h"
#include "opal/util/error.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"
#include "opal/dss/dss_types.h"

#include "orcm/mca/db/base/base.h"
#include "opal/runtime/opal_progress_threads.h"
#include "orcm/util/utils.h"

static void process_open(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    orcm_db_base_module_t *mod;
    orcm_db_base_active_component_t *active;
    orcm_db_base_component_t *component;
    int i, index;
    char **cmps = NULL;
    opal_value_t *kv;
    bool found;

    /* see if the caller provided the magic "components" property */
    if (NULL != req->input) {
        OPAL_LIST_FOREACH(kv, req->input, opal_value_t) {
            if (0 == strcmp(kv->key, "components")) {
                cmps = opal_argv_split(kv->data.string, ',');
                break;
            }
        }
    }

    /* cycle thru the available components until one saids
     * it can create a handle for these properties
     */
    OPAL_LIST_FOREACH(active, &orcm_db_base.actives, orcm_db_base_active_component_t) {
        component = active->component;
        found = true;
        if (NULL != cmps) {
            found = false;
            for (i=0; NULL != cmps[i]; i++) {
                if (0 == strcmp(cmps[i], component->base_version.mca_component_name)) {
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            /* let this component try */
            if (NULL != (mod = component->create_handle(req->input))) {
                /* create the handle */
                hdl = OBJ_NEW(orcm_db_handle_t);
                hdl->component = component;
                hdl->module = mod;
                index = opal_pointer_array_add(&orcm_db_base.handles, hdl);
                req->dbhandle = index;
                if (NULL != req->cbfunc) {
                    req->cbfunc(index, ORCM_SUCCESS, req->input, NULL,
                                req->cbdata);
                }
                opal_argv_free(cmps);
                OBJ_RELEASE(req);
                return;
            }
        }
    }

    /* if we get here, we were unable to create the handle */
    if (NULL != req->cbfunc) {
        req->cbfunc(-1, ORCM_ERROR, req->input, NULL, req->cbdata);
    }
    opal_argv_free(cmps);
    OBJ_RELEASE(req);
}

static void enqueue_request(opal_event_base_t *event_base, orcm_db_request_t *req,
                            void(*func)(int, short, void*))
{
    opal_event_set(event_base, &req->ev, -1, OPAL_EV_WRITE, func, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}

static orcm_db_request_t* make_an_open_request(char *name, opal_list_t *properties,
                                               orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    orcm_db_request_t *req = OBJ_NEW(orcm_db_request_t);
    req->primary_key = name;
    req->input = properties;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    return req;
}

void orcm_db_base_open(char *name,
                       opal_list_t *properties,
                       orcm_db_callback_fn_t cbfunc,
                       void *cbdata)
{
    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    orcm_db_request_t *req = make_an_open_request(name, properties, cbfunc, cbdata);
    enqueue_request(orcm_db_base.ev_base, req, process_open);
}

static opal_event_base_t* create_event_thread(int handle_id)
{
    char *thread_name = make_thread_name(handle_id);
    opal_event_base_t* event_base = opal_progress_thread_init(thread_name);
    SAFEFREE(thread_name);
    return event_base;
}

static int create_a_handle(orcm_db_request_t *req)
{
    orcm_db_handle_t *handle = NULL;
    process_open(0, 0, req);
    if (NULL != (handle = (orcm_db_handle_t*)
        opal_pointer_array_get_item(&orcm_db_base.handles, req->dbhandle))) {
        handle->ev_base = create_event_thread(req->dbhandle);
    }
    return req->dbhandle;
}

static void remove_component_element(orcm_db_request_t *req)
{
    opal_value_t *kv = NULL;
    opal_value_t *next_kv = NULL;
    OPAL_LIST_FOREACH_SAFE(kv, next_kv, req->input, opal_value_t) {
        if (0 == strncmp(kv->key, "components", strlen(kv->key) + 1)) {
            opal_list_remove_item(req->input, &kv->super);
            OBJ_RELEASE(kv);
            break;
        }
    }
}

static void add_component_element(opal_list_t *list, char *storage)
{
    opal_value_t *kv = orcm_util_load_opal_value("components", storage, OPAL_STRING);
    if (NULL != kv) {
        opal_list_append(list, (opal_list_item_t*)kv);
    }
}

static void append_storage_type(orcm_db_request_t *req, int index)
{
    if (NULL != orcm_db_base.storages) {
        if (NULL == req->input) {
            req->input = OBJ_NEW(opal_list_t);
        } else {
            remove_component_element(req);
        }
        add_component_element(req->input, orcm_db_base.storages[index]);
    }
    if (NULL != req->cbfunc && NULL != req->input) {
        OBJ_RETAIN(req->input);
    }
}

static void create_handles(orcm_db_bucket_t *bucket, orcm_db_request_t *req)
{
    int thread_id;
    int storage_id;
    req->dbhandle = -1;
    for (thread_id = 0; thread_id < bucket->num_threads; ++thread_id) {
        for (storage_id = 0; storage_id < orcm_db_base.num_storage; ++storage_id) {
            append_storage_type(req, storage_id);
            OBJ_RETAIN(req);
            bucket->handle_ids[thread_id][storage_id] = create_a_handle(req);
            req->dbhandle = -1;
        }
    }
}

static int get_data_type_index(orcm_db_data_type_t data_type)
{
    int index = 0;
    if (ORCM_DB_EVENT_DATA == data_type) {
        index = 1;
    } else if (ORCM_DB_INVENTORY_DATA == data_type) {
        index = 2;
    } else if (ORCM_DB_DIAG_DATA == data_type) {
        index = 3;
    }
    return index;
}

static orcm_db_bucket_t* create_bucket(orcm_db_data_type_t data_type)
{
    int index = get_data_type_index(data_type);
    orcm_db_bucket_t *bucket = OBJ_NEW(orcm_db_bucket_t);
    bucket->num_threads = orcm_db_base.num_threads[index];
    if (NULL == (bucket->handle_ids = orcm_util_alloc_2d_int_array(
                 orcm_db_base.num_threads[index], orcm_db_base.num_storage))) {
        OBJ_RELEASE(bucket);
    }
    return bucket;
}

static void clean_data(orcm_db_callback_fn_t cbfunc,
                       int status, opal_list_t *properties, void *cbdata)
{
    if (NULL != cbfunc) {
        cbfunc(-1, status, properties, NULL, cbdata);
    }
}

static orcm_db_bucket_t* get_bucket(orcm_db_data_type_t data_type)
{
    orcm_db_bucket_t *bucket = NULL;
    opal_hash_table_get_value_uint32(orcm_db_base.buckets, data_type, (void**)&bucket);
    return bucket;
}

static orcm_db_bucket_t* check_or_create_bucket(orcm_db_data_type_t data_type,
                                                opal_list_t *properties,
                                                orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    orcm_db_bucket_t *bucket = get_bucket(data_type);
    if (NULL != bucket) {
        clean_data(cbfunc, ORCM_EXISTS, properties, cbdata);
        return NULL;
    }
    bucket = create_bucket(data_type);
    if (NULL == bucket) {
        clean_data(cbfunc, ORCM_ERR_OUT_OF_RESOURCE, properties, cbdata);
        return NULL;
    }
    return bucket;
}

void orcm_db_base_open_multi_thread_select(orcm_db_data_type_t data_type, opal_list_t *properties,
                                           orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    orcm_db_request_t *req = NULL;
    orcm_db_bucket_t *bucket = check_or_create_bucket(data_type, properties, cbfunc, cbdata);
    if (NULL == bucket) {
        return;
    }
    req = make_an_open_request(NULL, properties, cbfunc, cbdata);
    create_handles(bucket, req);
    SAFE_RELEASE(req->input);
    OBJ_RELEASE(req);
    opal_hash_table_set_value_uint32(orcm_db_base.buckets, data_type, bucket);
}

static void process_close(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc = ORCM_SUCCESS;

    /* get the handle object */
    hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                         req->dbhandle);
    if (NULL == hdl) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL != hdl->module->finalize) {
        hdl->module->finalize((struct orcm_db_base_module_t*)hdl->module);
    }

callback_and_cleanup:
    /* release the handle */
    opal_pointer_array_set_item(&orcm_db_base.handles, req->dbhandle, NULL);
    if (NULL != hdl) {
        OBJ_RELEASE(hdl);
    }

    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, NULL, NULL, req->cbdata);
    }

    OBJ_RELEASE(req);
}

static orcm_db_request_t* make_a_close_request(int dbhandle,
                                               orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    orcm_db_request_t *req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    return req;
}

void orcm_db_base_close(int dbhandle,
                        orcm_db_callback_fn_t cbfunc,
                        void *cbdata)
{
    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    orcm_db_request_t *req = make_a_close_request(dbhandle, cbfunc, cbdata);
    enqueue_request(orcm_db_base.ev_base, req, process_close);
}

static bool enqueue_request_one_storage(int handle_id, orcm_db_request_t *req,
                                        void(*func)(int, short, void*))
{
    orcm_db_handle_t *handle = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                                              handle_id);
    if (NULL == handle) {
        return false;
    }
    enqueue_request(handle->ev_base, req, func);
    return true;
}

void orcm_db_base_close_multi_thread_select(orcm_db_data_type_t data_type,
                                            orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    int thread_id, storage_id, handle_id;
    orcm_db_request_t *req = NULL;
    orcm_db_bucket_t *bucket = get_bucket(data_type);
    if (NULL == bucket) {
        return;
    }
    for (thread_id = 0; thread_id < bucket->num_threads; ++thread_id) {
        for (storage_id = 0; storage_id < orcm_db_base.num_storage; ++storage_id) {
            handle_id = bucket->handle_ids[thread_id][storage_id];
            if (0 <= handle_id) {
                req = make_a_close_request(handle_id, cbfunc, cbdata);
                enqueue_request_one_storage(handle_id, req, process_close);
            }
        }
    }
}

static void process_store(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc=ORCM_SUCCESS;

    /* get the handle object */
    hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                         req->dbhandle);
    if (NULL == hdl) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL != hdl->module->store) {
        rc = hdl->module->store((struct orcm_db_base_module_t*)hdl->module,
                                req->primary_key, req->kvs);
    } else {
        rc = ORCM_ERR_NOT_IMPLEMENTED;
    }

callback_and_cleanup:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, req->kvs, NULL, req->cbdata);
    }
    OBJ_RELEASE(req);
}

void orcm_db_base_store(int dbhandle,
                        const char *primary_key,
                        opal_list_t *kvs,
                        orcm_db_callback_fn_t cbfunc,
                        void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->primary_key = (char*)primary_key;
    req->kvs = kvs;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_store, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}

static void process_store_new(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;

    orcm_db_handle_t *hdl;
    int rc = ORCM_SUCCESS;

    /* get the handle object */
    hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                         req->dbhandle);
    if (NULL == hdl) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL != hdl->module->store_new) {
        rc = hdl->module->store_new((struct orcm_db_base_module_t*)hdl->module,
                                    req->data_type, req->input, req->output);
    } else {
        rc = ORCM_ERR_NOT_IMPLEMENTED;
    }

callback_and_cleanup:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, req->input, req->output, req->cbdata);
    }
    OBJ_RELEASE(req);
}

static orcm_db_request_t* make_a_store_request(int dbhandle, orcm_db_data_type_t data_type,
                                               opal_list_t *input, opal_list_t *ret,
                                               orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    orcm_db_request_t *req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->data_type = data_type;
    req->input = input;
    req->output = ret;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    return req;
}

void orcm_db_base_store_new(int dbhandle,
                            orcm_db_data_type_t data_type,
                            opal_list_t *input,
                            opal_list_t *ret,
                            orcm_db_callback_fn_t cbfunc,
                            void *cbdata)
{
    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    orcm_db_request_t *req = make_a_store_request(dbhandle, data_type, input, ret, cbfunc, cbdata);
    enqueue_request(orcm_db_base.ev_base, req, process_store_new);
}

static void store_a_request(int *handle_ids, orcm_db_data_type_t data_type,
                            orcm_db_callback_fn_t cbfunc, opal_list_t* input,
                            opal_list_t* ret, void* cbdata)
{
    orcm_db_request_t *req = NULL;
    int storage_id = 0;
    for (; storage_id < orcm_db_base.num_storage; ++storage_id) {
        req = make_a_store_request(handle_ids[storage_id], data_type, input, ret, cbfunc, cbdata);
        if (NULL != cbfunc && NULL != req->input) {
            OBJ_RETAIN(req->input);
        }
        if (!enqueue_request_one_storage(handle_ids[storage_id], req, process_store_new)) {
            SAFE_RELEASE(req->input);
            OBJ_RELEASE(req);
        }
    }
    SAFE_RELEASE(input);
}

int orcm_db_base_store_multi_thread_select(orcm_db_data_type_t data_type,
                                           opal_list_t *input, opal_list_t *ret,
                                           orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    orcm_db_bucket_t *bucket = get_bucket(data_type);
    if (NULL == bucket) {
        clean_data(cbfunc, ORCM_ERR_NOT_FOUND, input, cbdata);
        return ORCM_ERR_NOT_FOUND;
    }

    /* round-robin to get the next thread that will serve the request */
    bucket->current_thread_id = (bucket->current_thread_id + 1) % bucket->num_threads;

    store_a_request(bucket->handle_ids[bucket->current_thread_id],
                    data_type, cbfunc, input, ret, cbdata);
    return bucket->current_thread_id;
}

static void process_record_data_samples(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc = ORCM_SUCCESS;

    /* get the handle object */
    hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                         req->dbhandle);
    if (NULL == hdl) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }

    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }

    if (NULL != hdl->module->record_data_samples) {
        rc = hdl->module->record_data_samples(
                (struct orcm_db_base_module_t*)hdl->module,
                req->hostname, req->time_stamp, req->data_group, req->input);
    } else {
        rc = ORCM_ERR_NOT_IMPLEMENTED;
    }

callback_and_cleanup:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, req->input, NULL, req->cbdata);
    }
    OBJ_RELEASE(req);
}

void orcm_db_base_record_data_samples(int dbhandle,
                                      const char *hostname,
                                      const struct timeval *time_stamp,
                                      const char *data_group,
                                      opal_list_t *samples,
                                      orcm_db_callback_fn_t cbfunc,
                                      void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->hostname = hostname;
    req->time_stamp = time_stamp;
    req->data_group = data_group;
    req->input = samples;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_record_data_samples, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}

static void process_update_node_features(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc = ORCM_SUCCESS;

    /* get the handle object */
    hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                         req->dbhandle);
    if (NULL == hdl) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }

    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }

    if (NULL != hdl->module->update_node_features) {
        rc = hdl->module->update_node_features(
                (struct orcm_db_base_module_t*)hdl->module,
                req->hostname, req->input);
    } else {
        rc = ORCM_ERR_NOT_IMPLEMENTED;
    }

callback_and_cleanup:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, req->input, NULL, req->cbdata);
    }
    OBJ_RELEASE(req);
}

void orcm_db_base_update_node_features(int dbhandle,
                                       const char *hostname,
                                       opal_list_t *features,
                                       orcm_db_callback_fn_t cbfunc,
                                       void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->hostname = hostname;
    req->input = features;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_update_node_features, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}

static void process_record_diag_test(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc = ORCM_SUCCESS;

    /* get the handle object */
    hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                         req->dbhandle);
    if (NULL == hdl) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }

    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }

    if (NULL != hdl->module->record_diag_test) {
        rc = hdl->module->record_diag_test(
                (struct orcm_db_base_module_t*)hdl->module,
                req->hostname,
                req->diag_type,
                req->diag_subtype,
                req->start_time,
                req->end_time,
                req->component_index,
                req->test_result,
                req->input);
    } else {
        rc = ORCM_ERR_NOT_IMPLEMENTED;
    }

callback_and_cleanup:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, req->input, NULL, req->cbdata);
    }
    OBJ_RELEASE(req);
}

void orcm_db_base_record_diag_test(int dbhandle,
                                   const char *hostname,
                                   const char *diag_type,
                                   const char *diag_subtype,
                                   const struct tm *start_time,
                                   const struct tm *end_time,
                                   const int *component_index,
                                   const char *test_result,
                                   opal_list_t *test_params,
                                   orcm_db_callback_fn_t cbfunc,
                                   void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->hostname = hostname;
    req->diag_type = diag_type;
    req->diag_subtype = diag_subtype;
    req->start_time = start_time;
    req->end_time = end_time;
    req->component_index = component_index;
    req->test_result = test_result;
    req->input = test_params;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_record_diag_test, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}

static void process_commit(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc=ORCM_SUCCESS;

    /* get the handle object */
    hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                         req->dbhandle);
    if (NULL == hdl) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL != hdl->module->commit) {
        rc = hdl->module->commit((struct orcm_db_base_module_t*)hdl->module);
    } else {
        rc = ORCM_ERR_NOT_IMPLEMENTED;
    }

callback_and_cleanup:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, NULL, NULL, req->cbdata);
    }
    OBJ_RELEASE(req);
}

static orcm_db_request_t *create_a_commit_request(int dbhandle,
                                                  orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    orcm_db_request_t *req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    return req;
}

void orcm_db_base_commit(int dbhandle,
                         orcm_db_callback_fn_t cbfunc,
                         void *cbdata)
{
    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    orcm_db_request_t *req = create_a_commit_request(dbhandle, cbfunc, cbdata);
    enqueue_request(orcm_db_base.ev_base, req, process_commit);
}

void orcm_db_base_commit_multi_thread_select(orcm_db_data_type_t data_type,
                                             int thread_id,
                                             orcm_db_callback_fn_t cbfunc,
                                             void *cbdata)
{
    orcm_db_request_t *req = NULL;
    int storage_id = 0;
    orcm_db_bucket_t *bucket = get_bucket(data_type);
    if (NULL == bucket) {
        return;
    }
    for (; storage_id < orcm_db_base.num_storage; ++storage_id) {
        req = create_a_commit_request(bucket->handle_ids[thread_id][storage_id], cbfunc, cbdata);
        enqueue_request_one_storage(bucket->handle_ids[thread_id][storage_id], req, process_commit);
    }
}

static void process_rollback(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc = ORCM_SUCCESS;

    /* get the handle object */
    hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                         req->dbhandle);
    if (NULL == hdl) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL != hdl->module->rollback) {
        rc = hdl->module->rollback((struct orcm_db_base_module_t*)hdl->module);
    } else {
        rc = ORCM_ERR_NOT_IMPLEMENTED;
    }

callback_and_cleanup:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, NULL, NULL, req->cbdata);
    }
    OBJ_RELEASE(req);
}

static orcm_db_request_t *make_a_rollback_request(int dbhandle,
                                                  orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    orcm_db_request_t *req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    return req;
}

void orcm_db_base_rollback(int dbhandle,
                           orcm_db_callback_fn_t cbfunc,
                           void *cbdata)
{
    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    orcm_db_request_t *req = make_a_rollback_request(dbhandle, cbfunc, cbdata);
    enqueue_request(orcm_db_base.ev_base, req, process_rollback);
}

void orcm_db_base_rollback_multi_thread_select(int dbhandle,
                                               orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    orcm_db_request_t *req = make_a_rollback_request(dbhandle, cbfunc, cbdata);
    enqueue_request_one_storage(dbhandle, req, process_rollback);
}

static void process_fetch(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl = NULL;
    int rc = ORCM_SUCCESS;

    /* get the handle object */
    hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                         req->dbhandle);
    if (NULL == hdl) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }

    if (NULL != hdl->module->fetch) {
        rc = hdl->module->fetch((struct orcm_db_base_module_t*)hdl->module,
                                req->source_data_name, req->input, req->output);
    } else {
        rc = ORCM_ERR_NOT_IMPLEMENTED;
    }

callback_and_cleanup:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, NULL, req->output, req->cbdata);
    }
    OBJ_RELEASE(req);
}

static void process_fetch_function(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl = NULL;
    int rc = ORCM_SUCCESS;

    /* get the handle object */
    hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                         req->dbhandle);
    if (NULL == hdl) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }

    if (NULL != hdl->module->fetch_function) {
        rc = hdl->module->fetch_function((struct orcm_db_base_module_t*)hdl->module,
                                req->source_data_name, req->input, req->output);
    } else {
        rc = ORCM_ERR_NOT_IMPLEMENTED;
    }

callback_and_cleanup:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, NULL, req->output, req->cbdata);
    }
    OBJ_RELEASE(req);
}

void orcm_db_base_fetch(int dbhandle,
                        const char *source_data,
                        opal_list_t *filters,
                        opal_list_t *kvs,
                        orcm_db_callback_fn_t cbfunc,
                        void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->input = filters;
    req->output = kvs;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    req->source_data_name = source_data;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_fetch, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}

void orcm_db_base_fetch_function(int dbhandle,
                        const char *source_data,
                        opal_list_t *arguments,
                        opal_list_t *kvs,
                        orcm_db_callback_fn_t cbfunc,
                        void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->input = arguments;
    req->output = kvs;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    req->source_data_name = source_data;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_fetch_function, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}

int orcm_db_base_get_num_rows(int dbhandle, int rshandle, int *num_rows)
{
    orcm_db_handle_t *hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles, dbhandle);

    if (NULL == hdl || NULL == hdl->module || NULL == hdl->module->get_num_rows) {
        return ORCM_ERROR;
    }

    return hdl->module->get_num_rows((struct orcm_db_base_module_t*)hdl->module, rshandle, num_rows);
}

int orcm_db_base_get_next_row(int dbhandle, int rshandle, opal_list_t *row)
{
    orcm_db_handle_t *hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles, dbhandle);

    if (NULL == hdl || NULL == hdl->module || NULL == hdl->module->get_next_row) {
        return ORCM_ERROR;
    }

    return hdl->module->get_next_row((struct orcm_db_base_module_t*)hdl->module, rshandle, row);
}

int orcm_db_base_close_result_set(int dbhandle, int rshandle)
{
    orcm_db_handle_t *hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles, dbhandle);

    if (NULL == hdl || NULL == hdl->module || NULL == hdl->module->close_result_set) {
        return ORCM_ERROR;
    }

    return hdl->module->close_result_set((struct orcm_db_base_module_t*)hdl->module, rshandle);
}

static void process_remove(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc;

    /* get the handle object */
    hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                         req->dbhandle);
    if (NULL == hdl) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto callback_and_cleanup;
    }

    if (NULL != hdl->module->remove) {
        rc = hdl->module->remove((struct orcm_db_base_module_t*)hdl->module,
                                 req->primary_key, req->key);
    } else {
        rc = ORCM_ERR_NOT_IMPLEMENTED;
    }

callback_and_cleanup:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, NULL, NULL, req->cbdata);
    }
    OBJ_RELEASE(req);
}

void orcm_db_base_remove_data(int dbhandle,
                              const char *primary_key,
                              const char *key,
                              orcm_db_callback_fn_t cbfunc,
                              void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->primary_key = (char*)primary_key;
    req->key = (char*)key;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_remove, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}
