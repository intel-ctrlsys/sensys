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

class ipmiutilAgent_tests: public testing::Test
{
protected:
    ipmiLibInterface *agent;
    std::string fileName;

    static const int MY_MODULE_PRIORITY = 20;
    const std::set<std::string> getNodesInConfigFile();

    virtual void SetUp();
    virtual void TearDown();

    void initParserFramework();
    void cleanParserFramework();
    void setFileName();

    bool isStringInTheSet(const std::string &name, const std::set<std::string> &list);
    void assertHostNameListIsConsistent(std::set<std::string> list);
};

#endif //IPMIUTILAGENT_TESTS_H
