/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2013-2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <string.h>
#include <sys/types.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/time.h>
#include <time.h>

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

#include "opal_stdint.h"
#include "opal/util/argv.h"
#include "opal/util/error.h"
#include "opal/class/opal_bitmap.h"

#include "orcm/runtime/orcm_globals.h"

#include "orcm/mca/db/base/base.h"

#include "db_odbc.h"

extern bool is_supported_opal_int_type(opal_data_type_t type);

/* Module API functions */
static int odbc_init(struct orcm_db_base_module_t *imod);
static void odbc_finalize(struct orcm_db_base_module_t *imod);
static int odbc_store_sample(struct orcm_db_base_module_t *imod,
                             const char *primary_key,
                             opal_list_t *kvs);
static int odbc_store(struct orcm_db_base_module_t *imod,
                      orcm_db_data_type_t data_type,
                      opal_list_t *input,
                      opal_list_t *ret);
static int odbc_record_data_samples(struct orcm_db_base_module_t *imod,
                                    const char *hostname,
                                    const struct timeval *time_stamp,
                                    const char *data_group,
                                    opal_list_t *samples);
static int odbc_update_node_features(struct orcm_db_base_module_t *imod,
                                     const char *hostname,
                                     opal_list_t *features);
static int odbc_record_diag_test(struct orcm_db_base_module_t *imod,
                                 const char *hostname,
                                 const char *diag_type,
                                 const char *diag_subtype,
                                 const struct tm *start_time,
                                 const struct tm *end_time,
                                 const int *component_index,
                                 const char *test_result,
                                 opal_list_t *test_params);
static int odbc_commit(struct orcm_db_base_module_t *imod);
static int odbc_rollback(struct orcm_db_base_module_t *imod);
static int odbc_fetch(struct orcm_db_base_module_t *imod,
                      const char* view,
                      opal_list_t *filters,
                      opal_list_t *output);
static int odbc_get_num_rows(struct orcm_db_base_module_t *imod,
                             int rshandle,
                             int *num_rows);
static int odbc_get_next_row(struct orcm_db_base_module_t *imod,
                             int rshandle,
                             opal_list_t *row);
static int odbc_close_result_set(struct orcm_db_base_module_t *imod,
                                 int rshandle);
static int odbc_remove(struct orcm_db_base_module_t *imod,
                      const char *primary_key,
                      const char *key);

/* Internal helper functions */
static int odbc_store_data_sample(mca_db_odbc_module_t *mod,
                                  opal_list_t *input,
                                  opal_list_t *out);
static int odbc_store_node_features(mca_db_odbc_module_t *mod,
                                    opal_list_t *input,
                                    opal_list_t *out);
static int odbc_store_diag_test(mca_db_odbc_module_t *mod,
                                opal_list_t *input,
                                opal_list_t *out);

static void odbc_error_info(SQLSMALLINT handle_type, SQLHANDLE handle);
static void tm_to_sql_timestamp(SQL_TIMESTAMP_STRUCT *sql_timestamp,
                                const struct tm *time_info);
static bool tv_to_sql_timestamp(SQL_TIMESTAMP_STRUCT *sql_timestamp,
                                const struct timeval *time);

mca_db_odbc_module_t mca_db_odbc_module = {
    {
        odbc_init,
        odbc_finalize,
        odbc_store_sample,
        odbc_store,
        odbc_record_data_samples,
        odbc_update_node_features,
        odbc_record_diag_test,
        odbc_commit,
        odbc_rollback,
        odbc_fetch,
        odbc_get_num_rows,
        odbc_get_next_row,
        odbc_close_result_set,
        odbc_remove
    },
};

#define ERROR_MSG_FMT_INIT(mod, msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Connection failed: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "\tDSN: %s", mod->odbcdsn); \
    opal_output(0, "***********************************************");

static int odbc_init(struct orcm_db_base_module_t *imod)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    char **login = NULL;
    SQLRETURN ret;

    mod->results_sets = OBJ_NEW(opal_pointer_array_t);

    /* break the user info into its login parts */
    login = opal_argv_split(mod->user, ':');
    if (2 != opal_argv_count(login)) {
        ERROR_MSG_FMT_INIT(mod, "User info is invalid: %s", mod->user);
        opal_argv_free(login);
        return ORCM_ERR_BAD_PARAM;
    }

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &mod->envhandle);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_argv_free(login);
        mod->envhandle = NULL;
        ERROR_MSG_FMT_INIT(mod, "SQLAllocHandle returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }

    ret = SQLSetEnvAttr(mod->envhandle, SQL_ATTR_ODBC_VERSION,
                        (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_argv_free(login);
        SQLFreeHandle(SQL_HANDLE_ENV, mod->envhandle);
        mod->envhandle = NULL;
        ERROR_MSG_FMT_INIT(mod, "SQLSetEnvAttr returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }

    ret = SQLAllocHandle(SQL_HANDLE_DBC, mod->envhandle, &mod->dbhandle);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_argv_free(login);
        mod->dbhandle = NULL;
        SQLFreeHandle(SQL_HANDLE_ENV, mod->envhandle);
        mod->envhandle = NULL;
        ERROR_MSG_FMT_INIT(mod, "SQLAllocHandle returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }

    ret = SQLSetConnectAttr(mod->dbhandle, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_argv_free(login);
        SQLFreeHandle(SQL_HANDLE_DBC, mod->dbhandle);
        mod->dbhandle = NULL;
        SQLFreeHandle(SQL_HANDLE_ENV, mod->envhandle);
        mod->envhandle = NULL;
        ERROR_MSG_FMT_INIT(mod, "SQLSetConnectAttr returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }

    ret = SQLConnect(mod->dbhandle, (SQLCHAR *)mod->odbcdsn, SQL_NTS,
                     (SQLCHAR *)login[0], SQL_NTS,
                     (SQLCHAR *)login[1], SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_argv_free(login);
        SQLFreeHandle(SQL_HANDLE_DBC, mod->dbhandle);
        mod->dbhandle = NULL;
        SQLFreeHandle(SQL_HANDLE_ENV, mod->envhandle);
        mod->envhandle = NULL;
        ERROR_MSG_FMT_INIT(mod, "SQLConnect returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }
    opal_argv_free(login);

    ret = SQLSetConnectAttr(mod->dbhandle, SQL_ATTR_AUTOCOMMIT,
                            (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_DBC, mod->dbhandle);
        mod->dbhandle = NULL;
        SQLFreeHandle(SQL_HANDLE_ENV, mod->envhandle);
        mod->envhandle = NULL;
        ERROR_MSG_FMT_INIT(mod, "SQLSetConnectAttr returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }

    opal_output_verbose(5, orcm_db_base_framework.framework_output,
                        "db:odbc: Connection established to %s",
                        mod->odbcdsn);

    return ORCM_SUCCESS;
}

static void odbc_finalize(struct orcm_db_base_module_t *imod)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;

    if (NULL != mod->table) {
        free(mod->table);
    }
    if (NULL != mod->user) {
        free(mod->user);
    }

    if (NULL != mod->dbhandle) {
        SQLFreeHandle(SQL_HANDLE_DBC, mod->dbhandle);
    }

    if (NULL != mod->envhandle) {
        SQLFreeHandle(SQL_HANDLE_ENV, mod->envhandle);
    }
    OBJ_RELEASE(mod->results_sets);
    mod->results_sets = NULL;

    free(mod);
}

#define ERR_MSG_STORE(msg) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to record data sample: "); \
    opal_output(0, msg); \
    opal_output(0, "***********************************************");

#define ERR_MSG_FMT_STORE(msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to record data sample: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "***********************************************");

#define ERR_MSG_FMT_SQL_STORE(handle_type, handle, msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to record data sample: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "ODBC error details:"); \
    odbc_error_info(handle_type, handle); \
    opal_output(0, "***********************************************");

static int odbc_store(struct orcm_db_base_module_t *imod,
                      orcm_db_data_type_t data_type,
                      opal_list_t *input,
                      opal_list_t *ret)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    int rc = ORCM_SUCCESS;

    switch (data_type) {
    case ORCM_DB_ENV_DATA:
        rc = odbc_store_data_sample(mod, input, ret);
        break;
    case ORCM_DB_INVENTORY_DATA:
        rc = odbc_store_node_features(mod, input, ret);
        break;
    case ORCM_DB_DIAG_DATA:
        rc = odbc_store_diag_test(mod, input, ret);
        break;
    default:
        return ORCM_ERR_NOT_IMPLEMENTED;
    }

    return rc;
}

static int odbc_store_sample(struct orcm_db_base_module_t *imod,
                             const char *data_group,
                             opal_list_t *kvs)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    int rc = ORCM_SUCCESS;

    opal_value_t *kv;
    opal_value_t *timestamp_item = NULL;
    opal_value_t *hostname_item = NULL;
    struct tm time_info;

    SQL_TIMESTAMP_STRUCT sampletime;
    char hostname[256];

    char **data_item_argv = NULL;
    int argv_count;
    orcm_db_item_t item;
    orcm_db_item_type_t prev_type = ORCM_DB_ITEM_INTEGER;
    bool change_value_binding = true;
    SQLLEN null_len = SQL_NULL_DATA;

    SQLRETURN ret;
    SQLHSTMT stmt = SQL_NULL_HSTMT;

    bool local_tran_started = false;

    if (NULL == data_group) {
        ERR_MSG_STORE("No data group specified");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == kvs) {
        ERR_MSG_STORE("No value list specified");
        return ORCM_ERR_BAD_PARAM;
    }

    /* First, retrieve the time stamp and the hostname from the list */
    OPAL_LIST_FOREACH(kv, kvs, opal_value_t) {
        if (!strcmp(kv->key, "ctime")) {
            switch (kv->type) {
            case OPAL_TIMEVAL:
            case OPAL_TIME:
                if (!tv_to_sql_timestamp(&sampletime, &kv->data.tv)) {
                    ERR_MSG_STORE("Failed to convert time stamp value");
                    return ORCM_ERR_BAD_PARAM;
                }
                break;
            case OPAL_STRING:
                /* Note: assuming "%F %T%z" format and ignoring sub second
                resolution when passed as a string */
                strptime(kv->data.string, "%F %T%z", &time_info);
                tm_to_sql_timestamp(&sampletime, &time_info);
                break;
            default:
                ERR_MSG_STORE("Invalid value type specified for time stamp");
                return ORCM_ERR_BAD_PARAM;
            }
            timestamp_item = kv;
        } else if (!strcmp(kv->key, "hostname")) {
            if (OPAL_STRING == kv->type) {
                strncpy(hostname, kv->data.string, sizeof(hostname) - 1);
                hostname[sizeof(hostname) - 1] = '\0';
            } else {
                ERR_MSG_STORE("Invalid value type specified for hostname");
                return ORCM_ERR_BAD_PARAM;
            }
            hostname_item = kv;
        }

        if (NULL != timestamp_item && NULL != hostname_item) {
            break;
        }
    }

    if (NULL == timestamp_item) {
        ERR_MSG_STORE("No time stamp provided");
        return ORCM_ERR_BAD_PARAM;
    }
    if (NULL == hostname_item) {
        ERR_MSG_STORE("No hostname provided");
        return ORCM_ERR_BAD_PARAM;
    }

    /* Remove these from the list to avoid processing them again */
    opal_list_remove_item(kvs, (opal_list_item_t *)timestamp_item);
    opal_list_remove_item(kvs, (opal_list_item_t *)hostname_item);
    OBJ_RELEASE(timestamp_item);
    OBJ_RELEASE(hostname_item);

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_STORE("SQLAllocHandle returned: %d", ret);
        return ORCM_ERROR;
    }

    /*
     * 1.- hostname
     * 2.- data group
     * 3.- data item
     * 4.- time stamp
     * 5.- data type ID
     * 6.- integer value
     * 7.- real value
     * 8.- string value
     * 9.- units
     */
    ret = SQLPrepare(stmt,
                     (SQLCHAR *)
                     "{call record_data_sample(?, ?, ?, ?, ?, ?, ?, ?, ?)}",
                     SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLPrepare returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind hostname parameter. */
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)hostname, strlen(hostname), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 1 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind data group parameter. */
    ret = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)data_group, strlen(data_group),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 2 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind time stamp parameter. */
    ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                           SQL_TYPE_TIMESTAMP, 0, 0, (SQLPOINTER)&sampletime,
                           sizeof(sampletime), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 4 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind data type parameter. */
    ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                           0, 0, (SQLPOINTER)&item.opal_type,
                           sizeof(item.opal_type), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 5 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind integer value parameter (assuming the value is integer for now). */
    ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT,
                           0, 0, (SQLPOINTER)&item.value.value_int,
                           sizeof(item.value.value_int), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 6 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind real value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 7 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind string value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
        goto cleanup_and_exit;
    }

    OPAL_LIST_FOREACH(kv, kvs, opal_value_t) {
        ret = opal_value_to_orcm_db_item(kv, &item);

        if (ORCM_SUCCESS != ret) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_STORE("Unsupported value type");
            goto cleanup_and_exit;
        }
        change_value_binding = prev_type != item.item_type;
        prev_type = item.item_type;

        /* kv->key will contain: <data item>:<units> */
        data_item_argv = opal_argv_split(kv->key, ':');
        argv_count = opal_argv_count(data_item_argv);
        if (argv_count == 0) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_STORE("No data item specified");
            goto cleanup_and_exit;
        }
        /* Bind the data item parameter. */
        ret = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR,
                               SQL_VARCHAR, 0, 0, (SQLPOINTER)data_item_argv[0],
                               strlen(data_item_argv[0]), NULL);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_STORE("SQLBindParameter 3 returned: %d", ret);
            goto cleanup_and_exit;
        }

        if (argv_count > 1) {
            /* Bind the units parameter. */
            ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)data_item_argv[1],
                                   strlen(data_item_argv[1]), NULL);
        } else {
            /* No units provided, bind NULL. */
            ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0, NULL, 0, &null_len);
        }
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_STORE("SQLBindParameter 9 returned: %d", ret);
            goto cleanup_and_exit;
        }

        if (change_value_binding) {
            switch (item.item_type) {
            case ORCM_DB_ITEM_INTEGER:
                /* Value is integer, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0,
                                       (SQLPOINTER)&item.value.value_int,
                                       sizeof(item.value.value_int), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_REAL:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is real, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0,
                                       (SQLPOINTER)&item.value.value_real,
                                       sizeof(item.value.value_real), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_STRING:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is string, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0,
                                       (SQLPOINTER)item.value.value_str,
                                       strlen(item.value.value_str), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            default:
                rc = ORCM_ERROR;
                ERR_MSG_STORE("An unexpected error has occurred while "
                              "processing the values");
                goto cleanup_and_exit;
            }
        } else if (ORCM_DB_ITEM_STRING == item.item_type) {
            /* No need to change the binding for all the values, just update
             * the string binding. */
            ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)item.value.value_str,
                                   strlen(item.value.value_str), NULL);
            if (!(SQL_SUCCEEDED(ret))) {
                rc = ORCM_ERROR;
                ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
                goto cleanup_and_exit;
            }
        }

        ret = SQLExecute(stmt);
        if (!(SQL_SUCCEEDED(ret))) {
            ERR_MSG_FMT_SQL_STORE(SQL_HANDLE_STMT, stmt,
                                  "SQLExecute returned: %d", ret);
            rc = ORCM_ERROR;
            goto cleanup_and_exit;
        }

        local_tran_started = true;

        SQLCloseCursor(stmt);

        opal_argv_free(data_item_argv);
        data_item_argv = NULL;
    }

    if (mod->autocommit) {
        ret = SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_COMMIT);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_SQL_STORE(SQL_HANDLE_DBC, mod->dbhandle,
                                  "SQLEndTran returned: %d", ret);
            goto cleanup_and_exit;
        }
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "odbc_store_sample succeeded");

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_ROLLBACK);
    }

    if (NULL != data_item_argv) {
        opal_argv_free(data_item_argv);
    }

    if (SQL_NULL_HSTMT != stmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }

    return rc;
}

static int odbc_store_data_sample(mca_db_odbc_module_t *mod,
                                  opal_list_t *input,
                                  opal_list_t *out)
{
    int rc = ORCM_SUCCESS;

    const int NUM_PARAMS = 3;
    const char *params[] = {
        "data_group",
        "ctime",
        "hostname"
    };
    opal_value_t *param_items[] = {NULL, NULL, NULL};
    opal_bitmap_t item_bm;
    size_t num_items;

    char *data_group;
    char *hostname;
    char *data_item;
    char *units;
    struct tm time_info;
    SQL_TIMESTAMP_STRUCT sampletime;

    orcm_db_item_t item;
    orcm_db_item_type_t prev_type = ORCM_DB_ITEM_INTEGER;
    bool change_value_binding = true;

    orcm_value_t *mv;
    opal_value_t *kv;
    int i;

    SQLLEN null_len = SQL_NULL_DATA;
    SQLRETURN ret;
    SQLHSTMT stmt = SQL_NULL_HSTMT;

    bool local_tran_started = false;

    if (NULL == input) {
        ERR_MSG_STORE("No parameters provided");
        return ORCM_ERR_BAD_PARAM;
    }

    num_items = opal_list_get_size(input);
    OBJ_CONSTRUCT(&item_bm, opal_bitmap_t);
    opal_bitmap_init(&item_bm, (int)num_items);

    /* Get the main parameters form the list */
    orcm_util_find_items(params, NUM_PARAMS, input, param_items, &item_bm);

    /* Check the parameters */
    if (NULL == param_items[0]) {
        ERR_MSG_STORE("No data group provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    if (NULL == param_items[1]) {
        ERR_MSG_STORE("No time stamp provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }
    if (NULL == param_items[2]) {
        ERR_MSG_STORE("No hostname provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[0];
    if (OPAL_STRING == kv->type) {
        data_group = kv->data.string;
    } else {
        ERR_MSG_STORE("Invalid value type specified for data group");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[1];
    switch (kv->type) {
    case OPAL_TIMEVAL:
    case OPAL_TIME:
        if (!tv_to_sql_timestamp(&sampletime, &kv->data.tv)) {
            ERR_MSG_STORE("Failed to convert time stamp value");
            rc = ORCM_ERR_BAD_PARAM;
            goto cleanup_and_exit;
        }
        break;
    case OPAL_STRING:
        /* Note: assuming "%F %T%z" format and ignoring sub second
        resolution when passed as a string */
        strptime(kv->data.string, "%F %T%z", &time_info);
        tm_to_sql_timestamp(&sampletime, &time_info);
        break;
    default:
        ERR_MSG_STORE("Invalid value type specified for time stamp");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[2];
    if (OPAL_STRING == kv->type) {
        hostname = kv->data.string;
    } else {
        ERR_MSG_STORE("Invalid value type specified for hostname");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_STORE("SQLAllocHandle returned: %d", ret);
        OBJ_DESTRUCT(&item_bm);
        return ORCM_ERROR;
    }

    if (num_items <= (size_t)NUM_PARAMS) {
        ERR_MSG_STORE("No data samples provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    /*
     * 1.- hostname
     * 2.- data group
     * 3.- data item
     * 4.- time stamp
     * 5.- data type ID
     * 6.- integer value
     * 7.- real value
     * 8.- string value
     * 9.- units
     */
    ret = SQLPrepare(stmt,
                     (SQLCHAR *)
                     "{call record_data_sample(?, ?, ?, ?, ?, ?, ?, ?, ?)}",
                     SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLPrepare returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind hostname parameter. */
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)hostname, strlen(hostname), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 1 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind data group parameter. */
    ret = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)data_group, strlen(data_group),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 2 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind time stamp parameter. */
    ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                           SQL_TYPE_TIMESTAMP, 0, 0, (SQLPOINTER)&sampletime,
                           sizeof(sampletime), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 4 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind data type parameter. */
    ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                           0, 0, (SQLPOINTER)&item.opal_type,
                           sizeof(item.opal_type), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 5 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind integer value parameter (assuming the value is integer for now). */
    ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT,
                           0, 0, (SQLPOINTER)&item.value.value_int,
                           sizeof(item.value.value_int), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 6 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind real value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 7 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind string value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Store all the samples passed in the list */
    i = 0;
    OPAL_LIST_FOREACH(mv, input, orcm_value_t) {
        /* Ignore the items that have already been processed */
        if (opal_bitmap_is_set_bit(&item_bm, i)) {
            i++;
            continue;
        }
        ret = opal_value_to_orcm_db_item(&mv->value, &item);

        if (ORCM_SUCCESS != ret) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_STORE("Unsupported value type");
            goto cleanup_and_exit;
        }
        change_value_binding = prev_type != item.item_type;
        prev_type = item.item_type;

        data_item = mv->value.key;
        units = mv->units;
        if (NULL == data_item) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_STORE("No data item specified");
            goto cleanup_and_exit;
        }

        /* Bind the data item parameter. */
        ret = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR,
                               SQL_VARCHAR, 0, 0, (SQLPOINTER)data_item,
                               strlen(data_item), NULL);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_STORE("SQLBindParameter 3 returned: %d", ret);
            goto cleanup_and_exit;
        }

        if (NULL != units) {
            /* Bind the units parameter. */
            ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)units, strlen(units), NULL);
        } else {
            /* No units provided, bind NULL. */
            ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0, NULL, 0, &null_len);
        }
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_STORE("SQLBindParameter 9 returned: %d", ret);
            goto cleanup_and_exit;
        }

        if (change_value_binding) {
            switch (item.item_type) {
            case ORCM_DB_ITEM_INTEGER:
                /* Value is integer, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0,
                                       (SQLPOINTER)&item.value.value_int,
                                       sizeof(item.value.value_int), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_REAL:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is real, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0,
                                       (SQLPOINTER)&item.value.value_real,
                                       sizeof(item.value.value_real), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_STRING:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is string, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0,
                                       (SQLPOINTER)item.value.value_str,
                                       strlen(item.value.value_str), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            default:
                rc = ORCM_ERROR;
                ERR_MSG_STORE("An unexpected error has occurred while "
                              "processing the values");
                goto cleanup_and_exit;
            }
        } else if (ORCM_DB_ITEM_STRING == item.item_type) {
            /* No need to change the binding for all the values, just update
             * the string binding. */
            ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)item.value.value_str,
                                   strlen(item.value.value_str), NULL);
            if (!(SQL_SUCCEEDED(ret))) {
                rc = ORCM_ERROR;
                ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
                goto cleanup_and_exit;
            }
        }

        ret = SQLExecute(stmt);
        if (!(SQL_SUCCEEDED(ret))) {
            ERR_MSG_FMT_SQL_STORE(SQL_HANDLE_STMT, stmt,
                                  "SQLExecute returned: %d", ret);
            rc = ORCM_ERROR;
            goto cleanup_and_exit;
        }

        local_tran_started = true;

        SQLCloseCursor(stmt);
        i++;
    }

    if (mod->autocommit) {
        ret = SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_COMMIT);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_SQL_STORE(SQL_HANDLE_DBC, mod->dbhandle,
                                  "SQLEndTran returned: %d", ret);
            goto cleanup_and_exit;
        }
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "odbc_store_sample succeeded");

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_ROLLBACK);
    }

    if (SQL_NULL_HSTMT != stmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }

    OBJ_DESTRUCT(&item_bm);
    return rc;
}

static int odbc_record_data_samples(struct orcm_db_base_module_t *imod,
                                    const char *hostname,
                                    const struct timeval *time_stamp,
                                    const char *data_group,
                                    opal_list_t *samples)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    int rc = ORCM_SUCCESS;

    orcm_value_t *mv;

    SQL_TIMESTAMP_STRUCT sampletime;
    orcm_db_item_t item;
    orcm_db_item_type_t prev_type = ORCM_DB_ITEM_INTEGER;
    bool change_value_binding = true;
    SQLLEN null_len = SQL_NULL_DATA;

    SQLRETURN ret;
    SQLHSTMT stmt = SQL_NULL_HSTMT;

    bool local_tran_started = false;

    if (NULL == data_group) {
        ERR_MSG_STORE("No data group provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == hostname) {
        ERR_MSG_STORE("No hostname provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == time_stamp) {
        ERR_MSG_STORE("No time stamp provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == samples) {
        ERR_MSG_STORE("No value list provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (!tv_to_sql_timestamp(&sampletime, time_stamp)) {
        ERR_MSG_STORE("Failed to convert time stamp value");
        return ORCM_ERR_BAD_PARAM;
    }

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_STORE("SQLAllocHandle returned: %d", ret);
        return ORCM_ERROR;
    }

    /*
     * 1.- hostname
     * 2.- data group
     * 3.- data item
     * 4.- time stamp
     * 5.- data type ID
     * 6.- integer value
     * 7.- real value
     * 8.- string value
     * 9.- units
     */
    ret = SQLPrepare(stmt,
                     (SQLCHAR *)
                     "{call record_data_sample(?, ?, ?, ?, ?, ?, ?, ?, ?)}",
                     SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLPrepare returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind hostname parameter. */
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)hostname, strlen(hostname), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 1 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind data group parameter. */
    ret = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)data_group, strlen(data_group),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 2 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind time stamp parameter. */
    ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                           SQL_TYPE_TIMESTAMP, 0, 0, (SQLPOINTER)&sampletime,
                           sizeof(sampletime), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 4 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind data type parameter. */
    ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                           0, 0, (SQLPOINTER)&item.opal_type,
                           sizeof(item.opal_type), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 5 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind integer value parameter (assuming the value is integer for now). */
    ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT,
                           0, 0, (SQLPOINTER)&item.value.value_int,
                           sizeof(item.value.value_int), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 6 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind real value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 7 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind string value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
        goto cleanup_and_exit;
    }

    OPAL_LIST_FOREACH(mv, samples, orcm_value_t) {
        if (NULL == mv->value.key || 0 == strlen(mv->value.key)) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_STORE("Key or data item name not provided for value");
            goto cleanup_and_exit;
        }

        ret = opal_value_to_orcm_db_item(&mv->value, &item);
        if (ORCM_SUCCESS != ret) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_STORE("Unsupported value type");
            goto cleanup_and_exit;
        }
        change_value_binding = prev_type != item.item_type;
        prev_type = item.item_type;

        /* Bind the data item parameter. */
        ret = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR,
                               SQL_VARCHAR, 0, 0, (SQLPOINTER)mv->value.key,
                               strlen(mv->value.key), NULL);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_STORE("SQLBindParameter 3 returned: %d", ret);
            goto cleanup_and_exit;
        }

        if (NULL != mv->units) {
            /* Bind the units parameter. */
            ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)mv->units,
                                   strlen(mv->units), NULL);
        } else {
            /* No units provided, bind NULL. */
            ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0, NULL, 0, &null_len);
        }
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_STORE("SQLBindParameter 9 returned: %d", ret);
            goto cleanup_and_exit;
        }

        if (change_value_binding) {
            switch (item.item_type) {
            case ORCM_DB_ITEM_INTEGER:
                /* Value is integer, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0,
                                       (SQLPOINTER)&item.value.value_int,
                                       sizeof(item.value.value_int), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_REAL:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is real, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0,
                                       (SQLPOINTER)&item.value.value_real,
                                       sizeof(item.value.value_real), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_STRING:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is string, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0,
                                       (SQLPOINTER)item.value.value_str,
                                       strlen(item.value.value_str), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            default:
                rc = ORCM_ERROR;
                ERR_MSG_STORE("An unexpected error has occurred while "
                              "processing the values");
                goto cleanup_and_exit;
            }
        } else if (ORCM_DB_ITEM_STRING == item.item_type) {
            /* No need to change the binding for all the values, just update
             * the string binding. */
            ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)item.value.value_str,
                                   strlen(item.value.value_str), NULL);
            if (!(SQL_SUCCEEDED(ret))) {
                rc = ORCM_ERROR;
                ERR_MSG_FMT_STORE("SQLBindParameter 8 returned: %d", ret);
                goto cleanup_and_exit;
            }
        }

        ret = SQLExecute(stmt);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_SQL_STORE(SQL_HANDLE_STMT, stmt,
                                  "SQLExecute returned: %d", ret);
            goto cleanup_and_exit;
        }

        local_tran_started = true;

        SQLCloseCursor(stmt);
    }

    if (mod->autocommit) {
        ret = SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_COMMIT);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_SQL_STORE(SQL_HANDLE_DBC, mod->dbhandle,
                                  "SQLEndTran returned: %d", ret);
            goto cleanup_and_exit;
        }
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "odbc_record_data_samples succeeded");

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_ROLLBACK);
    }

    if (SQL_NULL_HSTMT != stmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }

    return rc;
}

#define ERR_MSG_UNF(msg) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to update node features"); \
    opal_output(0, msg); \
    opal_output(0, "***********************************************");

#define ERR_MSG_FMT_UNF(msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to update node features"); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "***********************************************");

#define ERR_MSG_FMT_SQL_UNF(handle_type, handle, msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to update node features"); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "ODBC error details:"); \
    odbc_error_info(handle_type, handle); \
    opal_output(0, "***********************************************");

static int odbc_update_node_features(struct orcm_db_base_module_t *imod,
                                     const char *hostname,
                                     opal_list_t *features)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    int rc = ORCM_SUCCESS;

    orcm_value_t *mv;
    orcm_db_item_t item;
    orcm_db_item_type_t prev_type = ORCM_DB_ITEM_INTEGER;
    bool change_value_binding = true;
    SQLLEN null_len = SQL_NULL_DATA;

    SQLRETURN ret;
    SQLHSTMT stmt = SQL_NULL_HSTMT;

    bool local_tran_started = false;

    if (NULL == hostname) {
        ERR_MSG_UNF("No hostname provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == features) {
        ERR_MSG_UNF("No node features provided");
        return ORCM_ERR_BAD_PARAM;
    }

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_UNF("SQLAllocHandle returned: %d", ret);
        return ORCM_ERROR;
    }

    /*
     * 1 p_hostname character varying,
     * 2 p_feature character varying,
     * 3 p_data_type_id integer,
     * 4 p_value_int bigint,
     * 5 p_value_real double precision,
     * 6 p_value_str character varying,
     * 7 p_units character varying
     * */
    ret = SQLPrepare(stmt,
                     (SQLCHAR *)"{call set_node_feature(?, ?, ?, ?, ?, ?, ?)}",
                     SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLPrepare returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind hostname parameter. */
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)hostname, strlen(hostname), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 1 returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind data type parameter. */
    ret = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                           0, 0, (SQLPOINTER)&item.opal_type,
                           sizeof(item.opal_type), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 3 returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind integer value parameter (assuming the value is integer for now). */
    ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                           SQL_BIGINT, 0, 0, (SQLPOINTER)&item.value.value_int,
                           sizeof(item.value.value_int), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 4 returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind real value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 5 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind string value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 6 returned: %d", ret);
        goto cleanup_and_exit;
    }

    OPAL_LIST_FOREACH(mv, features, orcm_value_t) {
        if (NULL == mv->value.key || 0 == strlen(mv->value.key)) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_UNF("Key or node feature name not provided for value");
            goto cleanup_and_exit;
        }

        ret = opal_value_to_orcm_db_item(&mv->value, &item);
        if (ORCM_SUCCESS != ret) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_UNF("Unsupported value type");
            goto cleanup_and_exit;
        }
        change_value_binding = prev_type != item.item_type;
        prev_type = item.item_type;

        /* Bind the feature parameter. */
        ret = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR,
                               SQL_VARCHAR, 0, 0, (SQLPOINTER)mv->value.key,
                               strlen(mv->value.key), NULL);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_UNF("SQLBindParameter 2 returned: %d", ret);
            goto cleanup_and_exit;
        }

        if (change_value_binding) {
            switch (item.item_type) {
            case ORCM_DB_ITEM_INTEGER:
                /* Value is integer, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0,
                                       (SQLPOINTER)&item.value.value_int,
                                       sizeof(item.value.value_int), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 4 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 5 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_REAL:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 4 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is real, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0,
                                       (SQLPOINTER)&item.value.value_real,
                                       sizeof(item.value.value_real), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 5 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_STRING:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 4 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 5 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is string, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0,
                                       (SQLPOINTER)item.value.value_str,
                                       strlen(item.value.value_str), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            default:
                rc = ORCM_ERROR;
                ERR_MSG_STORE("An unexpected error has occurred while "
                              "processing the values");
                goto cleanup_and_exit;
            }
        } else if (ORCM_DB_ITEM_STRING == item.item_type) {
            /* No need to change the binding for all the values, just update
             * the string binding. */
            ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)item.value.value_str,
                                   strlen(item.value.value_str), NULL);
            if (!(SQL_SUCCEEDED(ret))) {
                rc = ORCM_ERROR;
                ERR_MSG_FMT_UNF("SQLBindParameter 6 returned: %d", ret);
                goto cleanup_and_exit;
            }
        }

        if (NULL != mv->units) {
            /* Bind the units parameter. */
            ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)mv->units,
                                   strlen(mv->units), NULL);
        } else {
            /* No units provided, bind NULL. */
            ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0, NULL, 0, &null_len);
        }
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_UNF("SQLBindParameter 7 returned: %d", ret);
            goto cleanup_and_exit;
        }

        ret = SQLExecute(stmt);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_SQL_UNF(SQL_HANDLE_STMT, stmt,
                                "SQLExecute returned: %d", ret);
            goto cleanup_and_exit;
        }

        local_tran_started = true;

        SQLCloseCursor(stmt);
    }

    if (mod->autocommit) {
        ret = SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_COMMIT);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_SQL_UNF(SQL_HANDLE_DBC, mod->dbhandle,
                                "SQLEndTran returned: %d", ret);
            goto cleanup_and_exit;
        }
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "odbc_update_node_features succeeded");

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_ROLLBACK);
    }

    if (SQL_NULL_HSTMT != stmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }

    return rc;
}

static int odbc_store_node_features(mca_db_odbc_module_t *mod,
                                    opal_list_t *input,
                                    opal_list_t *out)
{
    int rc = ORCM_SUCCESS;

    const int NUM_PARAMS = 1;
    const char *params[] = {
        "hostname"
    };
    opal_value_t *param_items[] = {NULL};
    opal_bitmap_t item_bm;

    char *hostname = NULL;

    orcm_db_item_t item;
    orcm_db_item_type_t prev_type = ORCM_DB_ITEM_INTEGER;
    bool change_value_binding = true;

    opal_value_t *kv;
    orcm_value_t *mv;

    SQLLEN null_len = SQL_NULL_DATA;
    SQLRETURN ret;
    SQLHSTMT stmt = SQL_NULL_HSTMT;

    size_t num_items;
    int i;

    bool local_tran_started = false;

    if (NULL == input) {
        ERR_MSG_UNF("No parameters provided");
        return ORCM_ERR_BAD_PARAM;
    }

    num_items = opal_list_get_size(input);
    OBJ_CONSTRUCT(&item_bm, opal_bitmap_t);
    opal_bitmap_init(&item_bm, (int)num_items);

    /* Get the main parameters from the list */
    orcm_util_find_items(params, NUM_PARAMS, input, param_items, &item_bm);

    /* Check parameters */
    if (NULL == param_items[0]) {
        ERR_MSG_UNF("No hostname provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[0];
    if (OPAL_STRING == kv->type) {
        hostname = kv->data.string;
    } else {
        ERR_MSG_UNF("Invalid value type specified for hostname");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    if (num_items <= (size_t)NUM_PARAMS) {
        ERR_MSG_UNF("No node features provided");
        goto cleanup_and_exit;
    }

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_UNF("SQLAllocHandle returned: %d", ret);
        OBJ_DESTRUCT(&item_bm);
        return ORCM_ERROR;
    }

    /*
     * 1 p_hostname character varying,
     * 2 p_feature character varying,
     * 3 p_data_type_id integer,
     * 4 p_value_int bigint,
     * 5 p_value_real double precision,
     * 6 p_value_str character varying,
     * 7 p_units character varying
     * */
    ret = SQLPrepare(stmt,
                     (SQLCHAR *)"{call set_node_feature(?, ?, ?, ?, ?, ?, ?)}",
                     SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLPrepare returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind hostname parameter. */
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                          0, 0, (SQLPOINTER)hostname, strlen(hostname), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 1 returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind data type parameter. */
    ret = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                           0, 0, (SQLPOINTER)&item.opal_type,
                           sizeof(item.opal_type), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 3 returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind integer value parameter (assuming the value is integer for now). */
    ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                           SQL_BIGINT, 0, 0, (SQLPOINTER)&item.value.value_int,
                           sizeof(item.value.value_int), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 4 returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind real value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 5 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind string value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 6 returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Store all the node features provided in the list */
    i = 0;
    OPAL_LIST_FOREACH(mv, input, orcm_value_t) {
        /* Skip the items that have already been processed */
        if (opal_bitmap_is_set_bit(&item_bm, i)) {
            i++;
            continue;
        }

        if (NULL == mv->value.key || 0 == strlen(mv->value.key)) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_UNF("Key or node feature name not provided for value");
            goto cleanup_and_exit;
        }

        ret = opal_value_to_orcm_db_item(&mv->value, &item);
        if (ORCM_SUCCESS != ret) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_UNF("Unsupported value type");
            goto cleanup_and_exit;
        }
        change_value_binding = prev_type != item.item_type;
        prev_type = item.item_type;

        /* Bind the feature parameter. */
        ret = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR,
                               SQL_VARCHAR, 0, 0, (SQLPOINTER)mv->value.key,
                               strlen(mv->value.key), NULL);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_UNF("SQLBindParameter 2 returned: %d", ret);
            goto cleanup_and_exit;
        }

        if (change_value_binding) {
            switch (item.item_type) {
            case ORCM_DB_ITEM_INTEGER:
                /* Value is integer, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0,
                                       (SQLPOINTER)&item.value.value_int,
                                       sizeof(item.value.value_int), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 4 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 5 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_REAL:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 4 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is real, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0,
                                       (SQLPOINTER)&item.value.value_real,
                                       sizeof(item.value.value_real), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 5 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_STRING:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 4 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 5 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is string, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0,
                                       (SQLPOINTER)item.value.value_str,
                                       strlen(item.value.value_str), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_UNF("SQLBindParameter 6 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            default:
                rc = ORCM_ERROR;
                ERR_MSG_UNF("An unexpected error has occurred while "
                              "processing the values");
                goto cleanup_and_exit;
            }
        } else if (ORCM_DB_ITEM_STRING == item.item_type) {
            /* No need to change the binding for all the values, just update
             * the string binding. */
            ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)item.value.value_str,
                                   strlen(item.value.value_str), NULL);
            if (!(SQL_SUCCEEDED(ret))) {
                rc = ORCM_ERROR;
                ERR_MSG_FMT_UNF("SQLBindParameter 6 returned: %d", ret);
                goto cleanup_and_exit;
            }
        }

        if (NULL != mv->units) {
            /* Bind the units parameter. */
            ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)mv->units,
                                   strlen(mv->units), NULL);
        } else {
            /* No units provided, bind NULL. */
            ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0, NULL, 0, &null_len);
        }
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_UNF("SQLBindParameter 7 returned: %d", ret);
            goto cleanup_and_exit;
        }

        ret = SQLExecute(stmt);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_SQL_UNF(SQL_HANDLE_STMT, stmt,
                                "SQLExecute returned: %d", ret);
            goto cleanup_and_exit;
        }

        local_tran_started = true;

        SQLCloseCursor(stmt);
        i++;
    }

    if (mod->autocommit) {
        ret = SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_COMMIT);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_SQL_UNF(SQL_HANDLE_DBC, mod->dbhandle,
                                "SQLEndTran returned: %d", ret);
            goto cleanup_and_exit;
        }
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "odbc_update_node_features succeeded");

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_ROLLBACK);
    }

    if (SQL_NULL_HSTMT != stmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }

    OBJ_DESTRUCT(&item_bm);

    return rc;
}

#define ERR_MSG_RDT(msg) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to record diagnostic test: "); \
    opal_output(0, msg); \
    opal_output(0, "***********************************************");

#define ERR_MSG_FMT_RDT(msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to record diagnostic test: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "***********************************************");

#define ERR_MSG_FMT_SQL_RDT(handle_type, handle, msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to record diagnostic test: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "ODBC error details:"); \
    odbc_error_info(handle_type, handle); \
    opal_output(0, "***********************************************");

static int odbc_record_diag_test(struct orcm_db_base_module_t *imod,
                                 const char *hostname,
                                 const char *diag_type,
                                 const char *diag_subtype,
                                 const struct tm *start_time,
                                 const struct tm *end_time,
                                 const int *component_index,
                                 const char *test_result,
                                 opal_list_t *test_params)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    int rc = ORCM_SUCCESS;

    orcm_value_t *mv;

    SQL_TIMESTAMP_STRUCT start_time_sql;
    SQL_TIMESTAMP_STRUCT end_time_sql;

    orcm_db_item_t item;
    orcm_db_item_type_t prev_type = ORCM_DB_ITEM_INTEGER;
    bool change_value_binding = true;
    SQLLEN null_len = SQL_NULL_DATA;

    SQLRETURN ret;
    SQLHSTMT stmt = SQL_NULL_HSTMT;

    bool local_tran_started = false;

    if (NULL == hostname) {
        ERR_MSG_RDT("No hostname provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == diag_type) {
        ERR_MSG_RDT("No diagnostic type provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == diag_subtype) {
        ERR_MSG_RDT("No diagnostic subtype provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == start_time) {
        ERR_MSG_RDT("No start time provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == test_result) {
        ERR_MSG_RDT("No test result provided");
        return ORCM_ERR_BAD_PARAM;
    }

    tm_to_sql_timestamp(&start_time_sql, start_time);

    if (NULL != end_time) {
        tm_to_sql_timestamp(&end_time_sql, end_time);
    }

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_RDT("SQLAllocHandle returned: %d", ret);
        return ORCM_ERROR;
    }

    /*
     * 1.- hostname
     * 2.- diagnostic type
     * 3.- diagnostic subtype
     * 4.- start time
     * 5.- end time
     * 6.- component index
     * 7.- test result
     */
    ret = SQLPrepare(stmt,
                     (SQLCHAR *)
                     "{call record_diag_test_result(?, ?, ?, ?, ?, ?, ?)}",
                     SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLPrepare returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind hostname parameter. */
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)hostname, strlen(hostname), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 1 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind diagnostic type parameter. */
    ret = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)diag_type, strlen(diag_type),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 2 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind diagnostic subtype parameter. */
    ret = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)diag_subtype, strlen(diag_subtype),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 3 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind start time parameter. */
    ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                           SQL_TYPE_TIMESTAMP, 0, 0,
                           (SQLPOINTER)&start_time_sql, sizeof(start_time_sql),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 4 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind end time parameter. */
    if (NULL != end_time) {
        ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                               SQL_TYPE_TIMESTAMP, 0, 0,
                               (SQLPOINTER)&end_time_sql, sizeof(end_time_sql),
                               NULL);
    } else {
        ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                               SQL_TYPE_TIMESTAMP, 0, 0, NULL, 0, &null_len);
    }
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 5 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind component index parameter. */
    if (NULL != component_index) {
        ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_LONG,
                               SQL_INTEGER, 0, 0, (SQLPOINTER)component_index,
                               sizeof(component_index), NULL);
    } else {
        ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_LONG,
                               SQL_INTEGER, 0, 0, NULL, 0, &null_len);
    }
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 6 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind test result parameter. */
    ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)test_result, strlen(test_result),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 7 returned: %d", ret);
        goto cleanup_and_exit;
    }

    ret = SQLExecute(stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_SQL_RDT(SQL_HANDLE_STMT, stmt, "SQLExecute returned: %d",
                            ret);
        rc = ORCM_ERROR;
        goto cleanup_and_exit;
    }

    SQLCloseCursor(stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    stmt = SQL_NULL_HSTMT;

    local_tran_started = true;

    if (NULL == test_params) {
        /* No test parameters provided, we're done! */
        if (mod->autocommit) {
            ret = SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_COMMIT);
            if (!(SQL_SUCCEEDED(ret))) {
                rc = ORCM_ERROR;
                ERR_MSG_FMT_SQL_RDT(SQL_HANDLE_DBC, mod->dbhandle,
                                    "SQLEndTran returned: %d", ret);
                goto cleanup_and_exit;
            }
        }

        opal_output_verbose(2, orcm_db_base_framework.framework_output,
                            "odbc_record_diag_test succeeded");
        goto cleanup_and_exit;
    }

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLAllocHandle returned: %d", ret);
        goto cleanup_and_exit;
    }

    /*
     * 1.- hostname
     * 2.- diagnostic type
     * 3.- diagnostic subtype
     * 4.- start time
     * 5.- test parameter
     * 6.- data type
     * 7.- integer value
     * 8.- real value
     * 9.- string value
     * 10.- units
     */
    ret = SQLPrepare(stmt,
                     (SQLCHAR *)"{call record_diag_test_config"
                                "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)}",
                     SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLPrepare returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind hostname parameter. */
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)hostname, strlen(hostname), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 1 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind diagnostic type parameter. */
    ret = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)diag_type, strlen(diag_type),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 2 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind diagnostic subtype parameter. */
    ret = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)diag_subtype, strlen(diag_subtype),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 3 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind start time parameter. */
    ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                           SQL_TYPE_TIMESTAMP, 0, 0,
                           (SQLPOINTER)&start_time_sql, sizeof(start_time_sql),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 4 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind data type parameter. */
    ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                           0, 0, (SQLPOINTER)&item.opal_type,
                           sizeof(item.opal_type), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 6 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind integer value parameter (assuming the value is integer for now). */
    ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                           SQL_BIGINT, 0, 0, (SQLPOINTER)&item.value.value_int,
                           sizeof(item.value.value_int), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 7 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind real value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 8 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind string value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_UNF("SQLBindParameter 9 returned: %d", ret);
        goto cleanup_and_exit;
    }

    OPAL_LIST_FOREACH(mv, test_params, orcm_value_t) {
        if (NULL == mv->value.key || 0 == strlen(mv->value.key)) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_RDT("Key or test parameter name not provided for value");
            goto cleanup_and_exit;
        }

        ret = opal_value_to_orcm_db_item(&mv->value, &item);
        if (ORCM_SUCCESS != ret) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_RDT("Unsupported value type");
            goto cleanup_and_exit;
        }
        change_value_binding = prev_type != item.item_type;
        prev_type = item.item_type;

        /* Bind the test parameter. */
        ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR,
                               SQL_VARCHAR, 0, 0, (SQLPOINTER)mv->value.key,
                               strlen(mv->value.key), NULL);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_RDT("SQLBindParameter 5 returned: %d", ret);
            goto cleanup_and_exit;
        }

        if (NULL != mv->units) {
            /* Bind the units parameter. */
            ret = SQLBindParameter(stmt, 10, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)mv->units,
                                   strlen(mv->units), NULL);
        } else {
            /* No units provided, bind NULL. */
            ret = SQLBindParameter(stmt, 10, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0, NULL, 0, &null_len);
        }
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_RDT("SQLBindParameter 10 returned: %d", ret);
            goto cleanup_and_exit;
        }

        if (change_value_binding) {
            switch (item.item_type) {
            case ORCM_DB_ITEM_INTEGER:
                /* Value is integer, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0,
                                       (SQLPOINTER)&item.value.value_int,
                                       sizeof(item.value.value_int), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 9 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_REAL:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is real, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0,
                                       (SQLPOINTER)&item.value.value_real,
                                       sizeof(item.value.value_real), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 9 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_STRING:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is string, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0,
                                       (SQLPOINTER)item.value.value_str,
                                       strlen(item.value.value_str), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 9 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            default:
                rc = ORCM_ERROR;
                ERR_MSG_RDT("An unexpected error has occurred while "
                            "processing the values");
                goto cleanup_and_exit;
            }
        } else if (ORCM_DB_ITEM_STRING == item.item_type) {
            /* No need to change the binding for all the values, just update
             * the string binding. */
            ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)item.value.value_str,
                                   strlen(item.value.value_str), NULL);
            if (!(SQL_SUCCEEDED(ret))) {
                rc = ORCM_ERROR;
                ERR_MSG_FMT_RDT("SQLBindParameter 9 returned: %d", ret);
                goto cleanup_and_exit;
            }
        }

        ret = SQLExecute(stmt);
        if (!(SQL_SUCCEEDED(ret))) {
            ERR_MSG_FMT_SQL_RDT(SQL_HANDLE_STMT, stmt,
                                  "SQLExecute returned: %d", ret);
            rc = ORCM_ERROR;
            goto cleanup_and_exit;
        }

        SQLCloseCursor(stmt);
    }

    if (mod->autocommit) {
        ret = SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_COMMIT);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_SQL_RDT(SQL_HANDLE_DBC, mod->dbhandle,
                                "SQLEndTran returned: %d", ret);
            goto cleanup_and_exit;
        }
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "odbc_record_diag_test succeeded");

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_ROLLBACK);
    }

    if (SQL_NULL_HSTMT != stmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }

    return rc;
}

static int odbc_store_diag_test(mca_db_odbc_module_t *mod,
                                opal_list_t *input,
                                opal_list_t *out)
{
    int rc = ORCM_SUCCESS;

    const int NUM_PARAMS = 7;
    const char *params[] = {
        "hostname",
        "diag_type",
        "diag_subtype",
        "start_time",
        "test_result",
        "end_time",
        "component_index"
    };
    opal_value_t *param_items[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    opal_bitmap_t item_bm;
    size_t num_params_found;
    size_t num_items;

    char *hostname = NULL;
    char *diag_type = NULL;
    char *diag_subtype = NULL;
    char *test_result = NULL;
    int *component_index = NULL;

    struct tm time_info;
    SQL_TIMESTAMP_STRUCT start_time_sql;
    SQL_TIMESTAMP_STRUCT end_time_sql;

    orcm_db_item_t item;
    orcm_db_item_type_t prev_type = ORCM_DB_ITEM_INTEGER;
    bool change_value_binding = true;

    opal_value_t *kv;
    orcm_value_t *mv;

    SQLLEN null_len = SQL_NULL_DATA;
    SQLRETURN ret;
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    int i;

    bool local_tran_started = false;

    num_items = opal_list_get_size(input);
    OBJ_CONSTRUCT(&item_bm, opal_bitmap_t);
    opal_bitmap_init(&item_bm, (int)num_items);

    /* Get the main parameters from the list */
    num_params_found = orcm_util_find_items(params, NUM_PARAMS, input, param_items,
                                  &item_bm);

    /* Check the parameters */
    if (NULL == param_items[0]) {
        ERR_MSG_RDT("No hostname provided");
        OBJ_DESTRUCT(&item_bm);
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == param_items[1]) {
        ERR_MSG_RDT("No diagnostic type provided");
        OBJ_DESTRUCT(&item_bm);
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == param_items[2]) {
        ERR_MSG_RDT("No diagnostic subtype provided");
        OBJ_DESTRUCT(&item_bm);
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == param_items[3]) {
        ERR_MSG_RDT("No start time provided");
        OBJ_DESTRUCT(&item_bm);
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == param_items[4]) {
        ERR_MSG_RDT("No test result provided");
        OBJ_DESTRUCT(&item_bm);
        return ORCM_ERR_BAD_PARAM;
    }

    kv = param_items[0];
    if (OPAL_STRING == kv->type) {
        hostname = kv->data.string;
    } else {
        ERR_MSG_RDT("Invalid value type specified for hostname");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[1];
    if (OPAL_STRING == kv->type) {
        diag_type = kv->data.string;
    } else {
        ERR_MSG_RDT("Invalid value type specified for diagnostic "
                      "type");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[2];
    if (OPAL_STRING == kv->type) {
        diag_subtype = kv->data.string;
    } else {
        ERR_MSG_RDT("Invalid value type specified for diagnostic "
                      "subtype");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[3];
    switch (kv->type) {
    case OPAL_TIMEVAL:
    case OPAL_TIME:
        if (!tv_to_sql_timestamp(&start_time_sql, &kv->data.tv)) {
            ERR_MSG_RDT("Failed to convert start time stamp "
                          "value");
            rc = ORCM_ERR_BAD_PARAM;
            goto cleanup_and_exit;
        }
        break;
    case OPAL_STRING:
        /* Note: assuming "%F %T%z" format and ignoring sub second
        resolution when passed as a string */
        strptime(kv->data.string, "%F %T%z", &time_info);
        tm_to_sql_timestamp(&start_time_sql, &time_info);
        break;
    default:
        ERR_MSG_RDT("Invalid value type specified for start time");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[4];
    if (OPAL_STRING == kv->type) {
        test_result = kv->data.string;
    } else {
        ERR_MSG_RDT("Invalid value type specified for test result");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    if (NULL != (kv = param_items[5])) {
        switch (kv->type) {
        case OPAL_TIMEVAL:
        case OPAL_TIME:
            if (!tv_to_sql_timestamp(&end_time_sql, &kv->data.tv)) {
                ERR_MSG_RDT("Failed to convert end time stamp value");
                rc = ORCM_ERR_BAD_PARAM;
                goto cleanup_and_exit;
            }
            break;
        case OPAL_STRING:
            strptime(kv->data.string, "%F %T%z", &time_info);
            tm_to_sql_timestamp(&end_time_sql, &time_info);
            break;
        default:
            ERR_MSG_RDT("Invalid value type specified for end time");
            rc = ORCM_ERR_BAD_PARAM;
            goto cleanup_and_exit;
        }
    }

    if (NULL != (kv = param_items[6])) {
        if (OPAL_INT == kv->type) {
            component_index = &kv->data.integer;
        } else {
            ERR_MSG_RDT("Invalid value type specified for component "
                          "index");
            rc = ORCM_ERR_BAD_PARAM;
            goto cleanup_and_exit;
        }
    }

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_RDT("SQLAllocHandle returned: %d", ret);
        rc = ORCM_ERROR;
        goto cleanup_and_exit;
    }

    /* Record the diag. test result */
    /*
     * 1.- hostname
     * 2.- diagnostic type
     * 3.- diagnostic subtype
     * 4.- start time
     * 5.- end time
     * 6.- component index
     * 7.- test result
     */
    ret = SQLPrepare(stmt,
                     (SQLCHAR *)
                     "{call record_diag_test_result(?, ?, ?, ?, ?, ?, ?)}",
                     SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLPrepare returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind hostname parameter. */
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)hostname, strlen(hostname), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 1 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind diagnostic type parameter. */
    ret = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)diag_type, strlen(diag_type),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 2 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind diagnostic subtype parameter. */
    ret = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)diag_subtype, strlen(diag_subtype),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 3 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind start time parameter. */
    ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                           SQL_TYPE_TIMESTAMP, 0, 0,
                           (SQLPOINTER)&start_time_sql, sizeof(start_time_sql),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 4 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind end time parameter. */
    if (NULL != param_items[5]) {
        ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                               SQL_TYPE_TIMESTAMP, 0, 0,
                               (SQLPOINTER)&end_time_sql, sizeof(end_time_sql),
                               NULL);
    } else {
        ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                               SQL_TYPE_TIMESTAMP, 0, 0, NULL, 0, &null_len);
    }
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 5 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind component index parameter. */
    if (NULL != component_index) {
        ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_LONG,
                               SQL_INTEGER, 0, 0, (SQLPOINTER)component_index,
                               sizeof(component_index), NULL);
    } else {
        ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_LONG,
                               SQL_INTEGER, 0, 0, NULL, 0, &null_len);
    }
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 6 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind test result parameter. */
    ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)test_result, strlen(test_result),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 7 returned: %d", ret);
        goto cleanup_and_exit;
    }

    ret = SQLExecute(stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_SQL_RDT(SQL_HANDLE_STMT, stmt, "SQLExecute returned: %d",
                            ret);
        rc = ORCM_ERROR;
        goto cleanup_and_exit;
    }

    SQLCloseCursor(stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    stmt = SQL_NULL_HSTMT;

    local_tran_started = true;

    if (num_items <= num_params_found) {
        /* No test parameters provided, we're done! */
        if (mod->autocommit) {
            ret = SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_COMMIT);
            if (!(SQL_SUCCEEDED(ret))) {
                rc = ORCM_ERROR;
                ERR_MSG_FMT_SQL_RDT(SQL_HANDLE_DBC, mod->dbhandle,
                                    "SQLEndTran returned: %d", ret);
                goto cleanup_and_exit;
            }
        }

        opal_output_verbose(2, orcm_db_base_framework.framework_output,
                            "odbc_record_diag_test succeeded");
        goto cleanup_and_exit;
    }

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLAllocHandle returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Store the diag. test parameters */
    /*
     * 1.- hostname
     * 2.- diagnostic type
     * 3.- diagnostic subtype
     * 4.- start time
     * 5.- test parameter
     * 6.- data type
     * 7.- integer value
     * 8.- real value
     * 9.- string value
     * 10.- units
     */
    ret = SQLPrepare(stmt,
                     (SQLCHAR *)"{call record_diag_test_config"
                                "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)}",
                     SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLPrepare returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Bind hostname parameter. */
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)hostname, strlen(hostname), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 1 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind diagnostic type parameter. */
    ret = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)diag_type, strlen(diag_type),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 2 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind diagnostic subtype parameter. */
    ret = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)diag_subtype, strlen(diag_subtype),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 3 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind start time parameter. */
    ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                           SQL_TYPE_TIMESTAMP, 0, 0,
                           (SQLPOINTER)&start_time_sql, sizeof(start_time_sql),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 4 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind data type parameter. */
    ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                           0, 0, (SQLPOINTER)&item.opal_type,
                           sizeof(item.opal_type), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 6 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind integer value parameter (assuming the value is integer for now). */
    ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                           SQL_BIGINT, 0, 0, (SQLPOINTER)&item.value.value_int,
                           sizeof(item.value.value_int), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 7 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind real value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 8 returned: %d", ret);
        goto cleanup_and_exit;
    }
    /* Bind string value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_RDT("SQLBindParameter 9 returned: %d", ret);
        goto cleanup_and_exit;
    }

    i = 0;
    OPAL_LIST_FOREACH(mv, input, orcm_value_t) {
        /* Skip the items that have already been processed */
        if (opal_bitmap_is_set_bit(&item_bm, i)) {
            i++;
            continue;
        }

        if (NULL == mv->value.key || 0 == strlen(mv->value.key)) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_RDT("Key or test parameter name not provided for value");
            goto cleanup_and_exit;
        }

        ret = opal_value_to_orcm_db_item(&mv->value, &item);
        if (ORCM_SUCCESS != ret) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_RDT("Unsupported value type");
            goto cleanup_and_exit;
        }
        change_value_binding = prev_type != item.item_type;
        prev_type = item.item_type;

        /* Bind the test parameter. */
        ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR,
                               SQL_VARCHAR, 0, 0, (SQLPOINTER)mv->value.key,
                               strlen(mv->value.key), NULL);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_RDT("SQLBindParameter 5 returned: %d", ret);
            goto cleanup_and_exit;
        }

        if (NULL != mv->units) {
            /* Bind the units parameter. */
            ret = SQLBindParameter(stmt, 10, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)mv->units,
                                   strlen(mv->units), NULL);
        } else {
            /* No units provided, bind NULL. */
            ret = SQLBindParameter(stmt, 10, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0, NULL, 0, &null_len);
        }
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_RDT("SQLBindParameter 10 returned: %d", ret);
            goto cleanup_and_exit;
        }

        if (change_value_binding) {
            switch (item.item_type) {
            case ORCM_DB_ITEM_INTEGER:
                /* Value is integer, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0,
                                       (SQLPOINTER)&item.value.value_int,
                                       sizeof(item.value.value_int), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 9 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_REAL:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is real, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0,
                                       (SQLPOINTER)&item.value.value_real,
                                       sizeof(item.value.value_real), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the string value. */
                ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 9 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            case ORCM_DB_ITEM_STRING:
                /* Pass NULL for the integer value. */
                ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                                       SQL_BIGINT, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 7 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Pass NULL for the real value. */
                ret = SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 8 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                /* Value is string, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0,
                                       (SQLPOINTER)item.value.value_str,
                                       strlen(item.value.value_str), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    rc = ORCM_ERROR;
                    ERR_MSG_FMT_RDT("SQLBindParameter 9 returned: %d", ret);
                    goto cleanup_and_exit;
                }
                break;
            default:
                rc = ORCM_ERROR;
                ERR_MSG_RDT("An unexpected error has occurred while "
                            "processing the values");
                goto cleanup_and_exit;
            }
        } else if (ORCM_DB_ITEM_STRING == item.item_type) {
            /* No need to change the binding for all the values, just update
             * the string binding. */
            ret = SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)item.value.value_str,
                                   strlen(item.value.value_str), NULL);
            if (!(SQL_SUCCEEDED(ret))) {
                rc = ORCM_ERROR;
                ERR_MSG_FMT_RDT("SQLBindParameter 9 returned: %d", ret);
                goto cleanup_and_exit;
            }
        }

        ret = SQLExecute(stmt);
        if (!(SQL_SUCCEEDED(ret))) {
            ERR_MSG_FMT_SQL_RDT(SQL_HANDLE_STMT, stmt,
                                  "SQLExecute returned: %d", ret);
            rc = ORCM_ERROR;
            goto cleanup_and_exit;
        }

        SQLCloseCursor(stmt);

        i++;
    }

    if (mod->autocommit) {
        ret = SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_COMMIT);
        if (!(SQL_SUCCEEDED(ret))) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_SQL_RDT(SQL_HANDLE_DBC, mod->dbhandle,
                                "SQLEndTran returned: %d", ret);
            goto cleanup_and_exit;
        }
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "odbc_record_diag_test succeeded");

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_ROLLBACK);
    }

    if (SQL_NULL_HSTMT != stmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }

    OBJ_DESTRUCT(&item_bm);

    return rc;
}

#define ERR_MSG_FMT_SQL_COMMIT(handle_type, handle, msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to commit current transaction: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "ODBC error details:"); \
    odbc_error_info(handle_type, handle); \
    opal_output(0, "***********************************************");

static int odbc_commit(struct orcm_db_base_module_t *imod)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;

    SQLRETURN ret;

    ret = SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_COMMIT);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_SQL_COMMIT(SQL_HANDLE_DBC, mod->dbhandle,
                               "SQLEndTran returned: %d", ret);
        return ORCM_ERROR;
    }

    return ORCM_SUCCESS;
}

#define ERR_MSG_FMT_SQL_ROLLBACK(handle_type, handle, msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to cancel current transaction: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "ODBC error details:"); \
    odbc_error_info(handle_type, handle); \
    opal_output(0, "***********************************************");

static int odbc_rollback(struct orcm_db_base_module_t *imod)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;

    SQLRETURN ret;

    ret = SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_ROLLBACK);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_SQL_ROLLBACK(SQL_HANDLE_DBC, mod->dbhandle,
                                 "SQLEndTran returned: %d", ret);
        return ORCM_ERROR;
    }

    return ORCM_SUCCESS;
}

#define ERR_MSG_FMT_REMOVE(msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to remove data: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "***********************************************");

#define ERR_MSG_FMT_SQL_REMOVE(handle_type, handle, msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to remove data: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "ODBC error details:"); \
    odbc_error_info(handle_type, handle); \
    opal_output(0, "***********************************************");

static int odbc_remove(struct orcm_db_base_module_t *imod,
                       const char *primary_key,
                       const char *key)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    int rc = ORCM_SUCCESS;

    SQLRETURN ret;
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    char query[1024];

    bool local_tran_started = false;

    snprintf(query, sizeof(query), "delete from %s where %s",
             mod->table, primary_key);

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_REMOVE("SQLAllocHandle returned: %d", ret);
        return ORCM_ERROR;
    }

    ret = SQLExecDirect(stmt, (SQLCHAR *)query, SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_REMOVE("SQLExecDirect returned: %d", ret);
        goto cleanup_and_exit;
    }

    local_tran_started = true;

    ret = SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_COMMIT);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_SQL_REMOVE(SQL_HANDLE_DBC, mod->dbhandle,
                               "SQLEndTran returned: %d", ret);
        goto cleanup_and_exit;
    }

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        SQLEndTran(SQL_HANDLE_DBC, mod->dbhandle, SQL_ROLLBACK);
    }

    if (SQL_NULL_HSTMT != stmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }

    return rc;
}

static void odbc_error_info(SQLSMALLINT handle_type, SQLHANDLE handle)
{
    int i = 1;
    int ret;
    SQLCHAR sql_state[6];
    SQLINTEGER native_error;
    SQLCHAR msg[SQL_MAX_MESSAGE_LENGTH];
    SQLSMALLINT msg_len;

    ret = SQLGetDiagRec(handle_type, handle, i, sql_state, &native_error, msg,
                        sizeof(msg), &msg_len);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_output(0, "Unable to retrieve ODBC error information");
        return;
    }

    do {
        opal_output(0, "Status record: %d", i);
        opal_output(0, "SQL state: %s", sql_state);
        opal_output(0, "Native error: %d", native_error);
        opal_output(0, "Message: %s", msg);

        i++;
        ret = SQLGetDiagRec(handle_type, handle, i, sql_state, &native_error,
                            msg, sizeof(msg), &msg_len);
    } while (SQL_SUCCEEDED(ret));
}

static void tm_to_sql_timestamp(SQL_TIMESTAMP_STRUCT *sql_timestamp,
                             const struct tm *time_info)
{
    sql_timestamp->year = time_info->tm_year + 1900;
    sql_timestamp->month = time_info->tm_mon + 1;
    sql_timestamp->day = time_info->tm_mday;
    sql_timestamp->hour = time_info->tm_hour;
    sql_timestamp->minute = time_info->tm_min;
    sql_timestamp->second = time_info->tm_sec;
    sql_timestamp->fraction = 0;
}

static bool tv_to_sql_timestamp(SQL_TIMESTAMP_STRUCT *sql_timestamp,
                             const struct timeval *time)
{
    struct tm *time_info = NULL;
    time_info = localtime(&time->tv_sec);
    if (NULL != time_info) {
        tm_to_sql_timestamp(sql_timestamp, time_info);
        sql_timestamp->fraction = time->tv_usec * 1000;
        return true;
    } else {
        return false;
    }
}

#define ERR_MSG_FMT_FETCH(msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:odbc: Unable to fetch data: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "***********************************************");

static int odbc_fetch(struct orcm_db_base_module_t *imod,
                      const char *view,
                      opal_list_t *filters,
                      opal_list_t *kvs)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    int rc = ORCM_SUCCESS;
    SQLRETURN ret = SQL_SUCCESS;
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    char *query = NULL;
    int handle = -1;
    opal_value_t *result = NULL;


    if(NULL == view || 0 == strlen(view)) {
        ERR_MSG_FMT_FETCH("database view passed was empty of %s!", "NULL");
        rc = ORCM_ERR_NOT_IMPLEMENTED;
        goto cleanup_and_exit;
    }

    if(NULL == kvs) {
        ERR_MSG_FMT_FETCH("Argument 'kvs' passed was NULL but was expected to be valid opal_list_t pointer: %d", 0);
        rc = ORCM_ERROR;
        goto cleanup_and_exit;
    }
    query = build_query_from_view_name_and_filters(view, filters);
    if(NULL == query) {
        ERR_MSG_FMT_FETCH("build_query_from_view_name_and_filters returned: %s", "NULL");
        rc = ORCM_ERROR;
        goto cleanup_and_exit;
    }
    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_FETCH("SQLAllocHandle returned %d for SQL_HANDLE_STMT handle creation (DB Handle=0x%016lx)!", ret, (size_t)mod->dbhandle);
        rc = ORCM_ERROR;
        goto cleanup_and_exit;
    }

    ret = SQLExecDirect(stmt, (SQLCHAR *)query, SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_FETCH("SQLExecDirect returned: %d", ret);
        goto cleanup_and_exit;
    }

    /* Add new results set handle */
    handle = opal_pointer_array_add(mod->results_sets, (void*)stmt);
    if(-1 == handle) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_FETCH("opal_pointer_array_add returned: %d", -1);
        goto cleanup_and_exit;
    }

    /* Create returned handle object */
    result = OBJ_NEW(opal_value_t);
    result->type = OPAL_INT;
    result->data.integer = handle;
    opal_list_append(kvs, (void*)result); /* takes ownership */

cleanup_and_exit:
    free(query);
    return rc;
}

static int odbc_get_num_rows(struct orcm_db_base_module_t *imod, int rshandle, int *num_rows)
{
    int rc = ORCM_ERROR;
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    SQLHSTMT stmt = (SQLHSTMT)opal_pointer_array_get_item(mod->results_sets, rshandle);
    SQLLEN sql_rows = 0;

    if(SQL_NULL_HSTMT != stmt) {
        SQLRETURN ret = SQLRowCount(stmt, &sql_rows);

        if(SQL_SUCCEEDED(ret)) {
            *num_rows = (int)sql_rows;
            rc = ORCM_SUCCESS;
        }
    }
    return rc;
}
/**************************************************************************************************
 * If fetch features are added or column names change or opal types change for a column name
 * change here!!!
 **************************************************************************************************/
 #define COLUMN_NAME_WIDTH 48 /* size is arbitrary but reasonable */
 #define NO_COLUMN         (-1)
extern const char *opal_type_column_name;
extern const char *value_column_names[];

static int get_number_of_columns(SQLHSTMT results)
{
    SQLSMALLINT column_count = 0;
    SQLRETURN ret = SQLNumResultCols(results, &column_count);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_FETCH("SQLNumResultCols returned: %d", ret);
        return -1;
    }
    return (int)column_count;
}

static int get_column_number_by_name(SQLHSTMT results, const char* name)
{
    SQLULEN col_size = 0;
    SQLSMALLINT dec_dig = 0;
    SQLSMALLINT nullable = 0;
    SQLSMALLINT type = 0;
    SQLSMALLINT name_len = 0;
    char column_name[COLUMN_NAME_WIDTH];
    int cols = get_number_of_columns(results);
    if(-1 != cols) {
        for(int i = 1; i <= cols; ++i) {
            SQLRETURN ret = SQLDescribeCol(results, (SQLUSMALLINT)i, (SQLCHAR*)column_name, (SQLSMALLINT)COLUMN_NAME_WIDTH, &name_len, &type, &col_size, &dec_dig, &nullable);
            if(!(SQL_SUCCEEDED(ret))) {
                ERR_MSG_FMT_FETCH("SQLDescribeCol returned #1: %d", ret);
                return NO_COLUMN;
            }
            column_name[name_len] = '\0';
            if(0 == strcmp(column_name, name)) {
                return i;
            }
        }
    }
    return NO_COLUMN;
}

static int get_value_column(SQLHSTMT results, int index)
{
    SQLULEN col_size = 0;
    SQLSMALLINT dec_dig = 0;
    SQLSMALLINT nullable = 0;
    SQLSMALLINT type = 0;
    SQLSMALLINT name_len = 0;
    char column_name[COLUMN_NAME_WIDTH];
    SQLSMALLINT column_count = 0;
    SQLRETURN ret = SQLNumResultCols(results, &column_count);
    if (!(SQL_SUCCEEDED(ret))) {
        ERR_MSG_FMT_FETCH("SQLNumResultCols returned: %d", ret);
        return NO_COLUMN;
    }
    for(int i = 1; i <= (int)column_count; ++i) {
        ret = SQLDescribeCol(results, (SQLUSMALLINT)i, (SQLCHAR*)column_name, (SQLSMALLINT)COLUMN_NAME_WIDTH, &name_len, &type, &col_size, &dec_dig, &nullable);
        if(!(SQL_SUCCEEDED(ret))) {
            ERR_MSG_FMT_FETCH("SQLDescribeCol returned #2: %d", ret);
            return NO_COLUMN;
        }
        column_name[name_len] = '\0';
        if(0 == strcmp(column_name, value_column_names[index])) {
            return i;
        }
    }
    return NO_COLUMN;
}

static bool is_column_name_a_value_column(const char* column_name)
{
    int i = 0;
    bool rv = false;

    while(NULL != value_column_names[i]) {
        if(0 == strcmp(column_name, value_column_names[i])) {
            rv = true;
            break;
        }
        ++i;
    }
    return rv;
}

static opal_data_type_t get_opal_type_from_sql_type(SQLSMALLINT sql_type)
{
    switch(sql_type) { /* Based on sql_types.h and dss_types.h */
    case SQL_BIGINT:
        return OPAL_INT64;
    case SQL_INTEGER:
        return OPAL_INT32;
    case SQL_SMALLINT:
        return OPAL_INT16;
    case SQL_FLOAT:
        return OPAL_DOUBLE;
    case SQL_REAL:
        return OPAL_FLOAT;
    case SQL_DECIMAL:
    case SQL_NUMERIC:
    case SQL_DOUBLE:
        return OPAL_DOUBLE;
    case SQL_DATETIME:
        return OPAL_TIMEVAL;
    case SQL_CHAR:
    case SQL_VARCHAR:
        return OPAL_STRING;
    default:
        return OPAL_UNDEF;
    }
}
/**************************************************************************************************/

#define VALUE_STR_COLUMN_NUM    0
#define VALUE_INT_COLUMN_NUM    1
#define VALUE_REAL_COLUMN_NUM   2

static opal_value_t *get_value_object(SQLHSTMT results, int unused, opal_data_type_t type)
{
    SQLLEN result_len = 0;
    SQLRETURN ret = SQL_SUCCESS;
    int column = NO_COLUMN;
    if(OPAL_STRING == type && NO_COLUMN == get_value_column(results, VALUE_STR_COLUMN_NUM)) {
        return NULL;
    }
    if((OPAL_FLOAT == type || OPAL_DOUBLE == type) && NO_COLUMN == get_value_column(results, VALUE_REAL_COLUMN_NUM)) {
        return NULL;
    }
    if(true == is_supported_opal_int_type(type) && NO_COLUMN == get_value_column(results, VALUE_INT_COLUMN_NUM)) {
        return NULL;
    }
    opal_value_t *rv = OBJ_NEW(opal_value_t);
    rv->type = type;
    rv->key = strdup("value");
    switch(type) { /* all PQgetvalue buffers are owned by PQ */
        case OPAL_BYTE:
        case OPAL_SIZE:
        case OPAL_PID:
        case OPAL_INT:
        case OPAL_INT8:
        case OPAL_INT16:
        case OPAL_INT32:
        case OPAL_INT64:
        case OPAL_UINT:
        case OPAL_UINT8:
        case OPAL_UINT16:
        case OPAL_UINT32:
        case OPAL_UINT64:
            column = get_value_column(results, VALUE_INT_COLUMN_NUM);
            ret = SQLGetData(results, column, SQL_BIGINT, &rv->data.uint64, sizeof(uint64_t), &result_len);
            if(!(SQL_SUCCEEDED(ret))) {
                ERR_MSG_FMT_FETCH("SQLGetData returned #4: %d", ret);
                OBJ_RELEASE(rv);
                return NULL;
            }
            break;
        case OPAL_BOOL:
            {
                int64_t local;
                column = get_value_column(results, VALUE_INT_COLUMN_NUM);
                ret = SQLGetData(results, column, SQL_BIGINT, &local, sizeof(int64_t), &result_len);
                if(!(SQL_SUCCEEDED(ret))) {
                    ERR_MSG_FMT_FETCH("SQLGetData returned #5: %d", ret);
                    OBJ_RELEASE(rv);
                    return NULL;
                }
                rv->data.flag = (0 == local)?false:true;
            }
            break;
        case OPAL_STRING:
            {
                SQLULEN col_size = 0;
                SQLSMALLINT dec_dig = 0;
                SQLSMALLINT nullable = 0;
                SQLSMALLINT type = 0;
                SQLSMALLINT name_len = 0;
                char column_name[COLUMN_NAME_WIDTH];
                size_t size = 0;
                column = get_value_column(results, VALUE_STR_COLUMN_NUM);
                ret = SQLDescribeCol(results, column, (SQLCHAR*)column_name, (SQLSMALLINT)COLUMN_NAME_WIDTH, &name_len, &type, &col_size, &dec_dig, &nullable);
                if(!(SQL_SUCCEEDED(ret))) {
                    ERR_MSG_FMT_FETCH("SQLDescribeCol returned #3: %d", ret);
                    OBJ_RELEASE(rv);
                    return NULL;
                }
                size = (size_t)col_size;
                column_name[name_len] = '\0';
                rv->data.string = (char*)malloc(size);
                memset(rv->data.string, 0, (int)size);
                ret = SQLGetData(results, column, SQL_C_CHAR, rv->data.string, (SQLLEN)(size), &result_len);
                if(!(SQL_SUCCEEDED(ret))) {
                    ERR_MSG_FMT_FETCH("SQLGetData returned #6: %d", ret);
                    OBJ_RELEASE(rv);
                    return NULL;
                }
            }
            break;
        case OPAL_FLOAT:
            {
                double dbl;
                column = get_value_column(results, VALUE_REAL_COLUMN_NUM);
                ret = SQLGetData(results, column, SQL_DOUBLE, &dbl, sizeof(double), &result_len);
                if(!(SQL_SUCCEEDED(ret))) {
                    ERR_MSG_FMT_FETCH("SQLGetData returned #7: %d", ret);
                    OBJ_RELEASE(rv);
                    return NULL;
                }
                rv->data.fval = (float)dbl;
            }
            break;
        case OPAL_DOUBLE:
            column = get_value_column(results, VALUE_REAL_COLUMN_NUM);
            ret = SQLGetData(results, column, SQL_DOUBLE, &rv->data.dval, sizeof(double), &result_len);
            if(!(SQL_SUCCEEDED(ret))) {
                ERR_MSG_FMT_FETCH("SQLGetData returned #8: %d", ret);
                OBJ_RELEASE(rv);
                return NULL;
            }
            break;
        default: /* No supported basic data type so ignore it */
            ERR_MSG_FMT_FETCH("An unsupported orcm value type was encountered: %d", type);
            OBJ_RELEASE(rv);
            return NULL;
    }
    return rv;
}

static int odbc_get_next_row(struct orcm_db_base_module_t *imod,
                             int rshandle,
                             opal_list_t *row)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    SQLHSTMT stmt = (SQLHSTMT)opal_pointer_array_get_item(mod->results_sets, rshandle);
    int rc = ORCM_SUCCESS;

    SQLRETURN ret = SQL_SUCCESS;
    int cols = -1;
    int data_type_col = -1;
    opal_data_type_t data_type = OPAL_UNDEF;
    bool inserted_value = false;
    opal_value_t* value_object = NULL;
    SQLLEN result_len = 0;

    if(SQL_NULL_HSTMT == stmt) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_FETCH("SQLAllocHandle returned %d for SQL_HANDLE_STMT handle creation (DB Handle=0x%016lx)!", ret, (size_t)mod->dbhandle);
        return ORCM_ERROR;
    }

    /* get next row */
    ret = SQLFetch(stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_FETCH("SQLFetch returned: %d", ret);
        goto next_cleanup;
    }

    /* get row ORCM data value type */
    data_type_col = get_column_number_by_name(stmt, opal_type_column_name);
    if(NO_COLUMN != data_type_col) {
        ret = SQLGetData(stmt, data_type_col, SQL_C_LONG, &data_type, sizeof(opal_data_type_t), &result_len);
        if(!(SQL_SUCCEEDED(ret))) {
            ERR_MSG_FMT_FETCH("SQLGetData returned: %d", ret);
            rc = ORCM_ERROR;
            goto next_cleanup;
        }

        /* retrieve 1 correctly typed object for one of 'value_str', 'value_int', 'value_real' */
        value_object = get_value_object(stmt, 0, data_type);
    } else { /* If the current schema is missing the data type column 'data_type_id' use this logic */
        data_type_col = get_value_column(stmt, VALUE_STR_COLUMN_NUM);
        data_type = OPAL_STRING;
        if(NO_COLUMN == data_type_col) {
            data_type_col = get_value_column(stmt, VALUE_INT_COLUMN_NUM);
            data_type = OPAL_INT64;
            if(NO_COLUMN == data_type_col) {
                data_type_col = get_value_column(stmt, VALUE_REAL_COLUMN_NUM);
                data_type = OPAL_DOUBLE;
            }
        }
        /* retrieve 1 grossly typed object for one of 'value_str', 'value_int', 'value_real' */
        value_object = get_value_object(stmt, 0, data_type);
    }

    /* Get row general column data */
    cols = get_number_of_columns(stmt);
    for(int i = 1; i <= cols; ++i) {
        SQLULEN len = 0;
        opal_value_t* kv = NULL;
        opal_data_type_t type = OPAL_UNDEF;
        char name_ref[COLUMN_NAME_WIDTH];
        SQLSMALLINT sql_data_type = -1;
        SQLSMALLINT dec_dig = 0;
        SQLSMALLINT nullable = 0;
        SQLSMALLINT name_len = 0;

        if(i == data_type_col) { /* skip 'data_type_id' column */
            continue;
        }

        ret = SQLDescribeCol(stmt, (SQLUSMALLINT)i, (SQLCHAR*)name_ref, (SQLSMALLINT)COLUMN_NAME_WIDTH, &name_len, &sql_data_type, &len, &dec_dig, &nullable);
        if(!(SQL_SUCCEEDED(ret))) {
            ERR_MSG_FMT_FETCH("SQLDescribeCol returned #4: %d", ret);
            rc = ORCM_ERROR;
            goto next_cleanup;
        }
        name_ref[name_len] = '\0';
        if(-1 == sql_data_type) {
            sql_data_type = SQL_VARCHAR; /* In practice, the SQLDescribeCol is not returning a type for the 'string' values when the underlying DB is postgres. */
        }
        if(true == is_column_name_a_value_column(name_ref)) {
            if(false == inserted_value && NULL != value_object) {
                opal_list_append(row, &value_object->super);
                value_object = NULL;
                inserted_value = true;
            }
            continue; /* skip all 'value_*' named columns as they are already handled */
        }

        type = get_opal_type_from_sql_type(sql_data_type);
        if(OPAL_UNDEF == type) {
            ERR_MSG_FMT_FETCH("An unrecognized column name '%s' was encountered and skipped during a call to 'get_next_row'", name_ref);
            continue; /* skip unknown fields */
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup(name_ref);
        kv->type = type;
        switch(type) {
            case OPAL_STRING:
                {
                    char* buffer = malloc(len + 1);
                    ret = SQLGetData(stmt, i, SQL_C_CHAR, buffer, len + 1, &result_len);
                    if(!(SQL_SUCCEEDED(ret))) {
                        ERR_MSG_FMT_FETCH("SQLGetData returned #1: %d", ret);
                        rc = ORCM_ERROR;
                        free(buffer);
                        OBJ_RELEASE(kv);
                        goto next_cleanup;
                    }
                    kv->data.string = strdup(buffer);
                    free(buffer);
                }
                break;
            case OPAL_INT:
                ret = SQLGetData(stmt, i, SQL_C_LONG, &kv->data.integer, sizeof(kv->data.integer), &result_len);
                if(!(SQL_SUCCEEDED(ret))) {
                    ERR_MSG_FMT_FETCH("SQLGetData returned #2: %d", ret);
                    rc = ORCM_ERROR;
                    OBJ_RELEASE(kv);
                    goto next_cleanup;
                }
                break;
            case OPAL_TIMEVAL:
                {
                    SQL_TIMESTAMP_STRUCT time_stamp;
                    struct tm temp_tm;
                    ret = SQLGetData(stmt, i, SQL_C_TYPE_TIMESTAMP, &time_stamp, sizeof(time_stamp), &result_len);
                    if(!(SQL_SUCCEEDED(ret))) {
                        ERR_MSG_FMT_FETCH("SQLGetData returned #3: %d", ret);
                        rc = ORCM_ERROR;
                        OBJ_RELEASE(kv);
                        goto next_cleanup;
                    }
                    /* The year in tm represents the number of years since 1900 */
                    temp_tm.tm_year = time_stamp.year - 1900;
                    /* The month in tm is zero-based */
                    memset(&temp_tm, 0, sizeof(temp_tm));
                    temp_tm.tm_mon = time_stamp.month - 1;
                    temp_tm.tm_mday = time_stamp.day;
                    temp_tm.tm_hour = time_stamp.hour;
                    temp_tm.tm_min = time_stamp.minute;
                    temp_tm.tm_sec = time_stamp.second;

                    kv->data.tv.tv_sec = mktime(&temp_tm);
                    kv->data.tv.tv_usec = 0;
                }
                break;
        }
        opal_list_append(row, (opal_list_item_t*)kv);
    }
next_cleanup:
    return rc;
}

static int odbc_close_result_set(struct orcm_db_base_module_t *imod,
                                 int rshandle)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    SQLHSTMT stmt = (SQLHSTMT)opal_pointer_array_get_item(mod->results_sets, rshandle);

    if(SQL_NULL_HSTMT == stmt) {
        ERR_MSG_FMT_FETCH("Bad results handle: %d", rshandle);
        return ORCM_ERROR;
    }
    opal_pointer_array_set_item(mod->results_sets, rshandle, NULL);
    if (SQL_NULL_HSTMT != stmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }

    return ORCM_SUCCESS;
}
