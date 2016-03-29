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
#include "orcm/mca/sensor/ipmi/ipmi_collector.h"

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
            AuthMethod = AUTH_OEM;
            PrivLevel = OEM;
            Port = 2024;
            Channel = 1;
        }
        virtual void TearDown() {};

        string Hostname, BmcAddress, Aggregator, User, Pass;
        auth_methods AuthMethod;
        priv_levels PrivLevel;
        int Port, Channel;

};

#endif // IPMI_COLLECTOR_TESTS_H
