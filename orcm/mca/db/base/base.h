/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc.  All rights reserved.
 * Copyright (c) 2013-2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_DB_BASE_H
#define MCA_DB_BASE_H

#include <time.h>

#include "orcm_config.h"
#include "orcm/types.h"

#include "opal/mca/mca.h"
#include "opal/mca/base/mca_base_framework.h"
#include "opal/mca/event/event.h"
#include "opal/class/opal_list.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/class/opal_bitmap.h"
#include "opal/dss/dss.h"

#include "orcm/mca/db/db.h"

BEGIN_C_DECLS

ORCM_DECLSPEC extern mca_base_framework_t orcm_db_base_framework;

/**
 * Select db modules for the specified handle
 */
ORCM_DECLSPEC int orcm_db_base_select(void);

typedef struct {
    opal_list_t actives;
    opal_pointer_array_t handles;
    opal_event_base_t *ev_base;
    bool ev_base_active;
} orcm_db_base_t;

typedef struct {
    opal_list_item_t super;
    orcm_db_base_component_t *component;
} orcm_db_base_active_component_t;
OBJ_CLASS_DECLARATION(orcm_db_base_active_component_t);

typedef struct {
    opal_object_t super;
    opal_event_t ev;

    int dbhandle;
    orcm_db_data_type_t data_type;
    opal_list_t *input;
    opal_list_t *output;
    orcm_db_callback_fn_t cbfunc;
    void *cbdata;

    opal_list_t *properties;

    const char *hostname;
    const struct timeval *time_stamp;
    const char *data_group;
    char *primary_key;
    char *key;

   const char *diag_type;
   const char *diag_subtype;
   const struct tm *start_time;
   const struct tm *end_time;
   const int *component_index;
   const char *test_result;

    opal_list_t *kvs;
    const char *view_name;
} orcm_db_request_t;
OBJ_CLASS_DECLARATION(orcm_db_request_t);

typedef struct {
    opal_object_t super;
    orcm_db_base_component_t *component;
    orcm_db_base_module_t *module;
} orcm_db_handle_t;
OBJ_CLASS_DECLARATION(orcm_db_handle_t);

typedef enum {
    ORCM_DB_ITEM_INTEGER,
    ORCM_DB_ITEM_REAL,
    ORCM_DB_ITEM_STRING
} orcm_db_item_type_t;

typedef struct {
    orcm_db_item_type_t item_type;
    opal_data_type_t opal_type;
    union {
        long long int value_int;
        double value_real;
        char *value_str;
    } value;
} orcm_db_item_t;

ORCM_DECLSPEC extern orcm_db_base_t orcm_db_base;

ORCM_DECLSPEC void orcm_db_base_open(char *name,
                                     opal_list_t *properties,
                                     orcm_db_callback_fn_t cbfunc,
                                     void *cbdata);
ORCM_DECLSPEC void orcm_db_base_close(int dbhandle,
                                      orcm_db_callback_fn_t cbfunc,
                                      void *cbdata);
ORCM_DECLSPEC void orcm_db_base_store(int dbhandle,
                                      const char *primary_key,
                                      opal_list_t *kvs,
                                      orcm_db_callback_fn_t cbfunc,
                                      void *cbdata);
ORCM_DECLSPEC void orcm_db_base_store_new(int dbhandle,
                                          orcm_db_data_type_t data_type,
                                          opal_list_t *input,
                                          opal_list_t *ret,
                                          orcm_db_callback_fn_t cbfunc,
                                          void *cbdata);
ORCM_DECLSPEC void orcm_db_base_record_data_samples(
        int dbhandle,
        const char *hostname,
        const struct timeval *time_stamp,
        const char *data_group,
        opal_list_t *samples,
        orcm_db_callback_fn_t cbfunc,
        void *cbdata);
ORCM_DECLSPEC void orcm_db_base_update_node_features(
        int dbhandle,
        const char *hostname,
        opal_list_t *features,
        orcm_db_callback_fn_t cbfunc,
        void *cbdata);
ORCM_DECLSPEC void orcm_db_base_record_diag_test(int dbhandle,
                                                 const char *hostname,
                                                 const char *diag_type,
                                                 const char *diag_subtype,
                                                 const struct tm *start_time,
                                                 const struct tm *end_time,
                                                 const int *component_index,
                                                 const char *test_result,
                                                 opal_list_t *test_params,
                                                 orcm_db_callback_fn_t cbfunc,
                                                 void *cbdata);
ORCM_DECLSPEC void orcm_db_base_commit(int dbhandle,
                                       orcm_db_callback_fn_t cbfunc,
                                       void *cbdata);
ORCM_DECLSPEC void orcm_db_base_rollback(int dbhandle,
                                         orcm_db_callback_fn_t cbfunc,
                                         void *cbdata);
ORCM_DECLSPEC void orcm_db_base_fetch(int dbhandle,
                                      const char *view,
                                      opal_list_t *filters,
                                      opal_list_t *kvs,
                                      orcm_db_callback_fn_t cbfunc,
                                      void *cbdata);
ORCM_DECLSPEC int orcm_db_base_get_num_rows(int dbhandle,
                                            int rshandle,
                                            int *num_rows);
ORCM_DECLSPEC int orcm_db_base_get_next_row(int dbhandle,
                                            int rshandle,
                                            opal_list_t *row);
ORCM_DECLSPEC int orcm_db_base_close_result_set(int dbhandle,
                                                int rshandle);
ORCM_DECLSPEC void orcm_db_base_remove_data(int dbhandle,
                                            const char *primary_key,
                                            const char *key,
                                            orcm_db_callback_fn_t cbfunc,
                                            void *cbdata);

ORCM_DECLSPEC int opal_value_to_orcm_db_item(const opal_value_t *kv,
                                             orcm_db_item_t *item);
ORCM_DECLSPEC int orcm_util_find_items(const char *keys[],
                             int num_keys,
                             opal_list_t *list,
                             opal_value_t *items[],
                             opal_bitmap_t *map);

END_C_DECLS

char* build_query_from_view_name_and_filters(const char* view_name, opal_list_t* filters);

#endif
