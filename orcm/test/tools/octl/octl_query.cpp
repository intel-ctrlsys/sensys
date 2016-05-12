/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "octl_tests.h"

// OPAL
#include "opal/dss/dss.h"

// ORCM
#include "orcm/tools/octl/common.h"
#include "orcm/mca/db/db.h"

// C++
#include <iostream>
#include <iomanip>

// Fixture
using namespace std;
enum FilterSizes{zero_filters, one_filters, two_filters,
                 two_filters_empty_key, two_filters_corrupted_value_type,
                 three_filters};

extern "C" {
    int query_db_stream(int cmd, opal_list_t *filters, uint32_t *results_count,
                        int *stream_index);
    int create_buffer_from_filters(opal_buffer_t **buffer, opal_list_t *filters,
                                   orcm_rm_cmd_flag_t cmd);
    int print_results_from_stream(uint32_t results_count, int stream_index,
                                  double start_time, double stop_time);
    int orcm_octl_query_event_data(int cmd, char **argv);
    opal_list_t *build_filters_list(int cmd, char **argv);
    opal_list_t *create_query_event_data_filter(int argc, char **argv);
    opal_list_t *create_query_event_snsr_data_filter(int argc, char **argv);
    opal_list_t *create_query_event_date_filter(int argc, char **argv);
    orcm_db_filter_t *create_string_filter(char *field, char *string,
                                           orcm_db_comparison_op_t op);
    int split_db_results(char *db_res, char ***db_results);
    char *add_to_str_date(char *date, int seconds);
    size_t opal_list_get_size(opal_list_t *list);
    char *assemble_datetime(char *date_str, char *time_str);
    bool replace_wildcard(char **filter_me, bool quit_on_first);
};

void ut_octl_query::SetUpTestCase()
{
    opal_dss_register_vars();
    opal_dss_open();

    orte_rml.recv_buffer_nb = MockedRecvBuffer;
}

void ut_octl_query::TearDownTestCase()
{
    opal_dss_close();
}

void ut_octl_query::MockedRecvBuffer(orte_process_name_t* peer,
                                     orte_rml_tag_t tag,
                                     bool persistent,
                                     orte_rml_buffer_callback_fn_t cbfunc,
                                     void* cbdata)
{
    NULL;
}

int ut_octl_query::MockedSendBufferFromDbQuery(orte_process_name_t* peer,
                              struct opal_buffer_t* buffer,
                              orte_rml_tag_t tag,
                              orte_rml_buffer_callback_fn_t cbfunc,
                              void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    int rv = ORTE_SUCCESS;
    int packed_int = ORCM_SUCCESS;
    uint32_t number_of_rows = 100000000;
    int stream_index = 1;
    xfer->active = false;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    opal_dss.pack(&xfer->data, &packed_int, 1, OPAL_INT);
    opal_dss.pack(&xfer->data, &number_of_rows, 1, OPAL_UINT32);
    opal_dss.pack(&xfer->data, &stream_index, 1, OPAL_INT);

    return rv;
}

int ut_octl_query::MockedSendBufferFromPrint(orte_process_name_t* peer,
                              struct opal_buffer_t* buffer,
                              orte_rml_tag_t tag,
                              orte_rml_buffer_callback_fn_t cbfunc,
                              void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    int rv = ORTE_SUCCESS;
    int packed_int = ORCM_SUCCESS;
    uint32_t number_of_rows = 1;
    int stream_index = 1;
    char *packed_str = NULL;
    xfer->active = false;
    packed_str = strdup("This is a str sent by the SCD framework");
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    opal_dss.pack(&xfer->data, &stream_index, 1, OPAL_INT);
    opal_dss.pack(&xfer->data, &number_of_rows, 1, OPAL_UINT32);
    opal_dss.pack(&xfer->data, &packed_str, 1, OPAL_STRING);

    return rv;
}

int ut_octl_query::MockedFailure1SendBuffer(orte_process_name_t* peer,
                              struct opal_buffer_t* buffer,
                              orte_rml_tag_t tag,
                              orte_rml_buffer_callback_fn_t cbfunc,
                              void* cbdata)
{
    return ORTE_ERR_BAD_PARAM;
}

int ut_octl_query::MockedFailure2SendBuffer(orte_process_name_t* peer,
                              struct opal_buffer_t* buffer,
                              orte_rml_tag_t tag,
                              orte_rml_buffer_callback_fn_t cbfunc,
                              void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    xfer->active = false;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    return ORCM_SUCCESS;
}

int ut_octl_query::MockedFailure3SendBuffer(orte_process_name_t* peer,
                              struct opal_buffer_t* buffer,
                              orte_rml_tag_t tag,
                              orte_rml_buffer_callback_fn_t cbfunc,
                              void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    int rv = ORTE_SUCCESS;
    int packed_int = ORCM_SUCCESS;
    uint32_t number_of_rows = 100000000;
    int stream_index = 1;
    xfer->active = false;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    opal_dss.pack(&xfer->data, &packed_int, 1, OPAL_INT);

    return rv;
}

int ut_octl_query::MockedFailure4SendBuffer(orte_process_name_t* peer,
                              struct opal_buffer_t* buffer,
                              orte_rml_tag_t tag,
                              orte_rml_buffer_callback_fn_t cbfunc,
                              void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    int rv = ORTE_SUCCESS;
    int packed_int = ORCM_SUCCESS;
    uint32_t number_of_rows = 100000000;
    int stream_index = 1;
    xfer->active = false;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    opal_dss.pack(&xfer->data, &packed_int, 1, OPAL_INT);
    opal_dss.pack(&xfer->data, &number_of_rows, 1, OPAL_UINT32);

    return rv;
}

int ut_octl_query::MockedFailure5SendBuffer(orte_process_name_t* peer,
                              struct opal_buffer_t* buffer,
                              orte_rml_tag_t tag,
                              orte_rml_buffer_callback_fn_t cbfunc,
                              void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    int rv = ORTE_SUCCESS;
    int packed_int = ORCM_SUCCESS;
    uint32_t number_of_rows = 1;
    int stream_index = 1;

    xfer->active = false;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    opal_dss.pack(&xfer->data, &stream_index, 1, OPAL_INT);
    opal_dss.pack(&xfer->data, &number_of_rows, 1, OPAL_UINT32);

    return rv;
}

int ut_octl_query::PrintResultsFromStream(uint32_t results_count,
                                          int stream_index, double start_time,
                                          double stop_time)
{
    return print_results_from_stream(results_count, stream_index, start_time,
                                     stop_time);
}

int ut_octl_query::ReplaceWildcard(const char *input_string,
                                   const char *expected_string,
                                   bool stop_on_first)
{
    char *tmp = strdup(input_string);
    bool res;
    res = replace_wildcard(&tmp, stop_on_first);
    return strcmp(expected_string, tmp);
}

int ut_octl_query::AssembleDatetime(const char *date,
                                    const char *time,
                                    const char *expected_datetime)
{
    char *tmp_date;
    char *tmp_time;
    int rc;
    tmp_date = strdup(date);
    tmp_time = strdup(time);
    char *datetime;
    datetime = assemble_datetime(tmp_date, tmp_time);
    if (datetime == NULL)
        return -1;
    cout << "The datetime is " << datetime << "\n";
    return strcmp(expected_datetime, datetime);
}

int ut_octl_query::EventDataFilter(int argc, const char **args)
{
    opal_list_t *results_list;
    results_list = create_query_event_data_filter(argc, (char**)args);
    if (NULL != results_list) {
        return (uint32_t)opal_list_get_size(results_list);
    }
    return 0;

}

int ut_octl_query::EventSensorDataFilter(int argc, const char **args)
{
    opal_list_t *results_list;
    results_list = create_query_event_snsr_data_filter(argc, (char**)args);
    if (NULL != results_list) {
        return (uint32_t)opal_list_get_size(results_list);
    }
    return 0;
}

int ut_octl_query::AddToStrDate(const char *date,
                                const char *expected_date,
                                int seconds)
{
    char *result_date;
    int rc;
    result_date = add_to_str_date((char*)date, seconds);
    rc = strcmp(result_date, expected_date);
    return rc;
}

orcm_db_filter_t* ut_octl_query::CreateStringFilter(const char *field, const char *string,
                                                    orcm_db_comparison_op_t op)
{
    return create_string_filter((char *)field, (char *)string, op);
}

opal_list_t * ut_octl_query::CreateFiltersList(int filters_size)
{
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;

    filters_list = OBJ_NEW(opal_list_t);
    switch(filters_size) {
        case zero_filters:
            break;
        case one_filters:
            filter_item = CreateStringFilter("data_item", "%", CONTAINS);
            opal_list_append(filters_list, &filter_item->value.super);
            break;
        case two_filters:
            filter_item = CreateStringFilter("data_item", "%", CONTAINS);
            opal_list_append(filters_list, &filter_item->value.super);
            filter_item = CreateStringFilter("hostname", "%", CONTAINS);
            opal_list_append(filters_list, &filter_item->value.super);
            break;
        case two_filters_empty_key:
            filter_item = CreateStringFilter("", "%", CONTAINS);
            opal_list_append(filters_list, &filter_item->value.super);
            filter_item = CreateStringFilter("hostname", "%", CONTAINS);
            opal_list_append(filters_list, &filter_item->value.super);
            break;
        case two_filters_corrupted_value_type:
            filter_item = CreateStringFilter("data_item", "%", CONTAINS);
            filter_item->value.type = OPAL_INT;
            opal_list_append(filters_list, &filter_item->value.super);
            filter_item = CreateStringFilter("hostname", "%", CONTAINS);
            opal_list_append(filters_list, &filter_item->value.super);
            break;
        case three_filters:
            filter_item = CreateStringFilter("data_item", "%", CONTAINS);
            opal_list_append(filters_list, &filter_item->value.super);
            filter_item = CreateStringFilter("hostname", "%", CONTAINS);
            opal_list_append(filters_list, &filter_item->value.super);
            filter_item = CreateStringFilter("another", "filter", NONE);
            opal_list_append(filters_list, &filter_item->value.super);
            break;
    }

    return filters_list;
}
int ut_octl_query::QueryDbStreamVaryingFilters(uint32_t *results_count,
                                             int *stream_index,
                                             int filters_size)
{
    int rc;
    int cmd = -1;
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;

    filters_list = CreateFiltersList(filters_size);
    rc = query_db_stream(cmd, filters_list, results_count, stream_index);

    return rc;
}

int ut_octl_query::CreateBufferFromFilters(opal_buffer_t **buffer_to_send,
                                           opal_list_t *filterslist,
                                           orcm_rm_cmd_flag_t cmd)
{
    int rc;
    rc = create_buffer_from_filters(buffer_to_send, filterslist, cmd);
    return rc;
}

int ut_octl_query::CreateBufferFromVaryingFilters(opal_buffer_t **buffer_to_send,
                                           opal_list_t *filterslist,
                                           orcm_rm_cmd_flag_t cmd,
                                           int filters_size)
{
    int rc;
    opal_list_t *filters_list = NULL;

    filters_list = CreateFiltersList(filters_size);
    rc = create_buffer_from_filters(buffer_to_send, filters_list, cmd);
    return rc;
}



TEST_F(ut_octl_query, event_data_node_list_incomplete)
{
    const char* cmdlist[4] = {
        "sensor",
        "query",
        "mynode",
        NULL
    };
    int rc;
    rc = orcm_octl_query_event_data(ORCM_GET_DB_QUERY_EVENT_DATA_COMMAND,
                                    (char**)cmdlist);
    EXPECT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, event_data_node_list_empty)
{
    const char* cmdlist[3] = {
        "sensor",
        "query",
        NULL
    };
    int rc;
    rc = orcm_octl_query_event_data(ORCM_GET_DB_QUERY_EVENT_DATA_COMMAND,
                                    (char**)cmdlist);
    EXPECT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, event_data_zero_null)
{
    int rc;
    rc = orcm_octl_query_event_data(0, NULL);
    EXPECT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, event_data_command_null)
{
    int rc;
    rc = orcm_octl_query_event_data(ORCM_GET_DB_QUERY_EVENT_DATA_COMMAND, NULL);
    EXPECT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, build_filters_list_data_command_null_argv)
{
    opal_list_t *results_list = NULL;
    results_list = build_filters_list(ORCM_GET_DB_QUERY_EVENT_DATA_COMMAND, NULL);
    EXPECT_EQ(NULL, results_list);
}

TEST_F(ut_octl_query, event_data_filter_no_params)
{
    const char* cmdlist[5] = {
        "query",
        "event",
        "data",
        "mynode",
        NULL
    };
    uint32_t row_count;
    row_count = EventDataFilter(4, cmdlist);
    EXPECT_EQ(2, row_count);
}

TEST_F(ut_octl_query, event_data_filter_bad_params)
{
    opal_list_t *results_list = NULL;
    results_list = create_query_event_data_filter(1, NULL);
    ASSERT_TRUE(results_list == NULL);
}

TEST_F(ut_octl_query, event_data_filter_good_argc_null_argv_1)
{
    uint32_t row_count;
    row_count = EventDataFilter(4, NULL);
    EXPECT_EQ(0, row_count);
}

TEST_F(ut_octl_query, event_data_filter_good_argc_null_argv_2)
{
    uint32_t row_count;
    row_count = EventDataFilter(8, NULL);
    EXPECT_EQ(0, row_count);
}

TEST_F(ut_octl_query, event_data_filter_params_as_wildcards)
{
    const char* cmdlist[9] = {
        "query",
        "event",
        "data",
        "*",
        "*",
        "*",
        "*",
        "mynode",
        NULL
    };
    uint32_t row_count;
    row_count = EventDataFilter(8, cmdlist);
    EXPECT_EQ(1, row_count);
}

TEST_F(ut_octl_query, event_data_filter_full_params)
{
    const char* cmdlist[9] = {
        "query",
        "event",
        "data",
        "2016-12-12",
        "09:00:00",
        "2017-01-01",
        "09:00:00",
        "mynode",
        NULL
    };
    uint32_t row_count;
    row_count = EventDataFilter(8, cmdlist);
    EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_data_filter_end_datetime_as_wildcard)
{
    const char* cmdlist[9] = {
        "query",
        "event",
        "data",
        "2016-12-12",
        "09:00:00",
        "*",
        "*",
        "mynode",
        NULL
    };
    uint32_t row_count;
    row_count = EventDataFilter(8, cmdlist);
    EXPECT_EQ(2, row_count);
}

TEST_F(ut_octl_query, event_data_filter_end_time_as_wildcard)
{
    const char* cmdlist[9] = {
        "query",
        "event",
        "data",
        "2016-12-12",
        "09:00:00",
        "2017-01-01",
        "*",
        "mynode",
        NULL
    };
    uint32_t row_count;
    row_count = EventDataFilter(8, cmdlist);
    EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_data_filter_start_datetime_as_wildcard)
{
    const char* cmdlist[9] = {
        "query",
        "event",
        "data",
        "*",
        "*",
        "2017-01-01",
        "09:00:00",
        "mynode",
        NULL
    };
    uint32_t row_count;
    row_count = EventDataFilter(8, cmdlist);
    EXPECT_EQ(2, row_count);
}

// create_query_event_snsr_data_filter function tests

TEST_F(ut_octl_query, event_sensor_data_filter_good_argc_null_argv)
{
    uint32_t row_count;
    row_count = EventSensorDataFilter(8, NULL);
    EXPECT_EQ(0, row_count);
}

TEST_F(ut_octl_query, event_sensor_data_filter_full_before_params)
{
    const char* cmdlist[9] = {
        "query",
        "event",
        "sensor-data",
        "1234",
        "before",
        "1",
        "coretemp",
        "mynode",
    };
    uint32_t row_count;
    row_count = EventSensorDataFilter(8, cmdlist);
    EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_sensor_data_filter_full_after_params)
{
  const char* cmdlist[9] = {
    "query",
    "event",
    "sensor-data",
    "1234",
    "before",
    "1",
    "coretemp",
    "mynode",
  };
  uint32_t row_count;
  row_count = EventSensorDataFilter(8, cmdlist);
  EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_sensor_data_filter_full_before_empty_id)
{
  const char* cmdlist[9] = {
    "query",
    "event",
    "sensor-data",
    "",
    "before",
    "1",
    "coretemp",
    "mynode",
  };
  uint32_t row_count;
  row_count = EventSensorDataFilter(8, cmdlist);
  EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_sensor_data_filter_full_before_empty_minutes)
{
  const char* cmdlist[9] = {
    "query",
    "event",
    "sensor-data",
    "1234",
    "before",
    "",
    "coretemp",
    "mynode",
  };
  uint32_t row_count;
  row_count = EventSensorDataFilter(8, cmdlist);
  EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_sensor_data_filter_full_before_empty_sensor)
{
  const char* cmdlist[9] = {
    "query",
    "event",
    "sensor-data",
    "1234",
    "before",
    "1",
    "",
    "mynode",
  };
  uint32_t row_count;
  row_count = EventSensorDataFilter(8, cmdlist);
  EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_sensor_data_filter_full_before_two_sensors)
{
  const char* cmdlist[9] = {
    "query",
    "event",
    "sensor-data",
    "1234",
    "before",
    "1",
    "coretemp,syslog",
    "mynode",
  };
  uint32_t row_count;
  row_count = EventSensorDataFilter(8, cmdlist);
  EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_sensor_data_filter_full_after_empty_id)
{
  const char* cmdlist[9] = {
    "query",
    "event",
    "sensor-data",
    "",
    "after",
    "1",
    "coretemp",
    "mynode",
  };
  uint32_t row_count;
  row_count = EventSensorDataFilter(8, cmdlist);
  EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_sensor_data_filter_full_after_empty_minutes)
{
  const char* cmdlist[9] = {
    "query",
    "event",
    "sensor-data",
    "1234",
    "after",
    "",
    "coretemp",
    "mynode",
  };
  uint32_t row_count;
  row_count = EventSensorDataFilter(8, cmdlist);
  EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_sensor_data_filter_full_after_empty_sensor)
{
  const char* cmdlist[9] = {
    "query",
    "event",
    "sensor-data",
    "1234",
    "after",
    "1",
    "",
    "mynode",
  };
  uint32_t row_count;
  row_count = EventSensorDataFilter(8, cmdlist);
  EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_sensor_data_filter_full_after_two_sensors)
{
  const char* cmdlist[9] = {
    "query",
    "event",
    "sensor-data",
    "1234",
    "after",
    "1",
    "coretemp,syslog",
    "mynode",
  };
  uint32_t row_count;
  row_count = EventSensorDataFilter(8, cmdlist);
  EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_sensor_data_filter_full_before_bad_id)
{
  const char* cmdlist[9] = {
    "query",
    "event",
    "sensor-data",
    "bad_id",
    "after",
    "1",
    "coretemp,syslog",
    "mynode",
  };
  uint32_t row_count;
  row_count = EventSensorDataFilter(8, cmdlist);
  EXPECT_EQ(3, row_count);
}

TEST_F(ut_octl_query, event_sensor_data_filter_full_bad_action)
{
  const char* cmdlist[9] = {
    "query",
    "event",
    "sensor-data",
    "1234",
    "bad_action",
    "1",
    "coretemp,syslog",
    "mynode",
  };
  uint32_t row_count;
  row_count = EventSensorDataFilter(8, cmdlist);
  EXPECT_EQ(0, row_count);
}

// create_query_event_date_filter

TEST_F(ut_octl_query, event_date_filter_good_argc_null_argv)
{
    uint32_t row_count;
    opal_list_t *results_list;
    results_list = create_query_event_date_filter(3, NULL);
    ASSERT_TRUE(NULL == results_list);
}

TEST_F(ut_octl_query, event_date_filter_bad_argc)
{
    uint32_t row_count;
    opal_list_t *results_list;
    results_list = create_query_event_date_filter(1, NULL);
    ASSERT_TRUE(NULL == results_list);
}

// split_db_results function tests

TEST_F(ut_octl_query, split_db_results_null_params)
{
    size_t rc;
    rc = split_db_results(NULL, NULL);
    EXPECT_EQ(0, rc);
}

TEST_F(ut_octl_query, split_db_results_full_params)
{
    size_t rc;
    const char* str = "some,comma,separated,string";
    char** db_results;
    rc = split_db_results((char*)str, (char***)&db_results);
    EXPECT_EQ(4, rc);
}

TEST_F(ut_octl_query, split_db_results_string_without_commas)
{
    size_t rc;
    const char* str = "some-comma-separated-string";
    char** db_results;
    rc = split_db_results((char*)str, (char***)&db_results);
    EXPECT_EQ(1, rc);
}

// add_to_str_date function tests

TEST_F(ut_octl_query, add_to_str_date_null_date)
{
    char *res;
    res = add_to_str_date(NULL, 0);
    ASSERT_TRUE(res == NULL);
}

TEST_F(ut_octl_query, add_to_str_date_add_1_second)
{
    int rc;
    rc = AddToStrDate("2016-02-19 09:00:00", "2016-02-19 09:00:01", 1);
    EXPECT_EQ(0, rc);
}

TEST_F(ut_octl_query, add_to_str_date_negative_1_second)
{
    int rc;
    rc = AddToStrDate("2016-02-19 09:00:00", "2016-02-19 09:00:01", 1);
    EXPECT_EQ(0, rc);
}

// assemble_datetime function tests

TEST_F(ut_octl_query, assemble_datetime_with_null_params)
{
    char *str = NULL;
    str = assemble_datetime(NULL, NULL);
    ASSERT_TRUE(str == NULL);
}

TEST_F(ut_octl_query, assemble_datetime_with_first_null_param)
{
    char *str = NULL;
    char tmp[] = "Somestring";
    str = assemble_datetime(NULL, tmp);
    ASSERT_TRUE(str == NULL);
}

TEST_F(ut_octl_query, assemble_datetime_with_second_null_param)
{
    char *str = NULL;
    char tmp[] = "Somestring";
    str = assemble_datetime(tmp, NULL);
    ASSERT_TRUE(str == NULL);
}

TEST_F(ut_octl_query, assemble_datetime_correct_usage)
{
    int rc;
    rc = AssembleDatetime("2016-02-22", "09:12:12", "2016-02-22 09:12:12");
    EXPECT_EQ(0, rc);
}

TEST_F(ut_octl_query, assemble_datetime_time_as_wildcard)
{
    int rc;
    rc = AssembleDatetime("2016-02-22", "*", "2016-02-22 ");
    EXPECT_EQ(0, rc);
}

TEST_F(ut_octl_query, assemble_datetime_date_as_wildcard)
{
    int rc;
    rc = AssembleDatetime("*", "09:12:12", "2016-02-22 ");
    EXPECT_EQ(-1, rc);
}

TEST_F(ut_octl_query, assemble_datetime_both_as_wildcard)
{
    int rc;
    rc = AssembleDatetime("*", "*", "2016-02-22 ");
    EXPECT_EQ(-1, rc);
}

// replace_wildcard function tests

TEST_F(ut_octl_query, replace_wildcard_with_null_param)
{
    bool res;
    res = replace_wildcard(NULL, false);
    EXPECT_EQ(false, res);
}

TEST_F(ut_octl_query, replace_wildcard_with_null_string)
{
    bool res;
    char *tmp = NULL;
    res = replace_wildcard(&tmp, false);
    EXPECT_EQ(false, res);
}

TEST_F(ut_octl_query, replace_wildcard_one_char_string)
{
    int rc;
    rc = ReplaceWildcard("a", "a", false);
    EXPECT_EQ(0, rc);
}

TEST_F(ut_octl_query, replace_wildcard_one_wildcard)
{
    int rc;
    rc = ReplaceWildcard("*", "%", false);
    EXPECT_EQ(0, rc);
}

TEST_F(ut_octl_query, replace_wildcard_in_first_char)
{
    int rc;
    rc = ReplaceWildcard("*somestring", "%somestring", false);
    EXPECT_EQ(0, rc);
}

TEST_F(ut_octl_query, replace_wildcard_in_the_middle)
{
    int rc;
    rc = ReplaceWildcard("Some*string", "Some%string", false);
    EXPECT_EQ(0, rc);
}

TEST_F(ut_octl_query, replace_wildcard_in_the_end)
{
    int rc;
    rc = ReplaceWildcard("Somestring*", "Somestring%", false);
    EXPECT_EQ(0, rc);
}

TEST_F(ut_octl_query, replace_wildcard_in_first_char_with_stop)
{
    int rc;
    rc = ReplaceWildcard("*somestring", "%", true);
    EXPECT_EQ(0, rc);
}

TEST_F(ut_octl_query, replace_wildcard_in_the_middle_with_stop)
{
    int rc;
    rc = ReplaceWildcard("Some*string", "Some%", true);
    EXPECT_EQ(0, rc);
}

TEST_F(ut_octl_query, replace_wildcard_in_the_end_with_stop)
{
    int rc;
    rc = ReplaceWildcard("Somestring*", "Somestring%", true);
    EXPECT_EQ(0, rc);
}

// query_db_stream function tests

TEST_F(ut_octl_query, query_db_stream_valid_filters_rc)
{
    int rc;
    uint32_t results_count = 0;
    int stream_index = 0;

    orte_rml.send_buffer_nb = MockedSendBufferFromDbQuery;
    rc = QueryDbStreamVaryingFilters(&results_count, &stream_index, two_filters);

    EXPECT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query, query_db_stream_valid_filters_results_count)
{
    int rc;
    uint32_t results_count = 0;
    int stream_index = 0;

    orte_rml.send_buffer_nb = MockedSendBufferFromDbQuery;
    rc = QueryDbStreamVaryingFilters(&results_count, &stream_index, two_filters);

    EXPECT_NE(0, results_count);
}

TEST_F(ut_octl_query, query_db_stream_valid_filters_stream_index)
{
    int rc;
    uint32_t results_count = 0;
    int stream_index = 0;

    orte_rml.send_buffer_nb = MockedSendBufferFromDbQuery;
    rc = QueryDbStreamVaryingFilters(&results_count, &stream_index, two_filters);

    EXPECT_NE(0, stream_index);
}

TEST_F(ut_octl_query, query_db_stream_valid_filters_null_results_count)
{
    int rc;
    int stream_index = 0;

    rc = QueryDbStreamVaryingFilters(NULL, &stream_index, two_filters);

    EXPECT_EQ(OPAL_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, query_db_stream_valid_filters_null_stream_index)
{
    int rc;
    uint32_t results_count = 0;

    rc = QueryDbStreamVaryingFilters(&results_count, NULL, two_filters);

    EXPECT_EQ(OPAL_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, query_db_stream_invalid_filters)
{
    int rc;
    uint32_t results_count = 0;
    int stream_index = 0;

    rc = QueryDbStreamVaryingFilters(&results_count, &stream_index,
                                     one_filters);

    EXPECT_EQ(OPAL_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, query_db_stream_no_receiver)
{
    int rc;
    uint32_t results_count = 0;
    int stream_index = 0;

    orte_rml.send_buffer_nb = MockedFailure1SendBuffer;
    rc = QueryDbStreamVaryingFilters(&results_count, &stream_index,
                                     two_filters);
    orte_rml.send_buffer_nb = MockedSendBufferFromDbQuery;

    EXPECT_EQ(OPAL_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, query_db_stream_first_unpack_error)
{
    int rc;
    uint32_t results_count = 0;
    int stream_index = 0;
    orte_rml.send_buffer_nb = MockedFailure2SendBuffer;
    rc = QueryDbStreamVaryingFilters(&results_count, &stream_index,
                                     two_filters);
    orte_rml.send_buffer_nb = MockedSendBufferFromDbQuery;
    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER, rc);
}

TEST_F(ut_octl_query, query_db_stream_second_unpack_error)
{
    int rc;
    uint32_t results_count = 0;
    int stream_index = 0;
    orte_rml.send_buffer_nb = MockedFailure3SendBuffer;
    rc = QueryDbStreamVaryingFilters(&results_count, &stream_index,
                                     two_filters);
    orte_rml.send_buffer_nb = MockedSendBufferFromDbQuery;
    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER, rc);
}

TEST_F(ut_octl_query, query_db_stream_third_unpack_error)
{
    int rc;
    uint32_t results_count = 0;
    int stream_index = 0;
    orte_rml.send_buffer_nb = MockedFailure4SendBuffer;
    rc = QueryDbStreamVaryingFilters(&results_count, &stream_index,
                                     two_filters);
    orte_rml.send_buffer_nb = MockedSendBufferFromDbQuery;
    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER, rc);
}

// create_buffer_from_filters function tests

TEST_F(ut_octl_query, create_buffer_from_filters_null_filters)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    orcm_rm_cmd_flag_t command = ORCM_GET_DB_QUERY_HISTORY_COMMAND;

    rc = CreateBufferFromFilters(&buffer, NULL, command);
    EXPECT_EQ(OPAL_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, create_buffer_from_filters_buffer_unharmed)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    orcm_rm_cmd_flag_t command = ORCM_GET_DB_QUERY_HISTORY_COMMAND;

    rc = CreateBufferFromFilters(&buffer, NULL, command);
    EXPECT_EQ(NULL, buffer);
}

TEST_F(ut_octl_query, create_buffer_from_filters_first_pack_failed)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    orcm_rm_cmd_flag_t command = ORCM_GET_DB_QUERY_HISTORY_COMMAND;

    filters_list = OBJ_NEW(opal_list_t);
    opal_dss_close();
    rc = CreateBufferFromFilters(&buffer, filters_list,
                                 (orcm_rm_cmd_flag_t)command);
    EXPECT_EQ(OPAL_ERR_PACK_FAILURE, rc);
    opal_dss_register_vars();
    opal_dss_open();
}


TEST_F(ut_octl_query, create_buffer_from_filters_empty_filters)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_rm_cmd_flag_t command = ORCM_GET_DB_QUERY_HISTORY_COMMAND;

    rc = CreateBufferFromFilters(&buffer, filters_list,
                                 (orcm_rm_cmd_flag_t)command);
    EXPECT_EQ(OPAL_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, create_buffer_from_filters_empty_filters_buffer_unharmed)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_rm_cmd_flag_t command = ORCM_GET_DB_QUERY_HISTORY_COMMAND;


    rc = CreateBufferFromVaryingFilters(&buffer, filters_list,
                                        (orcm_rm_cmd_flag_t)command,
                                        zero_filters);
    EXPECT_EQ(NULL, buffer);
}

TEST_F(ut_octl_query, create_buffer_from_filters_not_enough_filters)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_rm_cmd_flag_t command = ORCM_GET_DB_QUERY_HISTORY_COMMAND;

    rc = CreateBufferFromVaryingFilters(&buffer, filters_list,
                                        (orcm_rm_cmd_flag_t)command,
                                        one_filters);
    EXPECT_EQ(OPAL_ERR_BAD_PARAM, rc);
}


TEST_F(ut_octl_query, create_buffer_from_filters_empty_key)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    orcm_rm_cmd_flag_t command = ORCM_GET_DB_QUERY_HISTORY_COMMAND;

    rc = CreateBufferFromVaryingFilters(&buffer, filters_list,
                                        (orcm_rm_cmd_flag_t)command,
                                        two_filters_empty_key);

    EXPECT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, create_buffer_from_filters_empty_key_buffer_unharmed)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    orcm_rm_cmd_flag_t command = ORCM_GET_DB_QUERY_HISTORY_COMMAND;

    rc = CreateBufferFromVaryingFilters(&buffer, filters_list,
                                        (orcm_rm_cmd_flag_t)command,
                                        two_filters_empty_key);

    EXPECT_EQ(NULL, buffer);
}

TEST_F(ut_octl_query, create_buffer_from_filters_corrupted_value_type)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    orcm_rm_cmd_flag_t command = ORCM_GET_DB_QUERY_HISTORY_COMMAND;

    rc = CreateBufferFromVaryingFilters(&buffer, filters_list,
                                        (orcm_rm_cmd_flag_t)command,
                                        two_filters_corrupted_value_type);

    EXPECT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, create_buffer_from_filters_corrupted_value_type_unharmed_buffer)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    orcm_rm_cmd_flag_t command = ORCM_GET_DB_QUERY_HISTORY_COMMAND;

    rc = CreateBufferFromVaryingFilters(&buffer, filters_list,
                                        (orcm_rm_cmd_flag_t)command,
                                        two_filters_corrupted_value_type);

    EXPECT_EQ(NULL, buffer);
}

TEST_F(ut_octl_query, create_buffer_from_filters_valid_return_code)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    int unpacked_int;
    uint16_t unpacked_uint16;
    uint8_t unpacked_uint8;
    orcm_rm_cmd_flag_t command;
    int n=1;

    rc = CreateBufferFromVaryingFilters(&buffer, filters_list,
                                        (orcm_rm_cmd_flag_t)command,
                                        two_filters);

    EXPECT_EQ(0, rc);
}

TEST_F(ut_octl_query, create_buffer_from_filters_valid_command_packed)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_rm_cmd_flag_t command_to_send = ORCM_GET_DB_QUERY_HISTORY_COMMAND;
    orcm_rm_cmd_flag_t command_to_recv = ORCM_GET_DB_QUERY_HISTORY_COMMAND;
    int n=1;

    CreateBufferFromVaryingFilters(&buffer, filters_list, command_to_send,
                                   two_filters);
    opal_dss.unpack(buffer, &command_to_recv, &n, ORCM_RM_CMD_T);

    EXPECT_EQ(ORCM_GET_DB_QUERY_HISTORY_COMMAND, command_to_recv);
}

TEST_F(ut_octl_query, create_buffer_from_filters_valid_filter_list_size_packed)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    uint16_t unpacked_uint16;
    orcm_rm_cmd_flag_t command;
    int n=1;

    CreateBufferFromVaryingFilters(&buffer, filters_list, command,
                                   two_filters);
    opal_dss.unpack(buffer, &command, &n, ORCM_RM_CMD_T);
    opal_dss.unpack(buffer, &unpacked_uint16, &n, OPAL_UINT16);

    EXPECT_EQ(2, unpacked_uint16);
}

TEST_F(ut_octl_query, create_buffer_from_filters_first_filter_key)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_rm_cmd_flag_t command;
    uint16_t unpacked_uint16;
    char *unpacked_string;
    int n=1;

    CreateBufferFromVaryingFilters(&buffer, filters_list, command,
                                   two_filters);
    opal_dss.unpack(buffer, &command, &n, ORCM_RM_CMD_T);
    opal_dss.unpack(buffer, &unpacked_uint16, &n, OPAL_UINT16);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);

    EXPECT_STREQ("data_item", unpacked_string);
}

TEST_F(ut_octl_query, create_buffer_from_filters_first_filter_operation)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_rm_cmd_flag_t command;
    uint16_t unpacked_uint16;
    char *unpacked_string;
    uint8_t unpacked_uint8;
    int n=1;

    CreateBufferFromVaryingFilters(&buffer, filters_list, command,
                                   two_filters);
    opal_dss.unpack(buffer, &command, &n, ORCM_RM_CMD_T);
    opal_dss.unpack(buffer, &unpacked_uint16, &n, OPAL_UINT16);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);
    opal_dss.unpack(buffer, &unpacked_uint8, &n, OPAL_UINT8);

    EXPECT_EQ(CONTAINS, unpacked_uint8);
}

TEST_F(ut_octl_query, create_buffer_from_filters_first_filter_value)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_rm_cmd_flag_t command;
    uint16_t unpacked_uint16;
    char *unpacked_string;
    uint8_t unpacked_uint8;
    int n=1;

    CreateBufferFromVaryingFilters(&buffer, filters_list, command,
                                   two_filters);
    opal_dss.unpack(buffer, &command, &n, ORCM_RM_CMD_T);
    opal_dss.unpack(buffer, &unpacked_uint16, &n, OPAL_UINT16);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);
    opal_dss.unpack(buffer, &unpacked_uint8, &n, OPAL_UINT8);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);

    EXPECT_STREQ("%", unpacked_string);
}

TEST_F(ut_octl_query, create_buffer_from_filters_second_filter_key)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_rm_cmd_flag_t command;
    uint16_t unpacked_uint16;
    char *unpacked_string;
    uint8_t unpacked_uint8;
    int n=1;

    CreateBufferFromVaryingFilters(&buffer, filters_list, command,
                                   two_filters);
    opal_dss.unpack(buffer, &command, &n, ORCM_RM_CMD_T);
    opal_dss.unpack(buffer, &unpacked_uint16, &n, OPAL_UINT16);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);
    opal_dss.unpack(buffer, &unpacked_uint8, &n, OPAL_UINT8);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);

    EXPECT_STREQ("hostname", unpacked_string);

}

TEST_F(ut_octl_query, create_buffer_from_filters_second_filter_operation)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_rm_cmd_flag_t command;
    uint16_t unpacked_uint16;
    char *unpacked_string;
    uint8_t unpacked_uint8;
    int n=1;

    CreateBufferFromVaryingFilters(&buffer, filters_list, command,
                                   two_filters);
    opal_dss.unpack(buffer, &command, &n, ORCM_RM_CMD_T);
    opal_dss.unpack(buffer, &unpacked_uint16, &n, OPAL_UINT16);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);
    opal_dss.unpack(buffer, &unpacked_uint8, &n, OPAL_UINT8);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);
    opal_dss.unpack(buffer, &unpacked_uint8, &n, OPAL_UINT8);

    EXPECT_EQ(CONTAINS, unpacked_uint8);
}

TEST_F(ut_octl_query, create_buffer_from_filters_second_filter_value)
{
    int rc;
    opal_buffer_t *buffer = NULL;
    opal_list_t *filters_list = NULL;
    orcm_rm_cmd_flag_t command;
    uint16_t unpacked_uint16;
    char *unpacked_string;
    uint8_t unpacked_uint8;
    int n=1;

    CreateBufferFromVaryingFilters(&buffer, filters_list, command,
                                   two_filters);
    opal_dss.unpack(buffer, &command, &n, ORCM_RM_CMD_T);
    opal_dss.unpack(buffer, &unpacked_uint16, &n, OPAL_UINT16);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);
    opal_dss.unpack(buffer, &unpacked_uint8, &n, OPAL_UINT8);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);
    opal_dss.unpack(buffer, &unpacked_uint8, &n, OPAL_UINT8);
    opal_dss.unpack(buffer, &unpacked_string, &n, OPAL_STRING);

    EXPECT_STREQ("%", unpacked_string);
}

// print_results_from_stream function tests

TEST_F(ut_octl_query, print_results_from_stream_return_code)
{
    int rc;
    int results_count = 1;
    int stream_index = 1;
    double start_time = 0.0;
    double stop_time = 1.0;

    orte_rml.send_buffer_nb = MockedSendBufferFromPrint;
    rc = PrintResultsFromStream(results_count, stream_index, start_time,
                                stop_time);

    EXPECT_EQ(OPAL_SUCCESS, rc);
}

TEST_F(ut_octl_query, print_results_from_stream_no_receiver)
{
    int rc;
    int results_count = 2;
    int stream_index = 1980;
    double start_time = 0.0;
    double stop_time = 1.0;

    orte_rml.send_buffer_nb = MockedFailure1SendBuffer;
    rc = print_results_from_stream(results_count, stream_index, start_time,
                                   stop_time);
    orte_rml.send_buffer_nb = MockedSendBufferFromPrint;

    EXPECT_EQ(OPAL_ERR_BAD_PARAM, rc);
}

TEST_F(ut_octl_query, print_results_from_stream_corrupted_stream_index)
{
    int rc;
    int results_count = 2;
    int stream_index = 1980;
    double start_time = 0.0;
    double stop_time = 1.0;

    orte_rml.send_buffer_nb = MockedFailure2SendBuffer;
    rc = print_results_from_stream(results_count, stream_index, start_time,
                                   stop_time);
    orte_rml.send_buffer_nb = MockedSendBufferFromPrint;

    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER, rc);
}

TEST_F(ut_octl_query, print_results_from_stream_unexpected_stream_index)
{
    int rc;
    int results_count = 2;
    int stream_index = 1;
    double start_time = 0.0;
    double stop_time = 1.0;

    orte_rml.send_buffer_nb = MockedFailure5SendBuffer;
    rc = print_results_from_stream(results_count, stream_index, start_time,
                                   stop_time);

    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER, rc);
}

TEST_F(ut_octl_query, print_results_from_stream_results_greater_than_count)
{
    int rc;
    int results_count = 2;
    int stream_index = 1;
    double start_time = 0.0;
    double stop_time = 1.0;

    orte_rml.send_buffer_nb = MockedSendBufferFromPrint;
    rc = print_results_from_stream(results_count, stream_index, start_time,
                                   stop_time);

    EXPECT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_octl_query, print_results_from_stream_results_less_than_count)
{
    int rc;
    int results_count = 10002;
    int stream_index = 1;
    double start_time = 0.0;
    double stop_time = 1.0;

    orte_rml.send_buffer_nb = MockedSendBufferFromPrint;
    rc = print_results_from_stream(results_count, stream_index, start_time,
                                   stop_time);

    EXPECT_EQ(ORCM_SUCCESS, rc);
}
