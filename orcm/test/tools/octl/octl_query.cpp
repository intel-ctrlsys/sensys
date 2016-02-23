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

// C++
#include <iostream>
#include <iomanip>

// Fixture
using namespace std;

extern "C" {
    int orcm_octl_query_event_data(int cmd, char **argv);
    opal_list_t *build_filters_list(int cmd, char **argv);
    opal_list_t *create_query_event_data_filter(int argc, char **argv);
    opal_list_t *create_query_event_snsr_data_filter(int argc, char **argv);
    opal_list_t *create_query_event_date_filter(int argc, char **argv);
    int split_db_results(char *db_res, char ***db_results);
    char *add_to_str_date(char *date, int seconds);
    size_t opal_list_get_size(opal_list_t *list);
    char *assemble_datetime(char *date_str, char *time_str);
    bool replace_wildcard(char **filter_me, bool quit_on_first);
};

void ut_octl_query::SetUpTestCase()
{
    return;
}

void ut_octl_query::TearDownTestCase()
{
    return;
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
