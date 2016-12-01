/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMIRESPONSEFACTORY_TESTS_H
#define IPMIRESPONSEFACTORY_TESTS_H

#include <ctime>

#include <map>

#include "gtest/gtest.h"

#include "orcm/mca/sensor/ipmi_ts/ipmiResponseFactory.hpp"

typedef map<uint8_t, string> PowerMap;

class IPMIResponseTests: public testing::Test
{
protected:
    PowerMap sys_pwr;
    PowerMap dev_pwr;

    virtual void SetUp() {
        sys_pwr[0x00] = "S0/G0";
        sys_pwr[0x01] = "S1";
        sys_pwr[0x02] = "S2";
        sys_pwr[0x03] = "S3";
        sys_pwr[0x04] = "S4";
        sys_pwr[0x05] = "S5/G2";
        sys_pwr[0x06] = "S4/S5";
        sys_pwr[0x07] = "G3";
        sys_pwr[0x08] = "sleeping";
        sys_pwr[0x09] = "G1 sleeping";
        sys_pwr[0x0A] = "S5 override";
        sys_pwr[0x20] = "Legacy On";
        sys_pwr[0x21] = "Legacy Off";
        sys_pwr[0x2A] = "Unknown";
        sys_pwr[0xFF] = "Illegal";

        dev_pwr[0x00] = "D0";
        dev_pwr[0x01] = "D1";
        dev_pwr[0x02] = "D2";
        dev_pwr[0x03] = "D3";
        dev_pwr[0x04] = "Unknown";
        dev_pwr[0xFF] = "Illegal";
    }

    virtual void TearDown() {
    }

    void test_wrong_size(MessageType msg);
};

#endif // IPMIRESPONSEFACTORY_TESTS_H
