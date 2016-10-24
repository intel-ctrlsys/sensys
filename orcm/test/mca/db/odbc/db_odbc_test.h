/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef GREI_ORCM_TEST_MCA_DB_ODBC_DB_ODBC_TEST_H_
#define GREI_ORCM_TEST_MCA_DB_ODBC_DB_ODBC_TEST_H_

#include "orcm/mca/db/odbc/db_odbc.h"
#include "gtest/gtest.h"

class db_odbc: public testing::Test
{
    public:
        static int alloc_handle_counter;
        static int alloc_handle_counter_limit;
        static int set_env_attr_counter;
        static int set_env_attr_counter_limit;
        static int set_connect_attr_counter;
        static int set_connect_attr_counter_limit;
        static int connect_counter;
        static int connect_counter_limit;
        static int prepare_counter;
        static int prepare_counter_limit;
        static int bind_parameter_counter;
        static int bind_parameter_counter_limit;
        static int execute_counter;
        static int execute_counter_limit;
        static int end_tran_counter;
        static int end_tran_counter_limit;

        static void SetUpTestCase();
        static void TearDownTestCase();
        static void reset_var();
        // gtest fixture required methods
        static SQLRETURN sql_alloc_handle(SQLSMALLINT, SQLHANDLE, SQLHANDLE*);
        static SQLRETURN sql_set_env_attr(SQLHENV, SQLINTEGER, SQLPOINTER, SQLINTEGER);
        static SQLRETURN sql_set_connect_attr(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER);
        static SQLRETURN sql_connect(SQLHDBC, SQLCHAR*, SQLSMALLINT, SQLCHAR*,
                                     SQLSMALLINT, SQLCHAR*, SQLSMALLINT);
        static SQLRETURN sql_prepare(SQLHSTMT, SQLCHAR*, SQLINTEGER);
        static SQLRETURN sql_bind_parameter(SQLHSTMT, SQLUSMALLINT, SQLSMALLINT,
                                            SQLSMALLINT, SQLSMALLINT, SQLULEN, SQLSMALLINT,
                                            SQLPOINTER, SQLLEN, SQLLEN*);
        static SQLRETURN sql_execute(SQLHSTMT);
        static SQLRETURN sql_end_tran(SQLSMALLINT, SQLHANDLE, SQLSMALLINT);
};

#endif /* GREI_ORCM_TEST_MCA_DB_ODBC_DB_ODBC_TEST_H_ */
