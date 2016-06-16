/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "octl_query_tests.h"

int ut_octl_query_tests::mocker_db_component = -1;
int ut_octl_query_tests::mock_returned_num_rows = 0;
orcm_db_base_API_fetch_fn_t ut_octl_query_tests::mocked_db_fetch_function = NULL;
orcm_db_base_API_get_next_row_fn_t ut_octl_query_tests::mocked_db_get_next_row = NULL;
orcm_db_base_API_get_num_rows_fn_t ut_octl_query_tests::mocked_db_get_num_rows = NULL;
orcm_db_base_API_open_fn_t ut_octl_query_tests::mocked_db_open = NULL;
orcm_db_base_API_close_fn_t ut_octl_query_tests::mocked_db_close = NULL;
orcm_db_base_API_close_result_set_fn_t ut_octl_query_tests::mocked_db_close_result_set = NULL;

void ut_octl_query_tests::SetUpTestCase()
{
    opal_init_test();
    opal_dss_register_vars();
    opal_dss_open();

    MockingDBSetDefault();
}

void ut_octl_query_tests::TearDownTestCase()
{
    UnMockingDBSetDefault();
    opal_dss_close();
}

void ut_octl_query_tests::MockedDBOpen(char *name, opal_list_t *properties,
                                 orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    fetch_cb_data *tmp_query_data = (fetch_cb_data*)cbdata;

    tmp_query_data->active = false;
    if( -1 != mocker_db_component ) {
        tmp_query_data->dbhandle = mocker_db_component;
    }

    return;
}

void ut_octl_query_tests::MockedDBClose(int dbhandle, orcm_db_callback_fn_t cbfunc,
                                  void *cbdata)
{
    fetch_cb_data *tmp_query_data = (fetch_cb_data*)cbdata;
    tmp_query_data->active = false;
    return;
}

int ut_octl_query_tests::MockedDBCloseResultSet(int dbhandle, int rshandle)
{
    return mocked_db_close_result_set ? ORCM_SUCCESS : ORCM_ERROR;
}

void ut_octl_query_tests::MockedDBFetch(int dbhandle, const char* view,
                                  opal_list_t *filters, opal_list_t *kvs,
                                  orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    fetch_cb_data *tmp_query_data = (fetch_cb_data*)cbdata;
    tmp_query_data->active = false;
    return;
}

int ut_octl_query_tests::MockedDBGetNumRows(int dbhandle, int rshandle, int *num_rows)
{
    if (mocked_db_get_num_rows) {
        return ORCM_SUCCESS;
    } else {
        if (mock_returned_num_rows) {
            *num_rows = mock_returned_num_rows;
            return ORCM_SUCCESS;
        }
        return ORCM_ERROR;
    }
}

int ut_octl_query_tests::MockedDBGetNextRow(int dbhandle, int rshandle, opal_list_t *row)
{
    return mocked_db_get_next_row ? ORCM_SUCCESS : ORCM_ERROR;
}

void ut_octl_query_tests::MockingDBSetDefault()
{
    mocked_db_fetch_function = orcm_db.fetch_function;
    mocked_db_get_next_row = orcm_db.get_next_row;
    mocked_db_get_num_rows = orcm_db.get_num_rows;
    mocked_db_open = orcm_db.open;
    mocked_db_close = orcm_db.close;
    mocked_db_close_result_set = orcm_db.close_result_set;

    orcm_db.open = MockedDBOpen;
    orcm_db.close = MockedDBClose;
    orcm_db.close_result_set = MockedDBCloseResultSet;
    orcm_db.fetch_function = MockedDBFetch;
    orcm_db.get_num_rows = MockedDBGetNumRows;
    orcm_db.get_next_row = MockedDBGetNextRow;
    MockDBDummyComponent("postgres");
}

void ut_octl_query_tests::UnMockingDBSetDefault()
{
    orcm_db.fetch_function = mocked_db_fetch_function;
    orcm_db.get_next_row = mocked_db_get_next_row;
    orcm_db.get_num_rows = mocked_db_get_num_rows;
    orcm_db.open = mocked_db_open;
    orcm_db.close = mocked_db_close;
    orcm_db.close_result_set = mocked_db_close_result_set;

    mocked_db_fetch_function = NULL;
    mocked_db_get_next_row = NULL;
    mocked_db_get_num_rows = NULL;
    mocked_db_open = NULL;
    mocked_db_close = NULL;
    mocked_db_close_result_set = NULL;
    UnMockDBDummyComponent();

    query_data.status = ORCM_SUCCESS;
    query_data.session_handle = 0;
}

void ut_octl_query_tests::MockDBDummyComponent(const char* component_name)
{
    orcm_db_handle_t *hdl = NULL;

    hdl = OBJ_NEW(orcm_db_handle_t);
    if( NULL != hdl ) {
        hdl->component = (orcm_db_base_component_t*)
                         malloc(sizeof(orcm_db_base_component_t));

        if( NULL != hdl->component ) {
            if( OPAL_SUCCESS
                == opal_pointer_array_init(&orcm_db_base.handles, 3, 1, 1) ) {
                hdl->module = NULL;
                strcpy(hdl->component->base_version.mca_component_name,
                       component_name);
                query_data.dbhandle = opal_pointer_array_add(
                                              &orcm_db_base.handles, hdl);
                mocker_db_component = query_data.dbhandle;
            } else {
                SAFEFREE(hdl->component);
                SAFE_RELEASE(hdl);
            }
        } else {
            SAFE_RELEASE(hdl);
        }
    }
}

void ut_octl_query_tests::UnMockDBDummyComponent()
{
    orcm_db_handle_t *hdl = NULL;

    if( -1 != mocker_db_component ) {
        hdl = (orcm_db_handle_t*)
              opal_pointer_array_get_item(&orcm_db_base.handles,
                                          query_data.dbhandle);
        SAFEFREE(hdl->component);
        SAFE_RELEASE(hdl);
        SAFEFREE(orcm_db_base.handles.addr);
        mocker_db_component = query_data.dbhandle = -1;
    }
}

opal_list_t *ut_octl_query_tests::GenerateDummyOpalList(int items, uint8_t data_type)
{
    int i;
    opal_list_t *list;
    opal_value_t *val;
    list = OBJ_NEW(opal_list_t);
    if (NULL == list || 0 == items) {
        return (opal_list_t*)NULL;
    }
    for (i=0; i<items; i++) {
        val = OBJ_NEW(opal_value_t);
        val->type = data_type;
        val->key = strdup("Dummy Key");
        val->data.string = strdup("Dummy data");
        opal_list_append(list, &val->super);
    }
    return list;
}

TEST_F(ut_octl_query_tests, query_event_data_full)
{
    int rc, argc = 3;
    const char *argv[] = { "2016-01-01 01:01:01", "2016-02-02 02:02:02",
                     "node[2:1-10],node_spcl[2:1-10],node01", NULL };

    rc = orcm_octl_query_func(QUERY_EVENT_DATA, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_event_snsr_data_full)
{
    int rc, argc = 5;
    const char *argv[] = { "1", "before", "10", "node[2:1-5],node01",
                           "coretemp*,freq_core[1:0-9]", NULL };

    rc = orcm_octl_query_func(QUERY_EVENT_SNSR_DATA, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_idle_full)
{
    int rc, argc = 2;
    const char *argv[] = { "10:00:00", "node01,node[2:1-5]", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_log_full)
{
    int rc, argc = 4;
    const char *argv[] = { "*logged", "2016-01-01 01:01:01", "2016-02-02 02:02:02",
                           "node02,node[2:1-5],node01", NULL };

    rc = orcm_octl_query_func(QUERY_LOG, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_node_status_full)
{
    int rc, argc = 1;
    const char *argv[] = { "node*", NULL };

    rc = orcm_octl_query_func(QUERY_NODE_STATUS, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_history_full)
{
    int rc, argc = 3;
    const char *argv[] = { "2016-01-01 01:01:01", "2016-02-02 02:02:02",
                           "node*", NULL };

    rc = orcm_octl_query_func(QUERY_HISTORY, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_sensor_full)
{
    int rc, argc = 6;
    const char *argv[] = { "freq*", "2016-01-01 01:01:01",
                           "2016-02-02 02:02:02", "30", "40", "node*", NULL };

    rc = orcm_octl_query_func(QUERY_SENSOR, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_snsr_get_inventory_full)
{
    int rc, argc = 1;
    const char *argv[] = { "node*", NULL };

    rc = orcm_octl_query_func(QUERY_SNSR_GET_INVENTORY, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_extra_arguments)
{
    int rc, argc = 4;
    const char *argv[] = { "2016-01-01", "*", "*", "node11", NULL };

    rc = orcm_octl_query_func(QUERY_HISTORY, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_missing_all_arguments)
{
    int rc, argc = 0;
    const char *argv[] = { NULL };

    rc = orcm_octl_query_func(QUERY_EVENT_DATA, argc, (char**)argv);

    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query_tests, query_invalid_date)
{
    int rc = ORCM_ERROR, argc = 1;
    const char *argv[] = { "*", NULL };

    rc = orcm_octl_query_func(QUERY_EVENT_DATA, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_invalid_integer)
{
    int rc, argc = 1;
    const char *argv[] = { "asdf", NULL };

    rc = orcm_octl_query_func(QUERY_EVENT_SNSR_DATA, argc, (char**)argv);

    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query_tests, query_invalid_interval)
{
    int rc = ORCM_ERROR, argc = 2;
    const char *argv[] = { "1", "sensor-list", NULL };

    rc = orcm_octl_query_func(QUERY_EVENT_SNSR_DATA, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_no_date)
{
    int rc = ORCM_ERROR, argc = 1;
    const char *argv[] = { "node11", NULL };

    rc = orcm_octl_query_func(QUERY_HISTORY, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_no_interval)
{
    int rc = ORCM_ERROR, argc = 1;
    const char *argv[] = { "node11", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_no_string)
{
    int rc = ORCM_ERROR, argc = 2;
    const char *argv[] = { "1", "*", NULL };

    rc = orcm_octl_query_func(QUERY_EVENT_SNSR_DATA, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_interval)
{
    int rc, argc = 2;
    const char *argv[] = { "00:10:00", "node01", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_interval_unit_h)
{
    int rc, argc = 2;
    const char *argv[] = { "10H", "node01", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_interval_unit_m)
{
    int rc, argc = 2;
    const char *argv[] = { "10M", "node01", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_interval_unit_s)
{
    int rc, argc = 2;
    const char *argv[] = { "10S", "node01", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_interval_unit_before)
{
    int rc, argc = 3;
    const char *argv[] = { "before", "10", "node01", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_interval_unit_before_neg)
{
    int rc, argc = 3;
    const char *argv[] = { "before", "-10", "node01", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_interval_unit_before_pos)
{
    int rc, argc = 3;
    const char *argv[] = { "before", "+10", "node01", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_interval_before_unsig)
{
    int rc, argc = 3;
    const char *argv[] = { "before", "00:10:00", "node01", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_interval_before_neg)
{
    int rc, argc = 3;
    const char *argv[] = { "before", "-00:10:00", "node01", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_interval_before_pos)
{
    int rc, argc = 3;
    const char *argv[] = { "before", "+00:10:00", "node01", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_interval_unit_after)
{
    int rc, argc = 3;
    const char *argv[] = { "after", "10", "node01", NULL };

    rc = orcm_octl_query_func(QUERY_IDLE, argc, (char**)argv);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, forced_validate_float_match)
{
    int rc, argc = 1;
    char *dummy_buff = NULL;
    const char *argv[] = { "10.1", NULL };

    rc = query_validate_float(&argc, (char**)argv, &dummy_buff);
    SAFEFREE(dummy_buff);

    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, forced_validate_float_no_match)
{
    int rc, argc = 1;
    char *dummy_buff = NULL;
    const char *argv[] = { "dummy string", NULL };

    rc = query_validate_float(&argc, (char**)argv, &dummy_buff);
    SAFEFREE(dummy_buff);

    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query_tests, forced_validate_string_no_match)
{
    int rc, argc = 1;
    char *dummy_buff = NULL;
    const char *argv[] = { "", NULL };

    rc = query_validate_float(&argc, (char**)argv, &dummy_buff);
    SAFEFREE(dummy_buff);

    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query_tests, forced_double_to_interval_bad_unit)
{
    int rc;
    double quantity = 10.1;
    char *dummy_buff = NULL;

    rc = query_double_to_interval(quantity, 'R', &dummy_buff);
    SAFEFREE(dummy_buff);

    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query_tests, open_database_wrong_status)
{
    int rc;

    query_data.status = ORCM_ERROR;
    rc = open_database();

    query_data.status = ORCM_SUCCESS;
    EXPECT_EQ(ORCM_ERROR, rc);
}

TEST_F(ut_octl_query_tests, open_database_invalid_component)
{
    int rc;

    UnMockDBDummyComponent();
    rc = open_database();

    MockDBDummyComponent("postgres");
    EXPECT_EQ(ORCM_ERR_NOT_FOUND, rc);
}

TEST_F(ut_octl_query_tests, is_valid_db_component_selected)
{
    bool res;

    res = is_valid_db_component_selected();
    ASSERT_TRUE(res == true);
}

TEST_F(ut_octl_query_tests, is_valid_db_component_selected_print)
{
    bool res;

    MockDBDummyComponent("print");
    res = is_valid_db_component_selected();
    ASSERT_TRUE(res == false);
}


TEST_F(ut_octl_query_tests, close_result_set_error)
{
    int rc;
    orcm_db_base_API_close_result_set_fn_t close_result_set;

    close_result_set = mocked_db_close_result_set;
    mocked_db_close_result_set = NULL;
    rc = close_database();

    mocked_db_close_result_set = close_result_set;
    EXPECT_EQ(ORCM_ERROR, rc);
}

TEST_F(ut_octl_query_tests, close_database)
{
    int rc;

    rc = close_database();
    EXPECT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, close_database_error)
{
    int rc;

    query_data.status = ORCM_ERROR;
    rc = close_database();

    query_data.status = ORCM_SUCCESS;
    EXPECT_EQ(ORCM_ERROR, rc);
}

TEST_F(ut_octl_query_tests, fetch_data_from_db)
{
    int rc;

    rc = fetch_data_from_db(QUERY_EVENT_DATA, NULL);
    EXPECT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, fetch_data_from_db_error_status)
{
    int rc;

    query_data.status = ORCM_ERROR;
    rc = fetch_data_from_db(QUERY_EVENT_DATA, NULL);

    query_data.status = ORCM_SUCCESS;
    EXPECT_EQ(ORCM_ERROR, rc);
}

TEST_F(ut_octl_query_tests, fetch_data_from_db_error_session)
{
    int rc;

    query_data.session_handle = -1;
    rc = fetch_data_from_db(QUERY_EVENT_DATA, NULL);

    query_data.session_handle = 0;
    EXPECT_EQ(ORCM_ERROR, rc);
}

TEST_F(ut_octl_query_tests, fetch_data_from_db_error_status_and_session)
{
    int rc;

    query_data.status = ORCM_ERROR;
    query_data.session_handle = -1;
    rc = fetch_data_from_db(QUERY_EVENT_DATA, NULL);

    query_data.status = ORCM_SUCCESS;
    query_data.session_handle = 0;
    EXPECT_EQ(ORCM_ERROR, rc);
}

TEST_F(ut_octl_query_tests, get_query_num_rows)
{
    int rc;

    rc = get_query_num_rows();
    EXPECT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, get_query_num_rows_error)
{
    int rc;
    orcm_db_base_API_get_num_rows_fn_t get_num_rows;

    get_num_rows = mocked_db_get_num_rows;
    mocked_db_get_num_rows = NULL;
    rc = get_query_num_rows();

    mocked_db_get_num_rows = get_num_rows;
    EXPECT_EQ(-1, rc);
}

TEST_F(ut_octl_query_tests, query_get_next_row_null_param)
{
    int rc;

    rc = query_get_next_row(NULL);
    EXPECT_EQ(ORCM_ERROR, rc);
}

TEST_F(ut_octl_query_tests, query_get_next_row_error)
{
    int rc;
    orcm_db_base_API_get_next_row_fn_t get_next_row;

    get_next_row = mocked_db_get_next_row;
    mocked_db_get_next_row = NULL;
    rc = query_get_next_row((opal_list_t*)0xdeadbeef);

    mocked_db_get_next_row = get_next_row;
    EXPECT_EQ(ORCM_ERROR, rc);
}

TEST_F(ut_octl_query_tests, query_get_next_row)
{
    int rc;

    rc = query_get_next_row((opal_list_t*)0xdeadbeef);
    EXPECT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, print_all_query_results_bad_num_rows)
{
    int rc;
    orcm_db_base_API_get_num_rows_fn_t get_num_rows;

    get_num_rows = mocked_db_get_num_rows;
    mocked_db_get_num_rows = NULL;
    rc = print_all_query_results(0, 0);
    mocked_db_get_num_rows = get_num_rows;
    EXPECT_EQ(ORCM_ERROR, rc);
}

TEST_F(ut_octl_query_tests, print_all_query_results_one_row)
{
    int rc, num_rows = 1;
    orcm_db_base_API_get_num_rows_fn_t get_num_rows;
    time_t dummy_time;
    get_num_rows = mocked_db_get_num_rows;
    mock_returned_num_rows = num_rows;
    mocked_db_get_num_rows = NULL;
    rc = print_all_query_results(&dummy_time, 1);
    mocked_db_get_num_rows = get_num_rows;
    EXPECT_EQ(num_rows, rc);
}

TEST_F(ut_octl_query_tests, print_all_query_results_two_rows)
{
    int rc, num_rows = 2;
    orcm_db_base_API_get_num_rows_fn_t get_num_rows;
    time_t dummy_time;
    get_num_rows = mocked_db_get_num_rows;
    mock_returned_num_rows = num_rows;
    mocked_db_get_num_rows = NULL;
    rc = print_all_query_results(&dummy_time, 1);
    mocked_db_get_num_rows = get_num_rows;
    EXPECT_EQ(num_rows, rc);
}


TEST_F(ut_octl_query_tests, print_all_query_results_row_error)
{
    int rc, num_rows = 1;
    orcm_db_base_API_get_num_rows_fn_t get_num_rows;
    orcm_db_base_API_get_next_row_fn_t get_next_row;
    time_t dummy_time;
    get_next_row = mocked_db_get_next_row;
    mocked_db_get_next_row = NULL;
    get_num_rows = mocked_db_get_num_rows;
    mock_returned_num_rows = num_rows;
    mocked_db_get_num_rows = NULL;

    rc = print_all_query_results(&dummy_time, 0);

    mocked_db_get_num_rows = get_num_rows;
    mocked_db_get_next_row = get_next_row;
    EXPECT_EQ(num_rows, rc);
}

TEST_F(ut_octl_query_tests, print_row_null_list)
{
    int rc;
    rc = print_row(NULL, ",", true);
    EXPECT_EQ(ORCM_ERROR, rc);
}

TEST_F(ut_octl_query_tests, print_row_three_columns_string_header)
{
    opal_list_t *row;
    int rc;
    row = GenerateDummyOpalList(3, OPAL_STRING);
    rc = print_row(row, ",", true);
    EXPECT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, print_row_three_columns_string)
{
    opal_list_t *row;
    int rc;
    row = GenerateDummyOpalList(3, OPAL_STRING);
    rc = print_row(row, ",", false);
    EXPECT_EQ(ORCM_SUCCESS, rc);
}


TEST_F(ut_octl_query_tests, print_row_three_columns)
{
    opal_list_t *row;
    int rc;
    row = GenerateDummyOpalList(3, OPAL_INT);
    rc = print_row(row, ",", true);
    EXPECT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query_tests, query_fail_open_database_error)
{
    int rc;

    query_data.status = ORCM_ERROR;
    rc = query(QUERY_EVENT_DATA, NULL, 0);

    query_data.status = ORCM_SUCCESS;
    EXPECT_EQ(ORCM_ERROR, rc);
}

TEST_F(ut_octl_query_tests, query_fail_open_database_not_found)
{
    int rc;

    UnMockDBDummyComponent();
    rc = query(QUERY_EVENT_DATA, NULL, 0);
    MockDBDummyComponent("postgres");
    EXPECT_EQ(ORCM_ERR_NOT_FOUND, rc);
}

TEST_F(ut_octl_query_tests, query_fail_fetch_data)
{
    int rc;
    int session = query_data.session_handle;
    query_data.session_handle = -1;
    rc = query(QUERY_EVENT_DATA, NULL, 0);

    query_data.session_handle = session;
    EXPECT_EQ(ORCM_ERROR, rc);
}

TEST_F(ut_octl_query_tests, query_close_database_error)
{
    int rc;
    orcm_db_base_API_close_result_set_fn_t close_result_set;

    close_result_set = mocked_db_close_result_set;
    mocked_db_close_result_set = NULL;
    rc = query(QUERY_EVENT_DATA, NULL, 0);

    mocked_db_close_result_set = close_result_set;
    EXPECT_EQ(ORCM_ERROR, rc);
}
