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

#define UNKNOWN_KEY "UNKNOWN-KEY"

using namespace std;

void ut_util_tests::SetUpTestCase()
{
    opal_init_test();
    OBJ_CONSTRUCT(&orcm_parser_base_framework.framework_components, opal_list_t);
    OBJ_CONSTRUCT(&orcm_parser_base.actives, opal_list_t);
}

void ut_util_tests::SetUp()
{
}

void ut_util_tests::TearDownTestCase()
{
    OPAL_LIST_DESTRUCT(&orcm_parser_base_framework.framework_components);
    OPAL_LIST_DESTRUCT(&orcm_parser_base.actives);
}

//
// TESTS
//

TEST_F(ut_util_tests, logical_group_finalize)
{
    orcm_logical_group_load_to_memory("orcm-default-config.xml");
    EXPECT_TRUE(ORCM_SUCCESS == orcm_logical_group_finalize());
    EXPECT_TRUE(ORCM_SUCCESS == orcm_logical_group_finalize());
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

