/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmi_collector_tests.h"
#include "orcm/mca/sensor/ipmi/ipmi_collector.h"

extern "C" {
    #include "orcm/util/utils.h"
}

TEST_F(ut_ipmi_collector, test_emptyConstructor)
{
    ipmiCollector c;
    ipmiCollector *cPtr = new ipmiCollector();
    ASSERT_TRUE(NULL != cPtr);
    EXPECT_STREQ("", c.getHostname().c_str());
    EXPECT_EQ(DEFAULT_AUTH_METHOD, cPtr->getAuthMethod());
    SAFEFREE(cPtr);
}

TEST_F(ut_ipmi_collector, test_constructor_allParams_NegativeValues)
{
    ipmiCollector c(Hostname,BmcAddress, Aggregator, User, Pass,
                    ORCM_ERROR, ORCM_ERROR, ORCM_ERROR, ORCM_ERROR);
    ipmiCollector *cPtr = new ipmiCollector(Hostname,BmcAddress, Aggregator, User, Pass,
                                            ORCM_ERROR, ORCM_ERROR, ORCM_ERROR, ORCM_ERROR);
    ASSERT_TRUE(NULL != cPtr);
    EXPECT_STREQ(Hostname.c_str(), c.getHostname().c_str());
    EXPECT_STREQ(Pass.c_str(), cPtr->getPass().c_str());
    EXPECT_EQ(DEFAULT_AUTH_METHOD, c.getAuthMethod());
    EXPECT_EQ(DEFAULT_PRIV_LEVEL, cPtr->getPrivLevel());
    SAFEFREE(cPtr);
}

TEST_F(ut_ipmi_collector, test_constructor_allParams)
{
    ipmiCollector c(Hostname,BmcAddress, Aggregator, User, Pass,
                    AuthMethod, PrivLevel, Port, Channel);
    ipmiCollector *cPtr = new ipmiCollector(Hostname,BmcAddress, Aggregator, User, Pass,
                                            AuthMethod, PrivLevel, Port, Channel);
    ASSERT_TRUE(NULL != cPtr);
    EXPECT_STREQ(Hostname.c_str(), c.getHostname().c_str());
    EXPECT_STREQ(Aggregator.c_str(), cPtr->getAggregator().c_str());
    EXPECT_EQ(AuthMethod, c.getAuthMethod());
    EXPECT_EQ(PrivLevel, cPtr->getPrivLevel());
    EXPECT_EQ(Port, c.getPort());
    EXPECT_EQ(Channel, cPtr->getChannel());
    SAFEFREE(cPtr);
}

TEST_F(ut_ipmi_collector, test_constructor_defaultValues)
{
    ipmiCollector c(Hostname,BmcAddress, Aggregator, User, Pass);
    ipmiCollector *cPtr = new ipmiCollector(Hostname,BmcAddress, Aggregator, User, Pass);
    ASSERT_TRUE(NULL != cPtr);
    EXPECT_STREQ(BmcAddress.c_str(), c.getBmcAddress().c_str());
    EXPECT_STREQ(User.c_str(), cPtr->getUser().c_str());
    EXPECT_EQ(DEFAULT_PORT, c.getPort());
    EXPECT_EQ(DEFAULT_CHANNEL, cPtr->getChannel());
    SAFEFREE(cPtr);
}

TEST_F(ut_ipmi_collector, test_setFunctions_returnError)
{
    ipmiCollector c;
    EXPECT_EQ(ORCM_ERROR, c.setAuthMethod(ORCM_ERROR));
    EXPECT_EQ(ORCM_ERROR, c.setPort(ORCM_ERROR));
    EXPECT_EQ(ORCM_ERROR, c.setPrivLevel(ORCM_ERROR));
    EXPECT_EQ(ORCM_ERROR, c.setChannel(ORCM_ERROR));
}

TEST_F(ut_ipmi_collector, test_setFunctions_returnSuccess)
{
    ipmiCollector c;
    EXPECT_EQ(ORCM_SUCCESS, c.setAuthMethod(AuthMethod));
    EXPECT_EQ(AuthMethod, c.getAuthMethod());
    EXPECT_EQ(ORCM_SUCCESS, c.setPort(Port));
    EXPECT_EQ(Port, c.getPort());
    EXPECT_EQ(ORCM_SUCCESS, c.setPrivLevel(PrivLevel));
    EXPECT_EQ(PrivLevel, c.getPrivLevel());
    EXPECT_EQ(ORCM_SUCCESS, c.setChannel(Channel));
    EXPECT_EQ(Channel, c.getChannel());
}
