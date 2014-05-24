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
        odbc_store,
        NULL,
        odbc_fetch,
        odbc_remove
    },
};

#define INIT_ERROR_MSG(mod, msg, ...) \
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
        INIT_ERROR_MSG(mod, "SQLAllocHandle returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }
    
    ret = SQLSetEnvAttr(mod->envhandle, SQL_ATTR_ODBC_VERSION,
                        (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_argv_free(login);
        SQLFreeHandle(SQL_HANDLE_ENV, mod->envhandle);
        mod->envhandle = NULL;
        INIT_ERROR_MSG(mod, "SQLSetEnvAttr returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }
    
    ret = SQLAllocHandle(SQL_HANDLE_DBC, mod->envhandle, &mod->dbhandle);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_argv_free(login);
        mod->dbhandle = NULL;
        SQLFreeHandle(SQL_HANDLE_ENV, mod->envhandle);
        mod->envhandle = NULL;
        INIT_ERROR_MSG(mod, "SQLAllocHandle returned: %d", ret);
        return ORCM_ERR_CONNECTION_FAILED;
    }
    
    ret = SQLSetConnectAttr(mod->dbhandle, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);
    if (!(SQL_SUCCEEDED(ret))) {
        opal_argv_free(login);
        SQLFreeHandle(SQL_HANDLE_DBC, mod->dbhandle);
        mod->dbhandle = NULL;
        SQLFreeHandle(SQL_HANDLE_ENV, mod->envhandle);
        mod->envhandle = NULL;
        INIT_ERROR_MSG(mod, "SQLSetConnectAttr returned: %d", ret);
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
        INIT_ERROR_MSG(mod, "SQLConnect returned: %d", ret);
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

#define STORE_ERR_MSG(msg, ...) \
    opal_output(0, "***********************************************\n"); \
    opal_output(0, "ODBC 'insert' command failed: "); \
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
        STORE_ERR_MSG("SQLAllocHandle returned: %d", ret);
        return ORCM_ERROR;
    }
    
    ret = SQLExecDirect(stmt, (SQLCHAR *)query, SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        free(query);
        STORE_ERR_MSG("SQLExecDirect returned: %d", ret);
        return ORCM_ERROR;
    }
    
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    free(query);

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "Query succeeded");

    return OPAL_SUCCESS;
}

#define FETCH_ERR_MSG(msg, ...) \
    opal_output(0, "***********************************************\n"); \
    opal_output(0, "ODBC 'delete' command failed: "); \
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
    opal_value_t temp_kv;
    SQLUSMALLINT i;
    
    snprintf(query, sizeof(query), "select * from %s where %s",
             mod->table, primary_key);
    
    ret = SQLAllocHandle(SQL_HANDLE_STMT, mod->dbhandle, &stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        FETCH_ERR_MSG("SQLAllocHandle returned: %d", ret);
        return ORCM_ERROR;
    }
    
    ret = SQLExecDirect(stmt, (SQLCHAR *)query, SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        FETCH_ERR_MSG("SQLExecDirect returned: %d", ret);
        return ORCM_ERROR;
    }
    
    ret = SQLNumResultCols(stmt, &cols);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        FETCH_ERR_MSG("SQLNumResultCols returned: %d", ret);
        return ORCM_ERROR;
    }
    
    ret = SQLFetch(stmt);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        FETCH_ERR_MSG("SQLFetch returned: %d", ret);
        return ORCM_ERROR;
    }
    
    for (i = 1; i <= cols; i++) {
        ret = SQLDescribeCol(stmt, i, NULL, 0, NULL, &type, &len, NULL, NULL);
        if (!(SQL_SUCCEEDED(ret))) {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            FETCH_ERR_MSG("SQLDescribeCol returned: %d", ret);
            return ORCM_ERROR;
        }
        
        memset(&temp_kv, 0, sizeof(temp_kv));
        switch (type) {
            case SQL_CHAR:
            case SQL_VARCHAR:
                temp_kv.type = OPAL_STRING;
                temp_kv.data.string = (char *)malloc(len);
                ret = SQLGetData(stmt, i, SQL_C_CHAR, temp_kv.data.string,
                                 len, NULL);
                break;
            case SQL_DECIMAL:
            case SQL_NUMERIC:
            case SQL_REAL:
            case SQL_FLOAT:
            case SQL_DOUBLE:
                /* TODO: add support for double in opal_value_t (treating as 
                    float for now) */
                temp_kv.type = OPAL_FLOAT;
                ret = SQLGetData(stmt, i, SQL_C_FLOAT, &temp_kv.data.fval,
                                 sizeof(temp_kv.data.fval), NULL);
                break;
            case SQL_SMALLINT:
                temp_kv.type = OPAL_INT16;
                ret = SQLGetData(stmt, i, SQL_C_SSHORT, &temp_kv.data.int16,
                                 sizeof(temp_kv.data.int16), NULL);
                break;
            case SQL_INTEGER:
                temp_kv.type = OPAL_INT32;
                ret = SQLGetData(stmt, i, SQL_C_SLONG, &temp_kv.data.int32,
                                 sizeof(temp_kv.data.int32), NULL);
                break;
            case SQL_BIT:
            case SQL_TINYINT:
                temp_kv.type = OPAL_BYTE;
                ret = SQLGetData(stmt, i, SQL_C_UTINYINT, &temp_kv.data.byte,
                                 sizeof(temp_kv.data.byte), NULL);
                break;
            /* TODO: add support for dates and time stamps in opal_value_t */
            /*case SQL_TYPE_DATE:
            case SQL_TYPE_TIME:
            case SQL_TYPE_TIMESTAMP:*/
            default:
                /* TODO: unsupported type (ignore for now) */
                continue;
        }
        if (!(SQL_SUCCEEDED(ret))) {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            FETCH_ERR_MSG("SQLGetData returned: %d", ret);
            return ORCM_ERROR;
        }
        
        kv = OBJ_NEW(opal_value_t);
        *kv = temp_kv;
        opal_list_append(kvs, &kv->super);
    }
    
    return OPAL_SUCCESS;
}

#define REMOVE_ERR_MSG(msg, ...) \
    opal_output(0, "***********************************************\n"); \
    opal_output(0, "ODBC 'delete' command failed: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "\nUnable remove data"); \
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
        REMOVE_ERR_MSG("SQLAllocHandle returned: %d", ret);
        return ORCM_ERROR;
    }
    
    ret = SQLExecDirect(stmt, (SQLCHAR *)query, SQL_NTS);
    if (!(SQL_SUCCEEDED(ret))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        REMOVE_ERR_MSG("SQLExecDirect returned: %d", ret);
        return ORCM_ERROR;
    }
    
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    
    return OPAL_SUCCESS;
}

