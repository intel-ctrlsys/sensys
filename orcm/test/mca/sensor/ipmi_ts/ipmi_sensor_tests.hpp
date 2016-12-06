/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMISENSOR_TESTS_H
#define IPMISENSOR_TESTS_H

#include <ctime>

#include <map>

#include "gtest/gtest.h"

#include "orcm/mca/sensor/ipmi_ts/ipmi_sensor/ipmiSensor.hpp"

extern "C" {
    #include "opal/runtime/opal.h"
    #include "orte/runtime/orte_globals.h"
};

class ipmiSensor_Tests: public testing::Test
{
protected:
    virtual void SetUp() {
        opal_init_test();
        static const char MCA_DFX_FLAG[] = "ORCM_MCA_sensor_ipmi_ts_dfx";
        setenv(MCA_DFX_FLAG, "1", 1);
    }

    virtual void TearDown() {
    }
};

#endif // IPMISENSOR_TESTS_H
