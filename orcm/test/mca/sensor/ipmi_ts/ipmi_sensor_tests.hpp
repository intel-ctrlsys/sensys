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

#include "orcm/mca/sensor/ipmi_ts/ipmiSensorFactory.hpp"

#include "ipmi_sensor_mocks.h"

extern "C" {
    #include "opal/runtime/opal.h"
    #include "orte/runtime/orte_globals.h"
};

extern bool callback_flag;
extern bool sensor_list_flag;
extern bool device_id_flag;
extern bool fru_data_flag;
extern bool acpi_power_flag;
extern bool sel_records_flag;
extern bool sensor_readings_flag;

class ipmiSensorFactory_Tests: public testing::Test
{
protected:
    virtual void SetUp() {
        opal_init_test();
        static const char MCA_DFX_FLAG[] = "ORCM_MCA_sensor_ipmi_ts_dfx";
        setenv(MCA_DFX_FLAG, "1", 1);

        mocks[PLUGIN_FINALIZE].restartMock();
        mocks[PLUGIN_INIT].restartMock();
        mocks[PLUGIN_TEST_FINALIZE].restartMock();
        mocks[PLUGIN_TEST_INIT].restartMock();
    }

    virtual void TearDown() {
    }
};

class ipmiSensor_Tests: public testing::Test
{
protected:
    virtual void SetUp() {
        opal_init_test();
        static const char MCA_DFX_FLAG[] = "ORCM_MCA_sensor_ipmi_ts_dfx";
        setenv(MCA_DFX_FLAG, "1", 1);

        callback_flag = false;
        sensor_list_flag = false;
        device_id_flag = false;
        fru_data_flag = false;
        acpi_power_flag = false;
        sel_records_flag = false;
        sensor_readings_flag = false;
        mocks[PLUGIN_INIT].restartMock();
    }

    virtual void TearDown() {
        callback_flag = false;
        sensor_list_flag = false;
        device_id_flag = false;
        fru_data_flag = false;
        acpi_power_flag = false;
        sel_records_flag = false;
        sensor_readings_flag = false;
    }
};

#endif // IPMISENSOR_TESTS_H
