/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef OCTL_QUERY_TESTS_H
#define OCTL_QUERY_TESTS_H

#include "gtest/gtest.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include "orcm/tools/octl/common.h"
    #include "orcm/tools/octl/query.h"
#ifdef __cplusplus
};
#endif // __cplusplus

extern bool is_mocked_db_close_result_set;

class ut_octl_query_tests: public testing::Test
{
    protected:
        static int mocker_db_component;
        static int mock_returned_num_rows;
        static orcm_db_base_API_fetch_fn_t mocked_db_fetch_function;
        static orcm_db_base_API_get_next_row_fn_t mocked_db_get_next_row;
        static orcm_db_base_API_get_num_rows_fn_t mocked_db_get_num_rows;
        static orcm_db_base_API_open_fn_t mocked_db_open;
        static orcm_db_base_API_close_fn_t mocked_db_close;
        static orcm_db_base_API_close_result_set_fn_t mocked_db_close_result_set;


        static void SetUpTestCase();
        static void TearDownTestCase();

        static void MockedDBOpen(char *name, opal_list_t *properties,
                                 orcm_db_callback_fn_t cbfunc, void *cbdata);
        static int MockedDBCloseResultSet(int dbhandle, int rshandle);
        static void MockedDBClose(int dbhandle, orcm_db_callback_fn_t cbfunc,
                                  void *cbdata);
        static void MockedDBFetch(int dbhandle, const char* view,
                                  opal_list_t *filters, opal_list_t *kvs,
                                  orcm_db_callback_fn_t cbfunc, void *cbdata);
        static int MockedDBGetNumRows(int dbhandle, int rshandle, int *num_rows);
        static void MockingDBSetDefault();
        static int MockedDBGetNextRow(int dbhandle, int rshandle, opal_list_t *row);

        static void MockDBDummyComponent(const char* component_name);

        static void UnMockDBDummyComponent();
        static void UnMockingDBSetDefault();
        static opal_list_t *GenerateDummyOpalList(int items, uint8_t data_type);
};

#endif // OCTL_QUERY_TESTS_H
