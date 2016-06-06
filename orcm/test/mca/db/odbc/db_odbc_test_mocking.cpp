/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "db_odbc_test_mocking.h"

#include <iostream>

using namespace std;

db_odbc_test_mocking db_odbc_mocking;


extern "C" { // Mocking must use correct "C" linkages
    SQLRETURN __wrap_SQLAllocHandle(SQLSMALLINT HandleType,
                                    SQLHANDLE InputHandle, SQLHANDLE *OutputHandle)
    {
        if (NULL == db_odbc_mocking.sql_alloc_handle_callback) {
            return __real_SQLAllocHandle(HandleType, InputHandle, OutputHandle);
        } else {
            return db_odbc_mocking.sql_alloc_handle_callback(HandleType, InputHandle, OutputHandle);
        }
    }

    SQLRETURN __wrap_SQLSetEnvAttr(SQLHENV EnvironmentHandle, SQLINTEGER Attribute,
                                   SQLPOINTER Value, SQLINTEGER StringLength)
    {
        if (NULL == db_odbc_mocking.sql_set_env_attr_callback) {
            return __real_SQLSetEnvAttr(EnvironmentHandle, Attribute, Value, StringLength);
        } else {
            return db_odbc_mocking.sql_set_env_attr_callback(EnvironmentHandle,
                                                             Attribute, Value, StringLength);
        }
    }

    SQLRETURN __wrap_SQLSetConnectAttr(SQLHDBC ConnectionHandle, SQLINTEGER Attribute,
                                       SQLPOINTER Value, SQLINTEGER StringLength)
    {
        if (NULL == db_odbc_mocking.sql_set_connect_attr_callback) {
            return __real_SQLSetConnectAttr(ConnectionHandle, Attribute, Value, StringLength);
        } else {
            return db_odbc_mocking.sql_set_connect_attr_callback(ConnectionHandle,
                                                                 Attribute, Value, StringLength);
        }
    }

    SQLRETURN __wrap_SQLConnect(SQLHDBC ConnectionHandle, SQLCHAR *ServerName,
                                SQLSMALLINT NameLength1, SQLCHAR *UserName,
                                SQLSMALLINT NameLength2, SQLCHAR *Authentication,
                                SQLSMALLINT NameLength3)
    {
        if (NULL == db_odbc_mocking.sql_connect_callback) {
            return __real_SQLConnect(ConnectionHandle, ServerName, NameLength1, UserName,
                                     NameLength2, Authentication, NameLength3);
        } else {
            return db_odbc_mocking.sql_connect_callback(ConnectionHandle, ServerName,
                                 NameLength1, UserName, NameLength2, Authentication, NameLength3);
        }
    }

    SQLRETURN __wrap_SQLPrepare(SQLHSTMT StatementHandle,
                                SQLCHAR *StatementText, SQLINTEGER TextLength)
    {
        if (NULL == db_odbc_mocking.sql_prepare_callback) {
            return __real_SQLPrepare(StatementHandle, StatementText, TextLength);
        } else {
            return db_odbc_mocking.sql_prepare_callback(StatementHandle, StatementText, TextLength);
        }
    }

    SQLRETURN __wrap_SQLBindParameter(SQLHSTMT hstmt, SQLUSMALLINT ipar,
                                      SQLSMALLINT fParamType, SQLSMALLINT fCType,
                                      SQLSMALLINT fSqlType, SQLULEN cbColDef,
                                      SQLSMALLINT ibScale, SQLPOINTER rgbValue,
                                      SQLLEN cbValueMax, SQLLEN *pcbValue)
    {
        if (NULL == db_odbc_mocking.sql_bind_parameter_callback) {
            return __real_SQLBindParameter(hstmt, ipar, fParamType, fCType, fSqlType,
                                           cbColDef, ibScale, rgbValue, cbValueMax, pcbValue);
        } else {
            return db_odbc_mocking.sql_bind_parameter_callback(hstmt, ipar, fParamType, fCType,
                                   fSqlType, cbColDef, ibScale, rgbValue, cbValueMax, pcbValue);
        }
    }

    SQLRETURN __wrap_SQLExecute(SQLHSTMT StatementHandle)
    {
        if (NULL == db_odbc_mocking.sql_execute_callback) {
            return __real_SQLExecute(StatementHandle);
        } else {
            return db_odbc_mocking.sql_execute_callback(StatementHandle);
        }
    }

    SQLRETURN __wrap_SQLEndTran(SQLSMALLINT HandleType,
                                SQLHANDLE Handle,SQLSMALLINT CompletionType)
    {
        if (NULL == db_odbc_mocking.sql_end_tran_callback) {
            return __real_SQLEndTran(HandleType, Handle, CompletionType);
        } else {
            return db_odbc_mocking.sql_end_tran_callback(HandleType, Handle, CompletionType);
        }
    }
} // extern "C"
