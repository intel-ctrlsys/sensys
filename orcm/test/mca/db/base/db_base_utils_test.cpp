/*
 * Copyright (c) 2016 Intel, Inc. All rights reserved.
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
    bool time_stamp_str_to_tv(char *stamp, struct timeval* time_value);
    bool is_supported_opal_int_type(opal_data_type_t type);
    char *build_query_from_view_name_and_filters(const char* view_name, opal_list_t* filter);
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
