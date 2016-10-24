/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "db_base_test.h"
#include "opal/dss/dss_types.h"
#include "opal/class/opal_list.h"
#include <sys/time.h>

using namespace std;

extern "C" {
    #include "orcm/mca/db/base/base.h"
    bool time_stamp_str_to_tv(char *stamp, struct timeval* time_value);
    bool is_supported_opal_int_type(opal_data_type_t type);
    char *build_query_from_view_name_and_filters(const char* view_name, opal_list_t* filter);
    char *get_opal_value_as_sql_string(opal_value_t *value);
    int opal_value_to_orcm_db_item(const opal_value_t *kv, orcm_db_item_t *item);
}

void ut_db_base_utils_tests::SetUpTestCase()
{
    return;
}

void ut_db_base_utils_tests::TearDownTestCase()
{
    return;
}

TEST_F(ut_db_base_utils_tests, time_stamp_str_to_tv_not_null)
{
    const char *str = "2016-19-05 10:00:00.123456";
    bool res;
    struct timeval time_value;
    res = time_stamp_str_to_tv((char*)str, &time_value);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, time_stamp_str_to_tv_stamp_null)
{
    bool res;
    struct timeval time_value;
    res = time_stamp_str_to_tv(NULL, &time_value);
    EXPECT_EQ(res, false);
}

TEST_F(ut_db_base_utils_tests, time_stamp_str_to_tv_timeval_null)
{
    const char *str = "2016-19-05 10:00:00.123456";
    bool res;
    res = time_stamp_str_to_tv((char*)str, NULL);
    EXPECT_EQ(res, false);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_byte)
{
    bool res;
    opal_data_type_t type = OPAL_BYTE;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_bool)
{
    bool res;
    opal_data_type_t type = OPAL_BOOL;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_size)
{
    bool res;
    opal_data_type_t type = OPAL_SIZE;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_pid)
{
    bool res;
    opal_data_type_t type = OPAL_PID;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_int)
{
    bool res;
    opal_data_type_t type = OPAL_INT;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_int8)
{
    bool res;
    opal_data_type_t type = OPAL_INT8;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_int16)
{
    bool res;
    opal_data_type_t type = OPAL_INT16;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_int32)
{
    bool res;
    opal_data_type_t type = OPAL_INT32;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_int64)
{
    bool res;
    opal_data_type_t type = OPAL_INT64;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_uint)
{
    bool res;
    opal_data_type_t type = OPAL_UINT;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_uint8)
{
    bool res;
    opal_data_type_t type = OPAL_UINT8;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_uint16)
{
    bool res;
    opal_data_type_t type = OPAL_UINT16;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_uint32)
{
    bool res;
    opal_data_type_t type = OPAL_UINT32;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, is_supported_type_opal_uint64)
{
    bool res;
    opal_data_type_t type = OPAL_UINT64;
    res = is_supported_opal_int_type(type);
    EXPECT_EQ(res, true);
}

TEST_F(ut_db_base_utils_tests, build_query_null_null)
{
    char *res = NULL;
    res = build_query_from_view_name_and_filters(NULL, NULL);
    ASSERT_TRUE(res == NULL);
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_null)
{
    char *res = NULL;
    res = get_opal_value_as_sql_string(NULL);
    ASSERT_TRUE(res == NULL);
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_int)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_INT;
    test_value.data.integer = 0;
    res = get_opal_value_as_sql_string(&test_value);
    EXPECT_EQ(0, strcmp(res, "0"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_int8)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_INT8;
    test_value.data.int8 = 0;
    res = get_opal_value_as_sql_string(&test_value);
    EXPECT_EQ(0, strcmp(res, "0"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_int16)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_INT16;
    test_value.data.int16 = 0;
    res = get_opal_value_as_sql_string(&test_value);
    EXPECT_EQ(0, strcmp(res, "0"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_int32)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_INT32;
    test_value.data.int32 = 0;
    res = get_opal_value_as_sql_string(&test_value);
    EXPECT_EQ(0, strcmp(res, "0"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_int64)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_INT64;
    test_value.data.int64 = 0;
    res = get_opal_value_as_sql_string(&test_value);
    EXPECT_EQ(0, strcmp(res, "0"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_uint)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_UINT;
    test_value.data.uint = 0;
    res = get_opal_value_as_sql_string(&test_value);
    EXPECT_EQ(0, strcmp(res, "0"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_uint8)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_UINT8;
    test_value.data.uint8 = 0;
    res = get_opal_value_as_sql_string(&test_value);
    EXPECT_EQ(0, strcmp(res, "0"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_uint16)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_UINT16;
    test_value.data.uint16 = 0;
    res = get_opal_value_as_sql_string(&test_value);
    EXPECT_EQ(0, strcmp(res, "0"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_uint32)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_UINT32;
    test_value.data.uint32 = 0;
    res = get_opal_value_as_sql_string(&test_value);
    EXPECT_EQ(0, strcmp(res, "0"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_uint64)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_UINT64;
    test_value.data.uint64 = 0;
    res = get_opal_value_as_sql_string(&test_value);
    EXPECT_EQ(0, strcmp(res, "0"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_float)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_FLOAT;
    test_value.data.fval = 0.0;
    res = get_opal_value_as_sql_string(&test_value);
    ASSERT_TRUE(NULL != strstr(res, "0.0"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_double)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_DOUBLE;
    test_value.data.dval = 0.0;
    res = get_opal_value_as_sql_string(&test_value);
    ASSERT_TRUE(NULL != strstr(res, "0.0"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_timeval)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_TIMEVAL;
    test_value.data.tv.tv_sec = 0;
    test_value.data.tv.tv_usec = 0;
    res = get_opal_value_as_sql_string(&test_value);
    ASSERT_TRUE(NULL != strstr(res, "00:00:00.000"));
}

TEST_F(ut_db_base_utils_tests, get_opal_value_as_sql_time)
{
    char *res = NULL;
    opal_value_t test_value;
    test_value.type = OPAL_TIME;
    test_value.data.tv.tv_sec = 0;
    test_value.data.tv.tv_usec = 0;
    res = get_opal_value_as_sql_string(&test_value);
    ASSERT_TRUE(NULL != strstr(res, "00:00:00.000"));
}

TEST_F(ut_db_base_utils_tests, opal_value_to_orcm_db_uint8)
{
    int res;
    opal_value_t test_value;
    orcm_db_item_t res_value;
    test_value.type = OPAL_UINT8;
    test_value.data.uint8 = 0;
    res = opal_value_to_orcm_db_item(&test_value, &res_value);
    EXPECT_EQ(0, res);
    EXPECT_EQ(test_value.data.uint8, res_value.value.value_int);
}

TEST_F(ut_db_base_utils_tests, opal_value_to_orcm_db_uint16)
{
    int res;
    opal_value_t test_value;
    orcm_db_item_t res_value;
    test_value.type = OPAL_UINT16;
    test_value.data.uint16 = 0;
    res = opal_value_to_orcm_db_item(&test_value, &res_value);
    EXPECT_EQ(0, res);
    EXPECT_EQ(test_value.data.uint16, res_value.value.value_int);
}

TEST_F(ut_db_base_utils_tests, opal_value_to_orcm_db_uint32)
{
    int res;
    opal_value_t test_value;
    orcm_db_item_t res_value;
    test_value.type = OPAL_UINT32;
    test_value.data.uint32 = 0;
    res = opal_value_to_orcm_db_item(&test_value, &res_value);
    EXPECT_EQ(0, res);
    EXPECT_EQ(test_value.data.uint32, res_value.value.value_int);
}

TEST_F(ut_db_base_utils_tests, opal_value_to_orcm_db_uint64)
{
    int res;
    opal_value_t test_value;
    orcm_db_item_t res_value;
    test_value.type = OPAL_UINT64;
    test_value.data.uint64 = 0;
    res = opal_value_to_orcm_db_item(&test_value, &res_value);
    EXPECT_EQ(0, res);
    EXPECT_EQ(test_value.data.uint64, res_value.value.value_int);
}
