/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMI_COLLECTOR_TESTS_H
#define IPMI_COLLECTOR_TESTS_H

#include "gtest/gtest.h"
#include <string>

using namespace std;

class ut_ipmi_collector: public testing::Test
{
    protected:
        virtual void SetUp()
        {
            Hostname="hostname";
            BmcAddress="bmc_address";
            Aggregator="aggregator";
            User = "user";
            Pass = "pass";
            AuthMethod = 5;
            PrivLevel = 3;
            Port = 1;
            Channel = 1;
        }
        virtual void TearDown() {};

        string Hostname, BmcAddress, Aggregator, User, Pass;
        int AuthMethod, PrivLevel, Port, Channel;

};

#endif // IPMI_COLLECTOR_TESTS_H
