/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc.  All rights reserved.
 * Copyright (c) 2013-2015 Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 *
 * The Database Framework
 *
 */

#ifndef ORCM_DB_H
#define ORCM_DB_H

#include <time.h>

#include "orcm_config.h"
#include "orcm/types.h"

#include "opal/mca/event/event.h"
#include "opal/dss/dss_types.h"

#include "orcm/mca/mca.h"

/**
 * DATABASE DESIGN
 *
 * DB APIs are non-blocking and executed by pushing the request onto the ORCM
 * event base. Upon completion, the provided cbfunc will be called to return
 * the status resulting from the operation (a NULL cbfunc is permitted). The
 * cbfunc is responsible for releasing the returned list
 */

BEGIN_C_DECLS

/* forward declare */
struct orcm_db_base_module_t;

typedef enum {
    ORCM_DB_ENV_DATA,
    ORCM_DB_INVENTORY_DATA,
    ORCM_DB_DIAG_DATA,
    ORCM_DB_EVENT_DATA
} orcm_db_data_type_t;

typedef enum {
    NONE,
    LT,
    GT,
    EQ,
    NE,
    LE,
    GE,
    CONTAINS,
    STARTS_WITH,
    ENDS_WITH,
    IN
} orcm_db_comparison_op_t;

typedef enum {
    ORCM_DB_QRY_END = 0,
    ORCM_DB_QRY_DATE,
    ORCM_DB_QRY_FLOAT,
    ORCM_DB_QRY_INTEGER,
    ORCM_DB_QRY_INTERVAL,
    ORCM_DB_QRY_OCOMMA_LIST,
    ORCM_DB_QRY_STRING,
    ORCM_DB_QRY_OPTIONAL = 32,
    ORCM_DB_QRY_NULL = 1024
} orcm_db_qry_arg_types;

typedef struct {
    opal_value_t value;
    orcm_db_comparison_op_t op;
} orcm_db_filter_t;
OBJ_CLASS_DECLARATION(orcm_db_filter_t);

/* callback function for async requests */
typedef void (*orcm_db_callback_fn_t)(int dbhandle,
                                      int status,
                                      opal_list_t *in,
                                      opal_list_t *out,
                                      void *cbdata);

/*
 * Initialize the module
 */
typedef int (*orcm_db_base_module_init_fn_t)(struct orcm_db_base_module_t *imod);

/*
 * Finalize the module
 */
typedef void (*orcm_db_base_module_finalize_fn_t)(struct orcm_db_base_module_t *imod);

/*
 * Open a database
 *
 * Open a database for access (read, write, etc.). The request
 * can contain a user-specified name for this database that
 * has nothing to do with the backend database - it is solely
 * for use as a debug tool to help identify the database. The
 * request can also optionally provide a list of opal_value_t
 * properties - this is where one might specify the name of
 * the backend database, a URI for contacting it, the name of
 * a particular table for request, etc. Thus, it is important
 * to note that the returned "handle" is associated solely with
 * the defined request - i.e., if the properties specify a database
 * and table, then the handle will be specific to that combination.
 *
 * NOTE: one special "property" allows you to specify the
 * name(s) of the component(s) you want considered for this
 * handle - i.e., the equivalent of specifying the MCA param
 * "db=list" - using the  reserved property name "components".
 * The components will be queried in the order specified. The ^
 * character is also supported, with the remaining components
 * considered in priority order
 *
 * Just like the standard POSIX file open, the call will return
 * a unique "handle" that must be provided with any subsequent
 * call to store or fetch data from this database.
 */
typedef void (*orcm_db_base_API_open_fn_t)(char *name,
                                           opal_list_t *properties,
                                           orcm_db_callback_fn_t cbfunc,
                                           void *cbdata);

/*
 * Close a database handle
 *
 * Close the specified database handle. This may or may not invoke
 * termination of a connection to a remote database or release of
 * memory storage, depending on the precise implementation of the
 * active database components. A -1 handle indicates that ALL open
 * database handles are to be closed.
 */
typedef void (*orcm_db_base_API_close_fn_t)(int dbhandle,
                                            orcm_db_callback_fn_t cbfunc,
                                            void *cbdata);

/*
 * Store one or more data elements against a primary key.  The values are
 * passed as a key-value list in the kvs parameter.  The semantics of the
 * primary key and list of values will depend on the data that needs to be
 * stored.
 *
 * At the moment the API store function is designed to handle storing data
 * collected by the sensor framework components.  In this case, the primary key
 * is a name for the group of data being passed (to classify the data and avoid
 * naming conflicts with other data items collected by other sensors) and the
 * list of values shall contain: the time stamp, the hostname and the values.
 * For the values, sensors may optionally provide the data units in the key
 * field using the following format: <data item name>:<data units>.  Note that
 * this means the colon (":") is a reserved character.
 */
typedef void (*orcm_db_base_API_store_fn_t)(int dbhandle,
                                            const char *primary_key,
                                            opal_list_t *kvs,
                                            orcm_db_callback_fn_t cbfunc,
                                            void *cbdata);
typedef int (*orcm_db_base_module_store_fn_t)(struct orcm_db_base_module_t *imod,
                                              const char *primary_key,
                                              opal_list_t *kvs);

typedef void (*orcm_db_base_API_store_new_fn_t)(int dbhandle,
                                                orcm_db_data_type_t data_type,
                                                opal_list_t *input,
                                                opal_list_t *ret,
                                                orcm_db_callback_fn_t cbfunc,
                                                void *cbdata);
typedef int (*orcm_db_base_module_store_new_fn_t)(
        struct orcm_db_base_module_t *imod,
        orcm_db_data_type_t data_type,
        opal_list_t *input,
        opal_list_t *ret);

/*
 * Specialized API function for storing data samples from components from the
 * sensor framework.  The samples are provided as a list of type
 * orcm_value_t.
 */
typedef void (*orcm_db_base_API_record_data_samples_fn_t)(
        int dbhandle,
        const char *hostname,
        const struct timeval *time_stamp,
        const char *data_group,
        opal_list_t *samples,
        orcm_db_callback_fn_t cbfunc,
        void *cbdata);
typedef int (*orcm_db_base_module_record_data_samples_fn_t)(
        struct orcm_db_base_module_t *imod,
        const char *hostname,
        const struct timeval *time_stamp,
        const char *data_group,
        opal_list_t *samples);

/*
 * Update one or more features for a node as part of the inventory data, for
 * example: number of sockets, cores per socket, RAM, etc.  The features are
 * passed as a list of key-value pairs plus units: orcm_value_t.  The
 * units may be left NULL if not applicable.
 */
typedef void (*orcm_db_base_API_update_node_features_fn_t)(
        int dbhandle,
        const char *hostname,
        opal_list_t *features,
        orcm_db_callback_fn_t cbfunc,
        void *cbdata);
typedef int (*orcm_db_base_module_update_node_features_fn_t)(
        struct orcm_db_base_module_t *imod,
        const char *hostname,
        opal_list_t *features);

/*
 * Store diagnostic test data for a particular diagnostic test that was run.
 * The data that can be stored includes: the test result and an optional list
 * of test parameters. The test parameters are passed as a list of key-value
 * pairs plus units: orcm_value_t. The units may be left NULL if not
 * applicable.
 */
typedef void (*orcm_db_base_API_record_diag_test_fn_t)(
        int dbhandle,
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
typedef int (*orcm_db_base_module_record_diag_test_fn_t)(
        struct orcm_db_base_module_t *imod,
        const char *hostname,
        const char *diag_type,
        const char *diag_subtype,
        const struct tm *start_time,
        const struct tm *end_time,
        const int *component_index,
        const char *test_result,
        opal_list_t *test_params);

/*
 * Commit data to the database - action depends on implementation within
 * each active component
 */
typedef void (*orcm_db_base_API_commit_fn_t)(int dbhandle,
                                             orcm_db_callback_fn_t cbfunc,
                                             void *cbdata);
typedef int (*orcm_db_base_module_commit_fn_t)(struct orcm_db_base_module_t *imod);

/*
 * Rollback or cancel the current database transaction.
 */
typedef void (*orcm_db_base_API_rollback_fn_t)(int dbhandle,
                                               orcm_db_callback_fn_t cbfunc,
                                               void *cbdata);
typedef int (*orcm_db_base_module_rollback_fn_t)(struct orcm_db_base_module_t *imod);

/*
 * Retrieve data
 *
 * Retrieve data for the given primary key associated with the specified key. Wildcards
 * are supported here as well. Caller is responsible for releasing the returned list
 * of opal_keyval_t objects.
 */
typedef void (*orcm_db_base_API_fetch_fn_t)(int dbhandle,
                                            const char* view,
                                            opal_list_t *filters,
                                            opal_list_t *kvs,
                                            orcm_db_callback_fn_t cbfunc,
                                            void *cbdata);
typedef int (*orcm_db_base_module_fetch_fn_t)(struct orcm_db_base_module_t *imod,
                                              const char* view,
                                              opal_list_t *filters,
                                              opal_list_t *kvs);

typedef void (*orcm_db_base_API_fetch_function_fn_t)(int dbhandle,
                                            const char* function,
                                            opal_list_t *arguments,
                                            opal_list_t *kvs,
                                            orcm_db_callback_fn_t cbfunc,
                                            void *cbdata);
typedef int (*orcm_db_base_module_fetch_function_fn_t)(struct orcm_db_base_module_t *imod,
                                              const char* fuction,
                                              opal_list_t *arguments,
                                              opal_list_t *kvs);
/*
 * Get the number of rows returned in a result set (previously fetched from the
 * database).
 */
typedef int (*orcm_db_base_API_get_num_rows_fn_t)(
        int dbhandle,
        int rshandle,
        int *num_rows);
typedef int (*orcm_db_base_module_get_num_rows_fn_t)(
        struct orcm_db_base_module_t *imod,
        int rshandle,
        int *num_rows);

/*
 * Get the next row from a result set (previously fetched from the database).
 */
typedef int (*orcm_db_base_API_get_next_row_fn_t)(
        int dbhandle,
        int rshandle,
        opal_list_t *row);
typedef int (*orcm_db_base_module_get_next_row_fn_t)(
        struct orcm_db_base_module_t *imod,
        int rshandle,
        opal_list_t *row);

/*
 * Close a result set (previously allocated as a result of fetching data from
 * the database).
 */
typedef int (*orcm_db_base_API_close_result_set_fn_t)(
        int dbhandle,
        int rshandle);
typedef int (*orcm_db_base_module_close_result_set_fn_t)(
        struct orcm_db_base_module_t *imod,
        int rshandle);
/*
 * Delete data
 *
 * Delete the data for the given primary key that is associated with the specified key.
 * If a NULL key is provided, all data for the given primary key will be deleted.
 */
typedef void (*orcm_db_base_API_remove_fn_t)(int dbhandle,
                                             const char *primary_key,
                                             const char *key,
                                             orcm_db_callback_fn_t cbfunc,
                                             void *cbdata);
typedef int (*orcm_db_base_module_remove_fn_t)(struct orcm_db_base_module_t *imod,
                                               const char *primary_key,
                                               const char *key);

/*
 * the standard module data structure
 */
struct orcm_db_base_module_t{
    orcm_db_base_module_init_fn_t                 init;
    orcm_db_base_module_finalize_fn_t             finalize;
    orcm_db_base_module_store_fn_t                store;
    orcm_db_base_module_store_new_fn_t            store_new;
    orcm_db_base_module_record_data_samples_fn_t  record_data_samples;
    orcm_db_base_module_update_node_features_fn_t update_node_features;
    orcm_db_base_module_record_diag_test_fn_t     record_diag_test;
    orcm_db_base_module_commit_fn_t               commit;
    orcm_db_base_module_rollback_fn_t             rollback;
    orcm_db_base_module_fetch_fn_t                fetch;
    orcm_db_base_module_get_num_rows_fn_t         get_num_rows;
    orcm_db_base_module_get_next_row_fn_t         get_next_row;
    orcm_db_base_module_close_result_set_fn_t     close_result_set;
    orcm_db_base_module_remove_fn_t               remove;
    orcm_db_base_module_fetch_function_fn_t       fetch_function;
};

typedef struct orcm_db_base_module_t orcm_db_base_module_t;

typedef struct {
    orcm_db_base_API_open_fn_t                 open;
    orcm_db_base_API_close_fn_t                close;
    orcm_db_base_API_store_fn_t                store;
    orcm_db_base_API_store_new_fn_t            store_new;
    orcm_db_base_API_record_data_samples_fn_t  record_data_samples;
    orcm_db_base_API_update_node_features_fn_t update_node_features;
    orcm_db_base_API_record_diag_test_fn_t     record_diag_test;
    orcm_db_base_API_commit_fn_t               commit;
    orcm_db_base_API_rollback_fn_t             rollback;
    orcm_db_base_API_fetch_fn_t                fetch;
    orcm_db_base_API_get_num_rows_fn_t         get_num_rows;
    orcm_db_base_API_get_next_row_fn_t         get_next_row;
    orcm_db_base_API_close_result_set_fn_t     close_result_set;
    orcm_db_base_API_remove_fn_t               remove;
    orcm_db_base_API_fetch_function_fn_t       fetch_function;
} orcm_db_API_module_t;


/* function to determine if this component is available for use.
 * Note that we do not use the standard component open
 * function as we do not want/need return of a module.
 */
typedef bool (*mca_db_base_component_avail_fn_t)(void);

/* create and return a database module */
typedef orcm_db_base_module_t* (*mca_db_base_component_create_hdl_fn_t)(opal_list_t *props);

/* provide a chance for the component to finalize */
typedef void (*mca_db_base_component_finalize_fn_t)(void);

typedef struct {
    mca_base_component_t                  base_version;
    mca_base_component_data_t             base_data;
    int                                   priority;
    mca_db_base_component_avail_fn_t      available;
    mca_db_base_component_create_hdl_fn_t create_handle;
    mca_db_base_component_finalize_fn_t   finalize;
} orcm_db_base_component_t;
#define VALUE_INT_COLUMN_NUM 0
#define VALUE_REAL_COLUMN_NUM 1
#define VALUE_STR_COLUMN_NUM 2
/*
 * Macro for use in components that are of type db
 */
#define ORCM_DB_BASE_VERSION_2_0_0 \
    ORCM_MCA_BASE_VERSION_2_1_0("db", 2, 0, 0)

/* Global structure for accessing DB functions */
ORCM_DECLSPEC extern orcm_db_API_module_t orcm_db;  /* holds API function pointers */

END_C_DECLS

#endif
