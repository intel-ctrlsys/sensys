/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
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

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

#include "opal_stdint.h"
#include "opal/util/argv.h"
#include "opal/util/error.h"

#include "orcm/mca/db/base/base.h"
#include "db_odbc.h"

static int odbc_init(struct orcm_db_base_module_t *imod);
static void odbc_finalize(struct orcm_db_base_module_t *imod);
static int odbc_store(struct orcm_db_base_module_t *imod,
                      const char *primary_key,
                      opal_list_t *kvs);
static int odbc_store_sample(struct orcm_db_base_module_t *imod,
                             const char *primary_key,
                             opal_list_t *kvs);
static int odbc_fetch(struct orcm_db_base_module_t *imod,
                      const char *primary_key,
                      const char *key,
                      opal_list_t *kvs);
static int odbc_remove(struct orcm_db_base_module_t *imod,
                      const char *primary_key,
                      const char *key);

mca_db_odbc_module_t mca_db_odbc_module = {
    {
        odbc_init,
        odbc_finalize,
        odbc_store_sample,
        NULL,
        odbc_fetch,
        odbc_remove
    },
};

#define INIT_ERROR_MSG_FMT(mod, msg, ...) \
    opal_output(0, "***********************************************\n"); \
    opal_output(0, "db:odbc: Connection failed: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "\n\tDSN: %s", mod->odbcdsn); \
    opal_output(0, "\n\tUser: %s", mod->user); \
    opal_output(0, "\n***********************************************");

static int odbc_init(struct orcm_db_base_module_t *imod)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    char **login = NULL;

    SQLRETURN ret;

    /* break the user info into its login parts */
    login = opal_argv_split(mod->user, ':');
    if (2 != opal_argv_count(login)) {
        opal_output(0, "db:odbc: User info is invalid: %s",
                    mod->user);
        opal_argv_free(login);
        return ORCM_ERR_BAD_PARAM;
    }

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &mod->envhandle);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_argv_free(login);
        mod->envhandle = NULL;
        INIT_ERROR_MSG_FMT(mod, "SQLAllocHandle returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }

    ret = SQLSetEnvAttr(mod->envhandle, SQL_ATTR_ODBC_VERSION,
                        (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_argv_free(login);
        SQLFreeHandle(SQL_HANDLE_ENV, mod->envhandle);
        mod->envhandle = NULL;
        INIT_ERROR_MSG_FMT(mod, "SQLSetEnvAttr returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }

    ret = SQLAllocHandle(SQL_HANDLE_DBC, mod->envhandle, &mod->dbhandle);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_argv_free(login);
        mod->dbhandle = NULL;
        SQLFreeHandle(SQL_HANDLE_ENV, mod->envhandle);
        mod->envhandle = NULL;
        INIT_ERROR_MSG_FMT(mod, "SQLAllocHandle returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }

    ret = SQLSetConnectAttr(mod->dbhandle, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_argv_free(login);
        SQLFreeHandle(SQL_HANDLE_DBC, mod->dbhandle);
        mod->dbhandle = NULL;
        SQLFreeHandle(SQL_HANDLE_ENV, mod->envhandle);
        mod->envhandle = NULL;
        INIT_ERROR_MSG_FMT(mod, "SQLSetConnectAttr returned: %d", ret);
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
        INIT_ERROR_MSG_FMT(mod, "SQLConnect returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }

    opal_argv_free(login);

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
}

#define STORE_ERR_MSG(msg) \
    opal_output(0, "***********************************************\n"); \
    opal_output(0, "ODBC component store command failed: "); \
    opal_output(0, msg); \
    opal_output(0, "\nUnable to log data"); \
    opal_output(0, "\n***********************************************");

#define STORE_ERR_MSG_FMT(msg, ...) \
    opal_output(0, "***********************************************\n"); \
    opal_output(0, "ODBC component store command failed: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "\nUnable to log data"); \
    opal_output(0, "\n***********************************************");

static int odbc_store(struct orcm_db_base_module_t *imod,
                      const char *primary_key,
                      opal_list_t *kvs)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    char *query, *vstr;
    char **cmdargs=NULL;
    time_t nowtime;
    struct tm nowtm;
    char tbuf[1024];
    opal_value_t *kv;

    SQLRETURN ret;
    SQLHSTMT stmt;

    /* cycle through the provided values and construct
     * an insert command for them - note that the values
     * MUST be in column-order for the database!
     */
    OPAL_LIST_FOREACH(kv, kvs, opal_value_t) {
        switch (kv->type) {
        case OPAL_STRING:
            snprintf(tbuf, sizeof(tbuf), "%s", kv->data.string);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_SIZE:
            snprintf(tbuf, sizeof(tbuf), "%" PRIsize_t "", kv->data.size);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_INT:
            snprintf(tbuf, sizeof(tbuf), "%d", kv->data.integer);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_INT8:
            snprintf(tbuf, sizeof(tbuf), "%" PRIi8 "", kv->data.int8);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_INT16:
            snprintf(tbuf, sizeof(tbuf), "%" PRIi16 "", kv->data.int16);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_INT32:
            snprintf(tbuf, sizeof(tbuf), "%" PRIi32 "", kv->data.int32);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_INT64:
            snprintf(tbuf, sizeof(tbuf), "%" PRIi64 "", kv->data.int64);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_UINT:
            snprintf(tbuf, sizeof(tbuf), "%u", kv->data.uint);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_UINT8:
            snprintf(tbuf, sizeof(tbuf), "%" PRIu8 "", kv->data.uint8);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_UINT16:
            snprintf(tbuf, sizeof(tbuf), "%" PRIu16 "", kv->data.uint16);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_UINT32:
            snprintf(tbuf, sizeof(tbuf), "%" PRIu32 "", kv->data.uint32);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_UINT64:
            snprintf(tbuf, sizeof(tbuf), "%" PRIu64 "", kv->data.uint64);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_PID:
            snprintf(tbuf, sizeof(tbuf), "%lu", (unsigned long)kv->data.pid);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_FLOAT:
            snprintf(tbuf, sizeof(tbuf), "%f", kv->data.fval);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        case OPAL_TIMEVAL:
            /* we only care about seconds */
            nowtime = kv->data.tv.tv_sec;
            (void)localtime_r(&nowtime, &nowtm);
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &nowtm);
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        default:
            snprintf(tbuf, sizeof(tbuf), "Unsupported type: %s",
                     opal_dss.lookup_data_type(kv->type));
            opal_argv_append_nosize(&cmdargs, tbuf);
            break;
        }
    }

    /* assemble the value string */
    vstr = opal_argv_join(cmdargs, ',');
    opal_argv_free(cmdargs);

    /* create the query */
    asprintf(&query, "insert into %s values (%s)", mod->table, vstr);
    free(vstr);

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "Executing query %s", query);

    /* execute it */
    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        free(query);
        STORE_ERR_MSG_FMT("SQLAllocHandle returned: %d", ret);
        return ORCM_ERROR;
    }

    ret = SQLExecDirect(stmt, (SQLCHAR *)query, SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        free(query);
        STORE_ERR_MSG_FMT("SQLExecDirect returned: %d", ret);
        return ORCM_ERROR;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    free(query);

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "Query succeeded");

    return ORCM_SUCCESS;
}

static int odbc_store_sample(struct orcm_db_base_module_t *imod,
                             const char *data_group,
                             opal_list_t *kvs)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;
    opal_value_t *kv;
    opal_list_item_t *item;
    char *sampletime_str;
    struct tm time_info;

    SQL_TIMESTAMP_STRUCT sampletime;
    char *hostname;
    char **data_item_argv;
    int argv_count;
    int numeric = 1;
    int change_value_binding = 1;
    double value_num;
    char *value_str;
    SQLLEN null_len = SQL_NULL_DATA;

    SQLRETURN ret;
    SQLHSTMT stmt;

    if (NULL == data_group) {
        STORE_ERR_MSG("No data group specified");
        return ORCM_ERROR;
    }

    if (NULL == kvs) {
        STORE_ERR_MSG("No value list specified");
        return ORCM_ERROR;
    }

    item = opal_list_get_first(kvs);
    kv = (opal_value_t *)item;
    if (item == opal_list_get_end(kvs) || kv->type != OPAL_STRING) {
        STORE_ERR_MSG("No time stamp provided");
        return ORCM_ERROR;
    }
    sampletime_str = kv->data.string;

    item = opal_list_get_next(item);
    kv = (opal_value_t *)item;
    if (item == opal_list_get_end(kvs) || kv->type != OPAL_STRING) {
        STORE_ERR_MSG("No hostname provided");
        return ORCM_ERROR;
    }
    hostname = kv->data.string;

    strptime(sampletime_str, "%F %T%z", &time_info);
    sampletime.year = time_info.tm_year + 1900;
    sampletime.month = time_info.tm_mon + 1;
    sampletime.day = time_info.tm_mday;
    sampletime.hour = time_info.tm_hour;
    sampletime.minute = time_info.tm_min;
    sampletime.second = time_info.tm_sec;
    sampletime.fraction = 0;

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        STORE_ERR_MSG_FMT("SQLAllocHandle returned: %d", ret);
        return ORCM_ERROR;
    }

    ret = SQLPrepare(stmt,
                     (SQLCHAR *)"{call add_data_sample(?, ?, ?, ?, ?, ?, ?)}",
                     SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        STORE_ERR_MSG_FMT("SQLPrepare returned: %d", ret);
        return ORCM_ERROR;
    }

    /* Bind hostname parameter. */
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, hostname, strlen(hostname), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        STORE_ERR_MSG_FMT("SQLBindParameter 1 returned: %d", ret);
        return ORCM_ERROR;
    }
    /* Bind data group parameter. */
    ret = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, (SQLPOINTER)data_group, strlen(data_group),
                           NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        STORE_ERR_MSG_FMT("SQLBindParameter 2 returned: %d", ret);
        return ORCM_ERROR;
    }
    /* Bind time stamp parameter. */
    ret = SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                           SQL_TYPE_TIMESTAMP, 0, 0, (SQLPOINTER)&sampletime,
                           sizeof(sampletime), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        STORE_ERR_MSG_FMT("SQLBindParameter 4 returned: %d", ret);
        return ORCM_ERROR;
    }
    /* Bind numeric value parameter (assuming the value is numeric for now). */
    ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE,
                           0, 0, (SQLPOINTER)&value_num,
                           sizeof(value_num), NULL);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        STORE_ERR_MSG_FMT("SQLBindParameter 5 returned: %d", ret);
        return ORCM_ERROR;
    }
    /* Bind string value parameter (assuming NULL for now). */
    ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, NULL, 0, &null_len);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        STORE_ERR_MSG_FMT("SQLBindParameter 6 returned: %d", ret);
        return ORCM_ERROR;
    }

    for (item = opal_list_get_next(item); item != opal_list_get_end(kvs);
         item = opal_list_get_next(item)) {
        kv = (opal_value_t *)item;
        switch (kv->type) {
        case OPAL_FLOAT:
            value_num = kv->data.fval;
            change_value_binding = !numeric ? 1 : 0;
            numeric = 1;
            break;
        case OPAL_DOUBLE:
            value_num = kv->data.dval;
            change_value_binding = !numeric ? 1 : 0;
            numeric = 1;
            break;
        case OPAL_INT:
            value_num = kv->data.integer;
            change_value_binding = !numeric ? 1 : 0;
            numeric = 1;
            break;
        case OPAL_INT8:
            value_num = kv->data.int8;
            change_value_binding = !numeric ? 1 : 0;
            numeric = 1;
            break;
        case OPAL_INT16:
            value_num = kv->data.int16;
            change_value_binding = !numeric ? 1 : 0;
            numeric = 1;
            break;
        case OPAL_INT32:
            value_num = kv->data.int32;
            change_value_binding = !numeric ? 1 : 0;
            numeric = 1;
            break;
        case OPAL_UINT:
            value_num = kv->data.uint;
            change_value_binding = !numeric ? 1 : 0;
            numeric = 1;
            break;
        case OPAL_UINT8:
            value_num = kv->data.uint8;
            change_value_binding = !numeric ? 1 : 0;
            numeric = 1;
            break;
        case OPAL_UINT16:
            value_num = kv->data.uint16;
            change_value_binding = !numeric ? 1 : 0;
            numeric = 1;
            break;
        case OPAL_UINT32:
            value_num = kv->data.uint32;
            change_value_binding = !numeric ? 1 : 0;
            numeric = 1;
            break;
        case OPAL_STRING:
            value_str = kv->data.string;
            change_value_binding = 1;
            numeric = 0;
            break;
        default:
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            STORE_ERR_MSG("Incorrect sample value type");
            return ORCM_ERROR;
        }

        /* kv->key will contain: <data item>:<units> */
        data_item_argv = opal_argv_split(kv->key, ':');
        argv_count = opal_argv_count(data_item_argv);
        if (argv_count == 0) {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            opal_argv_free(data_item_argv);
            STORE_ERR_MSG("No data item specified");
            return ORCM_ERROR;
        }
        /* Bind the data item parameter. */
        ret = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR,
                               SQL_VARCHAR, 0, 0, (SQLPOINTER)data_item_argv[0],
                               strlen(data_item_argv[0]), NULL);
        if (!(SQL_SUCCEEDED(ret))) {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            opal_argv_free(data_item_argv);
            STORE_ERR_MSG_FMT("SQLBindParameter 3 returned: %d", ret);
            return ORCM_ERROR;
        }

        if (argv_count > 1) {
            /* Bind the units parameter. */
            ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0,
                                   (SQLPOINTER)data_item_argv[1],
                                   strlen(data_item_argv[1]), NULL);
            if (!(SQL_SUCCEEDED(ret))) {
                SQLFreeHandle(SQL_HANDLE_STMT, stmt);
                opal_argv_free(data_item_argv);
                STORE_ERR_MSG_FMT("SQLBindParameter 7 returned: %d", ret);
                return ORCM_ERROR;
            }
        } else {
            /* No units provided, bind NULL. */
            ret = SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_CHAR,
                                   SQL_VARCHAR, 0, 0, NULL, 0, &null_len);
            if (!(SQL_SUCCEEDED(ret))) {
                SQLFreeHandle(SQL_HANDLE_STMT, stmt);
                opal_argv_free(data_item_argv);
                STORE_ERR_MSG_FMT("SQLBindParameter 7 returned: %d", ret);
                return ORCM_ERROR;
            }
        }

        if (change_value_binding) {
            if (numeric) {
                /* Value is numeric, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, (SQLPOINTER)&value_num,
                                       sizeof(value_num), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
                    opal_argv_free(data_item_argv);
                    STORE_ERR_MSG_FMT("SQLBindParameter 5 returned: %d", ret);
                    return ORCM_ERROR;
                }
                /* Pass NULL as the string value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
                    opal_argv_free(data_item_argv);
                    STORE_ERR_MSG_FMT("SQLBindParameter 6 returned: %d", ret);
                    return ORCM_ERROR;
                }
            } else {
                /* Pass NULL as the numeric value. */
                ret = SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                                       SQL_DOUBLE, 0, 0, NULL,
                                       0, &null_len);
                if (!(SQL_SUCCEEDED(ret))) {
                    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
                    opal_argv_free(data_item_argv);
                    STORE_ERR_MSG_FMT("SQLBindParameter 5 returned: %d", ret);
                    return ORCM_ERROR;
                }
                /* Value is string, bind to the appropriate value. */
                ret = SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR,
                                       SQL_VARCHAR, 0, 0, (SQLPOINTER)value_str,
                                       strlen(value_str), NULL);
                if (!(SQL_SUCCEEDED(ret))) {
                    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
                    opal_argv_free(data_item_argv);
                    STORE_ERR_MSG_FMT("SQLBindParameter 6 returned: %d", ret);
                    return ORCM_ERROR;
                }
            }
        }

        ret = SQLExecute(stmt);
        if (!(SQL_SUCCEEDED(ret))) {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            opal_argv_free(data_item_argv);
            STORE_ERR_MSG_FMT("SQLExecute returned: %d", ret);
            return ORCM_ERROR;
        }
        
        SQLCloseCursor(stmt);

        opal_argv_free(data_item_argv);
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "odbc_store succeeded");

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return ORCM_SUCCESS;
}

#define FETCH_ERR_MSG_FMT(msg, ...) \
    opal_output(0, "***********************************************\n"); \
    opal_output(0, "ODBC component fetch command failed: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "\nUnable fetch data"); \
    opal_output(0, "\n***********************************************");

static int odbc_fetch(struct orcm_db_base_module_t *imod,
                      const char *primary_key,
                      const char *key,
                      opal_list_t *kvs)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;

    SQLRETURN ret;
    SQLHSTMT stmt;
    SQLSMALLINT cols;
    SQLSMALLINT type;
    SQLULEN len;
    char query[1024];
    opal_value_t *kv;
    SQL_TIMESTAMP_STRUCT time_stamp;
    struct tm temp_tm;
    SQLUSMALLINT i;

    snprintf(query, sizeof(query), "select * from %s where %s",
             mod->table, primary_key);

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        FETCH_ERR_MSG_FMT("SQLAllocHandle returned: %d", ret);
        return ORCM_ERROR;
    }

    ret = SQLExecDirect(stmt, (SQLCHAR *)query, SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        FETCH_ERR_MSG_FMT("SQLExecDirect returned: %d", ret);
        return ORCM_ERROR;
    }

    ret = SQLNumResultCols(stmt, &cols);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        FETCH_ERR_MSG_FMT("SQLNumResultCols returned: %d", ret);
        return ORCM_ERROR;
    }

    ret = SQLFetch(stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        FETCH_ERR_MSG_FMT("SQLFetch returned: %d", ret);
        return ORCM_ERROR;
    }

    for (i = 1; i <= cols; i++) {
        ret = SQLDescribeCol(stmt, i, NULL, 0, NULL, &type, &len, NULL, NULL);
        if (!(SQL_SUCCEEDED(ret))) {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            FETCH_ERR_MSG_FMT("SQLDescribeCol returned: %d", ret);
            return ORCM_ERROR;
        }

        kv = OBJ_NEW(opal_value_t);
        switch (type) {
            case SQL_CHAR:
            case SQL_VARCHAR:
                kv->type = OPAL_STRING;
                kv->data.string = (char *)malloc(len);
                ret = SQLGetData(stmt, i, SQL_C_CHAR, kv->data.string,
                                 len, NULL);
                break;
            case SQL_DECIMAL:
            case SQL_NUMERIC:
            case SQL_REAL:
            case SQL_FLOAT:
                kv->type = OPAL_FLOAT;
                ret = SQLGetData(stmt, i, SQL_C_FLOAT, &kv->data.fval,
                                 sizeof(kv->data.fval), NULL);
                break;
            case SQL_DOUBLE:
                kv->type = OPAL_DOUBLE;
                ret = SQLGetData(stmt, i, SQL_C_DOUBLE, &kv->data.dval,
                                 sizeof(kv->data.dval), NULL);
                break;
            case SQL_SMALLINT:
                kv->type = OPAL_INT16;
                ret = SQLGetData(stmt, i, SQL_C_SSHORT, &kv->data.int16,
                                 sizeof(kv->data.int16), NULL);
                break;
            case SQL_INTEGER:
                kv->type = OPAL_INT32;
                ret = SQLGetData(stmt, i, SQL_C_SLONG, &kv->data.int32,
                                 sizeof(kv->data.int32), NULL);
                break;
            case SQL_BIT:
            case SQL_TINYINT:
                kv->type = OPAL_BYTE;
                ret = SQLGetData(stmt, i, SQL_C_UTINYINT, &kv->data.byte,
                                 sizeof(kv->data.byte), NULL);
                break;
            /* TODO: add support for dates and times */
            /*case SQL_TYPE_DATE:
            case SQL_TYPE_TIME:*/
            case SQL_TYPE_TIMESTAMP:
                kv->type = OPAL_TIMEVAL;
                ret = SQLGetData(stmt, i, SQL_C_TYPE_TIMESTAMP, &time_stamp,
                                 sizeof(time_stamp), NULL);
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
                break;
            default:
                /* TODO: unsupported type (ignore for now) */
                continue;
        }
        if (!(SQL_SUCCEEDED(ret))) {
            OBJ_RELEASE(kv);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            FETCH_ERR_MSG_FMT("SQLGetData returned: %d", ret);
            return ORCM_ERROR;
        }

        opal_list_append(kvs, &kv->super);
    }
    
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    
    return ORCM_SUCCESS;
}

#define REMOVE_ERR_MSG_FMT(msg, ...) \
    opal_output(0, "***********************************************\n"); \
    opal_output(0, "ODBC component remove command failed: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "\nUnable to remove data"); \
    opal_output(0, "\n***********************************************");

static int odbc_remove(struct orcm_db_base_module_t *imod,
                       const char *primary_key,
                       const char *key)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)imod;

    SQLRETURN ret;
    SQLHSTMT stmt;
    char query[1024];

    snprintf(query, sizeof(query), "delete from %s where %s",
             mod->table, primary_key);

    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        REMOVE_ERR_MSG_FMT("SQLAllocHandle returned: %d", ret);
        return ORCM_ERROR;
    }

    ret = SQLExecDirect(stmt, (SQLCHAR *)query, SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        REMOVE_ERR_MSG_FMT("SQLExecDirect returned: %d", ret);
        return ORCM_ERROR;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return ORCM_SUCCESS;
}

