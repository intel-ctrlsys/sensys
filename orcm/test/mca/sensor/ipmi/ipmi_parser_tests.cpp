/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmi_parser_tests.h"
#include "orcm/mca/sensor/ipmi/ipmi_parser.h"

#define verifyNode_AllParamsProvided(c) \
    EXPECT_STREQ("cn01", c.getHostname().c_str());           \
    EXPECT_STREQ("192.168.0.1", c.getBmcAddress().c_str());  \
    EXPECT_STREQ("username", c.getUser().c_str());           \
    EXPECT_STREQ("password", c.getPass().c_str());           \
    EXPECT_STREQ("aggregator01", c.getAggregator().c_str()); \
    EXPECT_EQ(MD5, c.getAuthMethod());                       \
    EXPECT_EQ(OEM, c.getPrivLevel());                        \
    EXPECT_EQ(1523, c.getPort());                            \
    EXPECT_EQ(2, c.getChannel());

#define verifyNode_DEFAULT_AUTH_METHOD(c) \
    EXPECT_STREQ("cn02", c.getHostname().c_str());           \
    EXPECT_STREQ("192.168.0.1", c.getBmcAddress().c_str());  \
    EXPECT_STREQ("username", c.getUser().c_str());           \
    EXPECT_STREQ("password", c.getPass().c_str());           \
    EXPECT_STREQ("aggregator01", c.getAggregator().c_str()); \
    EXPECT_EQ(DEFAULT_AUTH_METHOD, c.getAuthMethod());       \
    EXPECT_EQ(OEM, c.getPrivLevel());                        \
    EXPECT_EQ(1523, c.getPort());                            \
    EXPECT_EQ(2, c.getChannel());

#define verifyNode_DEFAULT_PRIV_LEVEL(c) \
    EXPECT_STREQ("cn03", c.getHostname().c_str());           \
    EXPECT_STREQ("192.168.0.1", c.getBmcAddress().c_str());  \
    EXPECT_STREQ("username", c.getUser().c_str());           \
    EXPECT_STREQ("password", c.getPass().c_str());           \
    EXPECT_STREQ("aggregator01", c.getAggregator().c_str()); \
    EXPECT_EQ(DEFAULT_PRIV_LEVEL, c.getPrivLevel());         \
    EXPECT_EQ(MD5, c.getAuthMethod());                       \
    EXPECT_EQ(1523, c.getPort());                            \
    EXPECT_EQ(2, c.getChannel());

#define verifyNode_DEFAULT_PORT(c) \
    EXPECT_STREQ("cn04", c.getHostname().c_str());           \
    EXPECT_STREQ("192.168.0.1", c.getBmcAddress().c_str());  \
    EXPECT_STREQ("username", c.getUser().c_str());           \
    EXPECT_STREQ("password", c.getPass().c_str());           \
    EXPECT_STREQ("aggregator01", c.getAggregator().c_str()); \
    EXPECT_EQ(MD5, c.getAuthMethod());                       \
    EXPECT_EQ(OEM, c.getPrivLevel());                        \
    EXPECT_EQ(DEFAULT_PORT, c.getPort());                    \
    EXPECT_EQ(2, c.getChannel());

#define verifyNode_DEFAULT_CHANNEL(c) \
    EXPECT_STREQ("cn05", c.getHostname().c_str());           \
    EXPECT_STREQ("192.168.0.1", c.getBmcAddress().c_str());  \
    EXPECT_STREQ("username", c.getUser().c_str());           \
    EXPECT_STREQ("password", c.getPass().c_str());           \
    EXPECT_STREQ("aggregator01", c.getAggregator().c_str()); \
    EXPECT_EQ(MD5, c.getAuthMethod());                       \
    EXPECT_EQ(OEM, c.getPrivLevel());                        \
    EXPECT_EQ(1523, c.getPort());                            \
    EXPECT_EQ(DEFAULT_CHANNEL, c.getChannel());

#define verifyNode_AllDefaults(c) \
    EXPECT_STREQ("cn06", c.getHostname().c_str());           \
    EXPECT_STREQ("192.168.0.1", c.getBmcAddress().c_str());  \
    EXPECT_STREQ("username", c.getUser().c_str());           \
    EXPECT_STREQ("password", c.getPass().c_str());           \
    EXPECT_STREQ("aggregator01", c.getAggregator().c_str()); \
    EXPECT_EQ(DEFAULT_AUTH_METHOD, c.getAuthMethod());       \
    EXPECT_EQ(DEFAULT_PRIV_LEVEL, c.getPrivLevel());         \
    EXPECT_EQ(DEFAULT_PORT, c.getPort());                    \
    EXPECT_EQ(DEFAULT_CHANNEL, c.getChannel());


void ut_ipmi_parser::initParserFramework()
{
    OBJ_CONSTRUCT(&orcm_parser_base_framework.framework_components, opal_list_t);
    OBJ_CONSTRUCT(&orcm_parser_base.actives, opal_list_t);
    orcm_parser_active_module_t *act_module;
    act_module = OBJ_NEW(orcm_parser_active_module_t);
    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_pugi_module;
    opal_list_append(&orcm_parser_base.actives, &act_module->super);
}

void ut_ipmi_parser::cleanParserFramework()
{
    OPAL_LIST_DESTRUCT(&orcm_parser_base_framework.framework_components);
    OPAL_LIST_DESTRUCT(&orcm_parser_base.actives);
}

void ut_ipmi_parser::setFileName()
{
    char* srcdir = getenv("srcdir");
    if (NULL != srcdir){
        fileName = string(srcdir) + "/ipmi.xml";
    } else {
        fileName = "ipmi.xml";
    }
}

TEST_F(ut_ipmi_parser, test_emptyConstructor_compatible)
{
    orcm_cfgi_base.config_file = strdup(fileName.c_str());
    float version_bkp = orcm_cfgi_base.version;
    orcm_cfgi_base.version = 3.1;

    ipmiParser p;
    ipmiParser *pPtr = new ipmiParser();
    orcm_cfgi_base.version = version_bkp;
    free(orcm_cfgi_base.config_file);
    ASSERT_TRUE(NULL != pPtr);
    delete pPtr;
}

TEST_F(ut_ipmi_parser, test_emptyConstructor)
{
    orcm_cfgi_base.config_file = strdup(fileName.c_str());

    ipmiParser p;
    ipmiParser *pPtr = new ipmiParser();
    ASSERT_TRUE(NULL != pPtr);
    delete pPtr;

    free(orcm_cfgi_base.config_file);
}

TEST_F(ut_ipmi_parser, test_constructor_validFile)
{
    ipmiParser p(fileName);
    ipmiParser *pPtr = new ipmiParser(fileName);
    ASSERT_TRUE(NULL != pPtr);
    delete pPtr;
}

TEST_F(ut_ipmi_parser, test_constructor_invalidFile)
{
    ipmiParser p(invalidFile);
    ipmiParser *pPtr = new ipmiParser(invalidFile);
    ASSERT_TRUE(NULL != pPtr);
    delete pPtr;
}

TEST_F(ut_ipmi_parser, test_parse_invalidFile)
{
    ipmiParser p(invalidFile);
    ipmiCollectorVector ipmiVector = p.getIpmiCollectorVector();
    ASSERT_EQ(0, ipmiVector.size());
}

TEST_F(ut_ipmi_parser, test_parse_validFile)
{
    ipmiParser p(fileName);
    ipmiCollectorVector ipmiVector = p.getIpmiCollectorVector();
    ASSERT_EQ(NUM_NODES, ipmiVector.size());
    verifyNode_AllParamsProvided(ipmiVector[0]);
    verifyNode_DEFAULT_AUTH_METHOD(ipmiVector[1]);
    verifyNode_DEFAULT_PRIV_LEVEL(ipmiVector[2]);
    verifyNode_DEFAULT_PORT(ipmiVector[3]);
    verifyNode_DEFAULT_CHANNEL(ipmiVector[4]);
    verifyNode_AllDefaults(ipmiVector[5]);
}

TEST_F(ut_ipmi_parser, test_parse_collectorMap)
{
    ipmiParser p(fileName);
    ipmiCollectorMap ipmiMap = p.getIpmiCollectorMap();
    ASSERT_EQ(NUM_NODES, ipmiMap.size());
    verifyNode_AllParamsProvided(ipmiMap["cn01"]);
    verifyNode_DEFAULT_AUTH_METHOD(ipmiMap["cn02"]);
    verifyNode_DEFAULT_PRIV_LEVEL(ipmiMap["cn03"]);
    verifyNode_DEFAULT_PORT(ipmiMap["cn04"]);
    verifyNode_DEFAULT_CHANNEL(ipmiMap["cn05"]);
    verifyNode_AllDefaults(ipmiMap["cn06"]);
}
