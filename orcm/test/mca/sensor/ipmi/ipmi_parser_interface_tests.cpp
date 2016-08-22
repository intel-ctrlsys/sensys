/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmi_parser_interface_tests.h"
#include "orcm/mca/sensor/ipmi/ipmi_parser.h"
#include "orcm/mca/sensor/ipmi/ipmi_parser_interface.cpp"

#define AGG_NAME "aggregator01"
#define FAKEHOST "fakehost"
#define CN01 "cn01"

void init_parser_framework() 
{
    OBJ_CONSTRUCT(&orcm_parser_base_framework.framework_components, opal_list_t);
    OBJ_CONSTRUCT(&orcm_parser_base.actives, opal_list_t);
    orcm_parser_active_module_t *act_module;
    act_module = OBJ_NEW(orcm_parser_active_module_t);
    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_pugi_module;
    opal_list_append(&orcm_parser_base.actives, &act_module->super);
}

void clean_parser_framework()
{
    OPAL_LIST_DESTRUCT(&orcm_parser_base_framework.framework_components);
    OPAL_LIST_DESTRUCT(&orcm_parser_base.actives);
}

void ut_ipmi_parser_interface::setFileName()
{
    char* srcdir = getenv("srcdir");
    if (NULL != srcdir){
        fileName = string(srcdir) + "/ipmi.xml";
    } else {
        fileName = "ipmi.xml";
    }
}

void ut_ipmi_parser_interface::mockedConstructor()
{
    ipmiParser parser(fileName);
    ic_map = parser.getIpmiCollectorMap();
    ic_vector = parser.getIpmiCollectorVector();
    get_aggregators();
}

TEST(negative, default_initialization)
{
    orcm_cfgi_base.config_file = strdup("dummyFile");

    init_parser_framework();
    ASSERT_FALSE(load_ipmi_config_file());

    free(orcm_cfgi_base.config_file);
}

TEST_F(ut_ipmi_parser_interface, get_aggregator)
{
    char *aggName;
    ASSERT_TRUE(get_next_aggregator_name(&aggName));
    EXPECT_STREQ(aggName, AGG_NAME);
    ASSERT_FALSE(get_next_aggregator_name(&aggName));
    if (NULL != aggName) {
        free(aggName);
    }
}

TEST_F(ut_ipmi_parser_interface, get_bmc_info)
{
    ipmi_collector ic;
    EXPECT_FALSE(get_bmc_info(FAKEHOST, &ic));
    ASSERT_TRUE(get_bmc_info(CN01, &ic));

    EXPECT_STREQ("cn01", ic.hostname);
    EXPECT_STREQ("192.168.0.1", ic.bmc_address);
    EXPECT_STREQ("username", ic.user);
    EXPECT_STREQ("password", ic.pass);
    EXPECT_STREQ("aggregator01", ic.aggregator);
    EXPECT_EQ(MD5, ic.auth_method);
    EXPECT_EQ(OEM, ic.priv_level);
}

TEST_F(ut_ipmi_parser_interface, get_bmcs)
{
    int n = 0;
    ipmi_collector *ic = NULL;

    EXPECT_FALSE(get_bmcs_for_aggregator((char*) FAKEHOST, &ic, &n));
    EXPECT_TRUE( 0 == n );

    ASSERT_TRUE(get_bmcs_for_aggregator((char*) AGG_NAME, &ic, &n));
    EXPECT_TRUE( 6 == n);
}
