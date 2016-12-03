/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#ifndef IPMIUTILAGENT_TESTS_H
#define IPMIUTILAGENT_TESTS_H

#include <set>
#include <string>

#include "gtest/gtest.h"
#include "orcm/mca/sensor/ipmi_ts/ipmiLibInterface.h"

#include "ipmiutilAgent_mocks.h"

class ipmiutilAgent_tests: public testing::Test
{
protected:
    buffer emptyBuffer;
    ipmiLibInterface *agent;
    std::string fileName;

    static const int MY_MODULE_PRIORITY = 20;
    const std::string PROBE_BMC;
    const std::set<std::string> getNodesInConfigFile();

    virtual void SetUp();
    virtual void TearDown();
    ipmiutilAgent_tests() : PROBE_BMC("cn01") {};

    void initParserFramework();
    void cleanParserFramework();
    void setFileName();

    bool isStringInTheSet(const std::string &name, const std::set<std::string> &list);
    void assertHostNameListIsConsistent(std::set<std::string> list);
};

class ipmiutilAgent_extra: public testing::Test
{
};


#endif //IPMIUTILAGENT_TESTS_H
