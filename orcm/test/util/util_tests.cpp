/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "util_tests.h"

#include <iostream>
#include "orcm/util/utils.c"

#define UNKNOWN_KEY "UNKNOWN-KEY"

#define TESTING_STRING  "Hello \tworld,  happy "
#define HELLO_TAB_WORLD "Hello \tworld"
#define HAPPY           "happy"
#define __HAPPY_        "  happy "
#define HELLO           "Hello"
#define WORLD           "world,"
#define TAB_WORLD       "\tworld,"

using namespace std;

void free_charptr_array(char **ptr, int size)
{
    if (NULL != ptr) {
        for(int i = 0; i < size; i++) {
            SAFEFREE(ptr[i]);
        }
        SAFEFREE(ptr);
    }
}

int ut_util_tests::last_orte_error_ = ORCM_SUCCESS;

void ut_util_tests::SetUpTestCase()
{
    opal_init_test();
    OBJ_CONSTRUCT(&orcm_parser_base_framework.framework_components, opal_list_t);
    OBJ_CONSTRUCT(&orcm_parser_base.actives, opal_list_t);
}

void ut_util_tests::SetUp()
{
    last_orte_error_ = ORCM_SUCCESS;
    util_mocking.orte_errmgr_base_log_callback = OrteErrmgrBaseLog;
}

void ut_util_tests::TearDownTestCase()
{
    util_mocking.orte_errmgr_base_log_callback = NULL;
    OPAL_LIST_DESTRUCT(&orcm_parser_base_framework.framework_components);
    OPAL_LIST_DESTRUCT(&orcm_parser_base.actives);
}

void ut_util_tests::OrteErrmgrBaseLog(int err, char* file, int lineno)
{
    (void)file;
    (void)lineno;
    last_orte_error_ = err;
}

void ut_util_tests::on_failure_return(int *value)
{
    ORCM_ON_FAILURE_RETURN(*value);
    *value = ORCM_ERROR;
}

void ut_util_tests::on_null_return(void **value)
{
    ORCM_ON_NULL_RETURN(*value);
    *value = (void*) 0xdeadbeaf;
}

int ut_util_tests::on_null_return_error(void *ptr, int error)
{
    ORCM_ON_NULL_RETURN_ERROR(ptr, error);
    return ORCM_ERROR;
}

//
// TESTS
//

TEST_F(ut_util_tests, macro_SAFEFREE)
{
    char* zero = 0;
    char* null = NULL;
    char* str = strdup("hello world");

    SAFEFREE(zero);
    EXPECT_TRUE(NULL == zero);

    SAFEFREE(null);
    EXPECT_TRUE(NULL == null);

    SAFEFREE(str);
    EXPECT_TRUE(NULL == str);

}

TEST_F(ut_util_tests, macro_ORCM_RELEASE)
{
    orcm_value_t *value = OBJ_NEW(orcm_value_t);
    orcm_value_t *null = NULL;

    ORCM_RELEASE(null);
    EXPECT_TRUE(NULL == null);

    ORCM_RELEASE(value);
    EXPECT_TRUE(NULL == value);
}

TEST_F(ut_util_tests, macro_SAFE_RELEASE)
{
    opal_list_t *value = OBJ_NEW(opal_list_t);
    opal_list_t *null = NULL;

    SAFE_RELEASE(null);
    EXPECT_TRUE(NULL == null);

    SAFE_RELEASE(value);
    EXPECT_TRUE(NULL == value);
}

TEST_F(ut_util_tests, macro_CHECK_NULL_ALLOC)
{
    int error = ORCM_SUCCESS;
    const char *null = NULL, *str = "Hello world";

    CHECK_NULL_ALLOC(str, error, label1);

label1:
    EXPECT_EQ(ORCM_SUCCESS, error);
    CHECK_NULL_ALLOC(null, error, label2);
    FAIL();
label2:
    EXPECT_EQ(ORCM_ERR_OUT_OF_RESOURCE, error);
}

TEST_F(ut_util_tests, macro_ORCM_ON_NULL_BREAK)
{
    int i = 0;
    void *ptr = &i;
    for (; i < 5 ; i++){
        if (i == 3){
            ptr = NULL;
        }
        ORCM_ON_NULL_BREAK(ptr);
        EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);
    }
    EXPECT_EQ(3, i);
    EXPECT_TRUE(NULL == ptr);
    EXPECT_EQ(ORCM_ERR_OUT_OF_RESOURCE, last_orte_error_);
}

TEST_F(ut_util_tests, macro_ORCM_ON_NULL_GOTO)
{
    void *ptr = (void*) ORCM_ERROR;
    ORCM_ON_NULL_GOTO(ptr, error);
    EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);

    ptr = NULL;
    ORCM_ON_NULL_GOTO(ptr, error);
    FAIL();

error:
    EXPECT_TRUE(NULL == ptr);
    EXPECT_EQ(ORCM_ERR_OUT_OF_RESOURCE, last_orte_error_);
}


TEST_F(ut_util_tests, macro_ORCM_ON_NULL_RETURN_ERROR)
{
    void *ptr = (void*) ORCM_ERROR;
    int error = on_null_return_error(ptr, ORCM_ERR_OUT_OF_RESOURCE);
    EXPECT_EQ(ORCM_ERROR, error);
    EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);

    ptr = NULL;
    error = on_null_return_error(ptr, ORCM_ERR_OUT_OF_RESOURCE);
    EXPECT_EQ(ORCM_ERR_OUT_OF_RESOURCE, error);
    EXPECT_EQ(ORCM_ERR_OUT_OF_RESOURCE, last_orte_error_);
}

TEST_F(ut_util_tests, macro_ORCM_ON_NULL_RETURN)
{
    void *value = (void*) ORCM_ERROR;
    on_null_return(&value);
    EXPECT_TRUE((void*) 0xdeadbeaf == value);
    EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);

    value = NULL;
    on_null_return(&value);
    EXPECT_TRUE(NULL == value);
    EXPECT_EQ(ORCM_ERR_OUT_OF_RESOURCE, last_orte_error_);
}

TEST_F(ut_util_tests, macro_ORCM_ON_FAILURE_RETURN)
{
    int value = ORCM_SUCCESS;
    on_failure_return(&value);
    EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);
    EXPECT_EQ(ORCM_ERROR, value);

    value = ORCM_ERR_OUT_OF_RESOURCE;
    on_failure_return(&value);
    EXPECT_EQ(ORCM_ERR_OUT_OF_RESOURCE, last_orte_error_);
    EXPECT_NE(ORCM_ERROR, value);
}

TEST_F(ut_util_tests, macro_ORCM_ON_FAILURE_GOTO)
{
    int value = ORCM_SUCCESS;
    ORCM_ON_FAILURE_GOTO(value, error);
    EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);

    value = ORCM_ERROR;
    ORCM_ON_FAILURE_GOTO(value, error);
    FAIL();

error:
    EXPECT_EQ(ORCM_ERROR, last_orte_error_);
    EXPECT_NE(ORCM_SUCCESS, value);
}

TEST_F(ut_util_tests, macro_ORCM_ON_FAILURE_BREAK)
{
    int i = ORCM_SUCCESS;
    for (; i < 5 ; i++){
        ORCM_ON_FAILURE_BREAK(i);
        EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);
    }
    EXPECT_EQ(1, last_orte_error_);
    EXPECT_EQ(1, i);
}

TEST_F(ut_util_tests, logical_group_finalize)
{
    char* file = strdup("orcm-default-config.xml");
    orcm_logical_group_load_to_memory(file);
    EXPECT_TRUE(ORCM_SUCCESS == orcm_logical_group_finalize());
    EXPECT_TRUE(ORCM_SUCCESS == orcm_logical_group_finalize());
    SAFEFREE(file);
}

TEST_F(ut_util_tests, orcm_attr_key_print_unknown_key)
{
    const char *str;
    str = orcm_attr_key_print(ORCM_ATTR_KEY_BASE);
    EXPECT_STREQ(UNKNOWN_KEY, str);
    str = orcm_attr_key_print(ORCM_ATTR_KEY_MAX - 1);
    EXPECT_STREQ(UNKNOWN_KEY, str);
    str = orcm_attr_key_print(ORCM_ATTR_KEY_MAX);
    EXPECT_STREQ(UNKNOWN_KEY, str);
}

TEST_F(ut_util_tests, orcm_attr_key_print_known_key)
{
    const char *str;
    str = orcm_attr_key_print(ORCM_PWRMGMT_POWER_MODE_KEY);
    EXPECT_STREQ("PWRMGMT_POWER_MODE", str);
    str = orcm_attr_key_print(ORCM_PWRMGMT_POWER_BUDGET_KEY);
    EXPECT_STREQ("PWRMGMT_POWER_BUDGET", str);
    str = orcm_attr_key_print(ORCM_PWRMGMT_POWER_WINDOW_KEY);
    EXPECT_STREQ("PWRMGMT_POWER_WINDOW", str);
    str = orcm_attr_key_print(ORCM_PWRMGMT_CAP_OVERAGE_LIMIT_KEY);
    EXPECT_STREQ("PWRMGMT_CAP_OVERAGE_LIMIT", str);
    str = orcm_attr_key_print(ORCM_PWRMGMT_CAP_UNDERAGE_LIMIT_KEY);
    EXPECT_STREQ("PWRMGMT_CAP_UNDERAGE_LIMIT", str);
    str = orcm_attr_key_print(ORCM_PWRMGMT_CAP_OVERAGE_TIME_LIMIT_KEY);
    EXPECT_STREQ("PWRMGMT_CAP_OVERAGE_TIME_LIMIT", str);
    str = orcm_attr_key_print(ORCM_PWRMGMT_CAP_UNDERAGE_TIME_LIMIT_KEY);
    EXPECT_STREQ("PWRMGMT_CAP_UNDERAGE_TIME_LIMIT", str);
    str = orcm_attr_key_print(ORCM_PWRMGMT_SUPPORTED_MODES_KEY);
    EXPECT_STREQ("PWRMGMT_SUPPORTED_MODES", str);
    str = orcm_attr_key_print(ORCM_PWRMGMT_SELECTED_COMPONENT_KEY);
    EXPECT_STREQ("PWRMGMT_SELECTED_COMPONENT", str);
    str = orcm_attr_key_print(ORCM_PWRMGMT_FREQ_STRICT_KEY);
    EXPECT_STREQ("PWRMGMT_FREQ_STRICT", str);
}

TEST_F(ut_util_tests, string_utils_splitString_defaults)
{
    string myString = TESTING_STRING; // "Hello \tworld,  happy "
    vector<string> myVector = splitString(myString);
    ASSERT_EQ(2, myVector.size());
    EXPECT_TRUE(HELLO_TAB_WORLD == myVector[0]);
    EXPECT_TRUE(HAPPY == myVector[1]);
}

TEST_F(ut_util_tests, string_utils_splitString_comma_notrim)
{
    string myString = TESTING_STRING; // "Hello \tworld,  happy "
    vector<string> myVector = splitString(myString, ",", false);
    ASSERT_EQ(2, myVector.size());
    EXPECT_TRUE(HELLO_TAB_WORLD == myVector[0]);
    EXPECT_TRUE(__HAPPY_ == myVector[1]);
}

TEST_F(ut_util_tests, string_utils_splitString_space_trim)
{
    string myString = TESTING_STRING; // "Hello \tworld,  happy "
    vector<string> myVector = splitString(myString, " ");
    ASSERT_EQ(5, myVector.size());
    EXPECT_TRUE(HELLO == myVector[0]);
    EXPECT_TRUE(WORLD == myVector[1]);
    EXPECT_TRUE("" == myVector[2]);
    EXPECT_TRUE(HAPPY == myVector[3]);
    EXPECT_TRUE("" == myVector[4]);
}

TEST_F(ut_util_tests, string_utils_splitString_space_notrim)
{
    string myString = TESTING_STRING; // "Hello \tworld,  happy "
    vector<string> myVector = splitString(myString, " ", false);
    ASSERT_EQ(5, myVector.size());
    EXPECT_TRUE(HELLO == myVector[0]);
    EXPECT_TRUE(TAB_WORLD == myVector[1]);
    EXPECT_TRUE("" == myVector[2]);
    EXPECT_TRUE(HAPPY == myVector[3]);
    EXPECT_TRUE("" == myVector[4]);
}


TEST_F(ut_util_tests, string_utils_unique_str_vector_join)
{
    vector<string> v1;
    vector<string> v2;

    v1.push_back(HELLO);
    v1.push_back(WORLD);
    v1.push_back(HAPPY);

    v2.push_back(__HAPPY_);
    v2.push_back(HELLO);
    v2.push_back(TAB_WORLD);
    v2.push_back(HAPPY);

    unique_str_vector_join(v1, v2);

    ASSERT_EQ(5, v1.size());
    EXPECT_TRUE(v1[0] == HELLO);
    EXPECT_TRUE(v1[1] == WORLD);
    EXPECT_TRUE(v1[2] == HAPPY);
    EXPECT_TRUE(v1[3] == __HAPPY_);
    EXPECT_TRUE(v1[4] == TAB_WORLD);

    ASSERT_EQ(4, v2.size());
    EXPECT_TRUE(v2[0] == __HAPPY_);
    EXPECT_TRUE(v2[1] == HELLO);
    EXPECT_TRUE(v2[2] == TAB_WORLD);
    EXPECT_TRUE(v2[3] == HAPPY);
}

TEST_F(ut_util_tests, string_utils_convertStringVectorToCharPtrArray_emptyVector)
{
    vector<string> v1;
    char **ptr = convertStringVectorToCharPtrArray(v1);
    EXPECT_TRUE(NULL == ptr);
    free_charptr_array(ptr, v1.size());
}

TEST_F(ut_util_tests, string_utils_convertStringVectorToCharPtrArray)
{
    vector<string> v1;
    v1.push_back(HELLO);
    v1.push_back(WORLD);
    char **ptr = convertStringVectorToCharPtrArray(v1);
    EXPECT_TRUE(NULL != ptr);
    EXPECT_TRUE(0 == strcmp(ptr[0], HELLO));
    EXPECT_TRUE(0 == strcmp(ptr[1], WORLD));
    free_charptr_array(ptr, v1.size());
}

TEST_F(ut_util_tests, check_numeric_tests)
{
    char* bad_string = (char*) "123 TEST STRING";
    char* good_string = (char*) "31415";
    EXPECT_EQ(ORCM_ERROR, check_numeric(NULL));
    EXPECT_EQ(ORCM_ERR_BAD_PARAM, check_numeric(bad_string));
    EXPECT_EQ(ORCM_SUCCESS, check_numeric(good_string));
}

TEST_F(ut_util_tests, orcm_util_get_time_in_sec_tests)
{
    char* bad_string = (char*) "123 TEST STRING";
    EXPECT_EQ(0, orcm_util_get_time_in_sec(NULL));
    EXPECT_EQ(0, orcm_util_get_time_in_sec(bad_string));
}

TEST_F(ut_util_tests, orcm_util_relase_nested_orcm_value_list_item_tests)
{
    orcm_value_t *null_ptr = NULL;

    orcm_util_release_nested_orcm_value_list_item(NULL);
    orcm_util_release_nested_orcm_value_list_item(&null_ptr);
}

TEST_F(ut_util_tests, orcm_util_get_number_orcm_value)
{
    char *units = strdup("");
    char *intkey = strdup("int");
    char *uintkey = strdup("uintkey");

    orcm_value_t* ptr = NULL;

    EXPECT_DOUBLE_EQ( (double) 0.0, orcm_util_get_number_orcm_value(NULL));

    /* INTs */
    int intValue = 3;
    int64_t int64Value = 3;

    ptr = orcm_util_load_orcm_value(intkey, &intValue, OPAL_INT, units);
    EXPECT_DOUBLE_EQ( (double) 3.0, orcm_util_get_number_orcm_value(ptr));
    SAFEFREE(ptr);

    ptr = orcm_util_load_orcm_value(intkey, &intValue, OPAL_INT8, units);
    EXPECT_DOUBLE_EQ( (double) 3.0, orcm_util_get_number_orcm_value(ptr));
    SAFEFREE(ptr);

    ptr = orcm_util_load_orcm_value(intkey, &intValue, OPAL_INT16, units);
    EXPECT_DOUBLE_EQ( (double) 3.0, orcm_util_get_number_orcm_value(ptr));
    SAFEFREE(ptr);

    ptr = orcm_util_load_orcm_value(intkey, &intValue, OPAL_INT32, units);
    EXPECT_DOUBLE_EQ( (double) 3.0, orcm_util_get_number_orcm_value(ptr));
    SAFEFREE(ptr);

    ptr = orcm_util_load_orcm_value(intkey, &int64Value, OPAL_INT64, units);
    EXPECT_DOUBLE_EQ( (double) 3.0, orcm_util_get_number_orcm_value(ptr));
    SAFEFREE(ptr);

    /* UINTs */
    uint uintValue = 3;

    ptr = orcm_util_load_orcm_value(uintkey, &intValue, OPAL_UINT, units);
    EXPECT_DOUBLE_EQ( (double) 3.0, orcm_util_get_number_orcm_value(ptr));
    SAFEFREE(ptr);

    ptr = orcm_util_load_orcm_value(uintkey, &intValue, OPAL_UINT8, units);
    EXPECT_DOUBLE_EQ( (double) 3.0, orcm_util_get_number_orcm_value(ptr));
    SAFEFREE(ptr);

    ptr = orcm_util_load_orcm_value(uintkey, &intValue, OPAL_UINT16, units);
    EXPECT_DOUBLE_EQ( (double) 3.0, orcm_util_get_number_orcm_value(ptr));
    SAFEFREE(ptr);

    ptr = orcm_util_load_orcm_value(uintkey, &intValue, OPAL_UINT32, units);
    EXPECT_DOUBLE_EQ( (double) 3.0, orcm_util_get_number_orcm_value(ptr));
    SAFEFREE(ptr);

    ptr = orcm_util_load_orcm_value(uintkey, &int64Value, OPAL_UINT64, units);
    EXPECT_DOUBLE_EQ( (double) 3.0, orcm_util_get_number_orcm_value(ptr));
    SAFEFREE(ptr);

    SAFEFREE(units);
    SAFEFREE(intkey);
    SAFEFREE(uintkey);
}

TEST_F(ut_util_tests, orcm_util_copy_opal_value_data_tests)
{
    char *key = strdup("key");
    opal_value_t* src = NULL;
    opal_value_t* dst;

    int intval = 3;
    unsigned int uintval = 3;
    double val = 3.14159265;
    bool flag = false;
    uint8_t bobj[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    struct timeval now;
    gettimeofday(&now, NULL);

    src = OBJ_NEW(opal_value_t);

    src = orcm_util_load_opal_value(key, &val, OPAL_DOUBLE);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(NULL, src));

    src = orcm_util_load_opal_value(key, key, OPAL_STRING);
    EXPECT_EQ(ORCM_ERR_COPY_FAILURE, orcm_util_copy_opal_value_data(NULL, src));

    src = orcm_util_load_opal_value(key, bobj, OPAL_BYTE_OBJECT);
    EXPECT_EQ(ORCM_ERR_COPY_FAILURE, orcm_util_copy_opal_value_data(NULL, src));

    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &flag, OPAL_BOOL);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.flag, src->data.flag);
    OBJ_RELEASE(dst);

    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &bobj[0], OPAL_BYTE);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.byte, src->data.byte);
    OBJ_RELEASE(dst);

    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, key, OPAL_STRING);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_STREQ(dst->data.string, src->data.string);

    SAFEFREE(src->data.string);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_TRUE(NULL == dst->data.string);
    OBJ_RELEASE(dst);

    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &intval, OPAL_SIZE);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &intval, OPAL_PID);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &intval, OPAL_INT);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &intval, OPAL_INT8);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &intval, OPAL_INT16);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &intval, OPAL_INT32);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &intval, OPAL_INT64);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &uintval, OPAL_UINT);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &uintval, OPAL_UINT8);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &uintval, OPAL_UINT16);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &uintval, OPAL_UINT32);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &uintval, OPAL_UINT64);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, &now, OPAL_TIMEVAL);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = orcm_util_load_opal_value(key, bobj, OPAL_BYTE_OBJECT);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src)); // Freeing dst

    src->data.bo.size = 0;
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_TRUE( NULL == dst->data.bo.bytes && 0 == dst->data.bo.bytes );

    SAFEFREE(src->data.bo.bytes);
    EXPECT_EQ(ORCM_SUCCESS, orcm_util_copy_opal_value_data(dst, src));
    EXPECT_TRUE( NULL == dst->data.bo.bytes && 0 == dst->data.bo.bytes );

    EXPECT_EQ(dst->data.size, src->data.size);
    OBJ_RELEASE(dst);

    OBJ_RELEASE(src);
    dst = OBJ_NEW(opal_value_t);
    src = OBJ_NEW(opal_value_t);
    src->type = OPAL_NULL;
    EXPECT_EQ(OPAL_ERR_NOT_SUPPORTED, orcm_util_copy_opal_value_data(dst, src));
    OBJ_RELEASE(dst);
}
