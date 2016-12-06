/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <vector>
#include <sstream>

#include "orcm/common/dataContainer.hpp"
#include "orcm/util/string_utils.h"

#include "ipmi_sensor_tests.hpp"
#include "timeoutHelper.hpp"

#define MAX_SENSOR_LIST 17
#define MAX_SENSOR_READINGS 12

using namespace std;

bool callback_flag = false;

template <typename T>
void assert_reading_item(dataContainer* dc,
    string key, T val, string units)
{
    cout << "[" << key << "] -> " << val << endl;
    ASSERT_TRUE(dc->getValue<T>(key) == val);
    ASSERT_FALSE(dc->getUnits(key).compare(units));
}

void assert_inventory_item(dataContainer* dc,
    string key, string val, string units)
{
    cout << "[" << key << "] -> " << val << endl;
    ASSERT_FALSE(dc->getValue<string>(key).compare(val));
    ASSERT_FALSE(dc->getUnits(key).compare(units));
}

void inventory_callback(std::string hostname, dataContainer* dc)
{
    ASSERT_EQ(MAX_SENSOR_LIST, dc->count());
    assert_inventory_item(dc, "sensor_ipmi_ts_1", "doubleValue", "");
    assert_inventory_item(dc, "sensor_ipmi_ts_2", "intValue", "");
    assert_inventory_item(dc, "sensor_ipmi_ts_3", "boolValue", "");
    assert_inventory_item(dc, "sensor_ipmi_ts_4", "int8Value", "");
    assert_inventory_item(dc, "sensor_ipmi_ts_5", "int16Value", "");
    assert_inventory_item(dc, "sensor_ipmi_ts_6", "int32Value", "");
    assert_inventory_item(dc, "sensor_ipmi_ts_7", "int64Value", "");
    assert_inventory_item(dc, "sensor_ipmi_ts_8", "uint8Value", "");
    assert_inventory_item(dc, "sensor_ipmi_ts_9", "uint16Value", "");
    assert_inventory_item(dc, "sensor_ipmi_ts_10", "uint32Value", "");
    assert_inventory_item(dc, "sensor_ipmi_ts_11", "uint64Value", "");
    assert_inventory_item(dc, "sensor_ipmi_ts_12", "floatValue", "");
    assert_inventory_item(dc, "bmc_ver", "1.22", "");
    assert_inventory_item(dc, "ipmi_ver", "2", "");
    assert_inventory_item(dc, "bb_serial", "QSIP23300384", "");
    assert_inventory_item(dc, "bb_vendor", "Intel Corporation", "");
    assert_inventory_item(dc, "bb_manufactured_date", "08/18/12", "");
    callback_flag = true;
}

void sample_callback(std::string hostname, dataContainer* dc)
{
    ASSERT_EQ(MAX_SENSOR_READINGS, dc->count());
    assert_reading_item(dc, "doubleValue", (double) 3.14159265, "doubles");
    assert_reading_item(dc, "intValue", (int) 5, "units-ints");
    assert_reading_item(dc, "boolValue", (bool) true, "booleans");
    assert_reading_item(dc, "int8Value", (int8_t) -8, "int8s");
    assert_reading_item(dc, "int16Value", (int16_t) -16, "int16s");
    assert_reading_item(dc, "int32Value", (int32_t) -32, "int32s");
    assert_reading_item(dc, "int64Value", (int64_t) -64, "int64s");
    assert_reading_item(dc, "uint8Value", (uint8_t) 8, "uint8s");
    assert_reading_item(dc, "uint16Value", (uint16_t) 16, "uint16s");
    assert_reading_item(dc, "uint32Value", (uint32_t) 32, "uint32s");
    assert_reading_item(dc, "uint64Value", (uint64_t) 64, "uint64s");
    assert_reading_item(dc, "floatValue", (float) 3.14, "floats");
    callback_flag = true;
}

TEST_F(ipmiSensor_Tests, collect_inventory_ipmi)
{
    dataContainer dc;
    ipmiSensor sensor("test");

    callback_flag = false;

    sensor.setInventoryPtr(inventory_callback);
    sensor.init();
    sensor.collect_inventory();
    ASSERT_TRUE_TIMEOUT(callback_flag, TIMEOUT);
    sensor.finalize();

    callback_flag = false;
}

TEST_F(ipmiSensor_Tests, ipmi_sample)
{
    dataContainer dc;
    ipmiSensor sensor("test");

    callback_flag = false;

    sensor.setSamplingPtr(sample_callback);
    sensor.init();
    sensor.sample();
    ASSERT_TRUE_TIMEOUT(callback_flag, TIMEOUT);
    sensor.finalize();

    callback_flag = false;
}
