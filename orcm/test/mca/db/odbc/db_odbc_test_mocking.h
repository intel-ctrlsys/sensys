/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef DB_ODBC_TEST_MOCKING_H
#define DB_ODBC_TEST_MOCKING_H


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include "orcm/mca/db/odbc/db_odbc.h"
    extern SQLRETURN __real_SQLAllocHandle(SQLSMALLINT, SQLHANDLE, SQLHANDLE*);
    extern SQLRETURN __real_SQLSetEnvAttr(SQLHENV, SQLINTEGER, SQLPOINTER, SQLINTEGER);
    extern SQLRETURN __real_SQLSetConnectAttr(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER);
    extern SQLRETURN __real_SQLConnect(SQLHDBC, SQLCHAR*, SQLSMALLINT, SQLCHAR*,
                                     SQLSMALLINT, SQLCHAR*, SQLSMALLINT);
    extern SQLRETURN __real_SQLPrepare(SQLHSTMT, SQLCHAR*, SQLINTEGER);
    extern SQLRETURN __real_SQLBindParameter(SQLHSTMT, SQLUSMALLINT, SQLSMALLINT,
                                            SQLSMALLINT, SQLSMALLINT, SQLULEN, SQLSMALLINT,
                                            SQLPOINTER, SQLLEN, SQLLEN*);
    extern SQLRETURN __real_SQLExecute(SQLHSTMT);
    extern SQLRETURN __real_SQLEndTran(SQLSMALLINT, SQLHANDLE, SQLSMALLINT);
#ifdef __cplusplus
}
#endif // __cplusplus

typedef SQLRETURN (*sql_alloc_handle_callback_fn_t)(SQLSMALLINT, SQLHANDLE, SQLHANDLE*);
typedef SQLRETURN (*sql_set_env_attr_callback_fn_t)(SQLHENV, SQLINTEGER, SQLPOINTER, SQLINTEGER);
typedef SQLRETURN (*sql_set_connect_attr_callback_fn_t)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER);
typedef SQLRETURN (*sql_connect_callback_fn_t)(SQLHDBC, SQLCHAR*, SQLSMALLINT, SQLCHAR*,
                                               SQLSMALLINT, SQLCHAR*, SQLSMALLINT);
typedef SQLRETURN (*sql_prepare_callback_fn_t)(SQLHSTMT, SQLCHAR*, SQLINTEGER);
typedef SQLRETURN (*sql_bind_parameter_callback_fn_t)(SQLHSTMT, SQLUSMALLINT, SQLSMALLINT,
                                                      SQLSMALLINT, SQLSMALLINT, SQLULEN, SQLSMALLINT,
                                                      SQLPOINTER, SQLLEN, SQLLEN*);
typedef SQLRETURN (*sql_execute_callback_fn_t)(SQLHSTMT);
typedef SQLRETURN (*sql_end_tran_callback_fn_t)(SQLSMALLINT, SQLHANDLE, SQLSMALLINT);

class db_odbc_test_mocking
{
    public:

        // Public Callbacks
        sql_alloc_handle_callback_fn_t sql_alloc_handle_callback;
        sql_set_env_attr_callback_fn_t sql_set_env_attr_callback;
        sql_set_connect_attr_callback_fn_t sql_set_connect_attr_callback;
        sql_connect_callback_fn_t sql_connect_callback;
        sql_prepare_callback_fn_t sql_prepare_callback;
        sql_bind_parameter_callback_fn_t sql_bind_parameter_callback;
        sql_execute_callback_fn_t sql_execute_callback;
        sql_end_tran_callback_fn_t sql_end_tran_callback;
};

extern db_odbc_test_mocking db_odbc_mocking;

#endif // DB_ODBC_TEST_MOCKING_H
