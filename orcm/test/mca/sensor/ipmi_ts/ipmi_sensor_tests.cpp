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

#define MAX_SENSOR_LIST 12
#define MAX_DEVICE_ID 1
#define MAX_ACPI_POWER 2
#define MAX_FRU_DATA 3
#define MAX_SEL_RECORDS 5
#define MAX_SENSOR_READINGS 12

using namespace std;

bool callback_flag = false;
bool sensor_list_flag = false;
bool device_id_flag = false;
bool fru_data_flag = false;
bool acpi_power_flag = false;
bool sel_records_flag = false;
bool sensor_readings_flag = false;

template <typename T>
void assert_numeric_item(dataContainer* dc,
    string key, T val, string units)
{
    cout << "[" << key << "] -> " << val << endl;
    ASSERT_TRUE(dc->getValue<T>(key) == val);
    ASSERT_FALSE(dc->getUnits(key).compare(units));
}

void assert_string_item(dataContainer* dc,
    string key, string val, string units)
{
    cout << "[" << key << "] -> " << val << endl;
    ASSERT_FALSE(dc->getValue<string>(key).compare(val));
    ASSERT_FALSE(dc->getUnits(key).compare(units));
}

void inventory_callback(std::string hostname, dataContainer* dc)
{
    if (MAX_SENSOR_LIST == dc->count())
    {
        assert_string_item(dc, "sensor_ipmi_ts_1", "doubleValue", "");
        assert_string_item(dc, "sensor_ipmi_ts_2", "intValue", "");
        assert_string_item(dc, "sensor_ipmi_ts_3", "boolValue", "");
        assert_string_item(dc, "sensor_ipmi_ts_4", "int8Value", "");
        assert_string_item(dc, "sensor_ipmi_ts_5", "int16Value", "");
        assert_string_item(dc, "sensor_ipmi_ts_6", "int32Value", "");
        assert_string_item(dc, "sensor_ipmi_ts_7", "int64Value", "");
        assert_string_item(dc, "sensor_ipmi_ts_8", "uint8Value", "");
        assert_string_item(dc, "sensor_ipmi_ts_9", "uint16Value", "");
        assert_string_item(dc, "sensor_ipmi_ts_10", "uint32Value", "");
        assert_string_item(dc, "sensor_ipmi_ts_11", "uint64Value", "");
        assert_string_item(dc, "sensor_ipmi_ts_12", "floatValue", "");
        sensor_list_flag = true;
    }
    else if (MAX_DEVICE_ID == dc->count())
    {
        assert_string_item(dc, "bmc_ver", "1.22", "");
        device_id_flag = true;
    }
    else if (MAX_FRU_DATA == dc->count())
    {
        assert_string_item(dc, "bb_serial", "QSIP23300384", "");
        assert_string_item(dc, "bb_vendor", "Intel Corporation", "");
        assert_string_item(dc, "bb_manufactured_date", "08/18/12", "");
        fru_data_flag = true;
    }
    else
    {
        cout << "INV SIZE" << dc->count() << endl;
        ASSERT_TRUE(false);
    }

    callback_flag = sensor_list_flag && device_id_flag && fru_data_flag;
}

void sample_callback(std::string hostname, dataContainer* dc)
{
    if (MAX_SENSOR_READINGS == dc->count())
    {
        assert_numeric_item(dc, "doubleValue", (double) 3.14159265, "doubles");
        assert_numeric_item(dc, "intValue", (int) 5, "units-ints");
        assert_numeric_item(dc, "boolValue", (bool) true, "booleans");
        assert_numeric_item(dc, "int8Value", (int8_t) -8, "int8s");
        assert_numeric_item(dc, "int16Value", (int16_t) -16, "int16s");
        assert_numeric_item(dc, "int32Value", (int32_t) -32, "int32s");
        assert_numeric_item(dc, "int64Value", (int64_t) -64, "int64s");
        assert_numeric_item(dc, "uint8Value", (uint8_t) 8, "uint8s");
        assert_numeric_item(dc, "uint16Value", (uint16_t) 16, "uint16s");
        assert_numeric_item(dc, "uint32Value", (uint32_t) 32, "uint32s");
        assert_numeric_item(dc, "uint64Value", (uint64_t) 64, "uint64s");
        assert_numeric_item(dc, "floatValue", (float) 3.14, "floats");
        sensor_readings_flag = true;
    }
    else if (MAX_DEVICE_ID == dc->count())
    {
        assert_string_item(dc, "bmc_ver", "1.22", "");
        device_id_flag = true;
    }
    else if (MAX_SEL_RECORDS == dc->count())
    {
        assert_string_item(dc, "fake_SEL_record_1", "SEL_01", "");
        assert_string_item(dc, "fake_SEL_record_2", "SEL_02", "");
        assert_string_item(dc, "fake_SEL_record_3", "SEL_03", "");
        assert_string_item(dc, "fake_SEL_record_4", "SEL_04", "");
        assert_string_item(dc, "fake_SEL_record_5", "SEL_05", "");
        sel_records_flag = true;
    }
    else if (MAX_ACPI_POWER == dc->count())
    {
        assert_string_item(dc, "system_power_state", "S1", "");
        assert_string_item(dc, "device_power_state", "D1", "");
        acpi_power_flag = true;
    }
    else
        ASSERT_TRUE(false);

    callback_flag = (device_id_flag && sel_records_flag &&
        sensor_readings_flag && acpi_power_flag);
}

TEST_F(ipmiSensor_Tests, collect_inventory_ipmi)
{
    static const char MCA_DFX_FLAG[] = "ORCM_MCA_sensor_ipmi_ts_dfx";
    setenv(MCA_DFX_FLAG, "1", 1);

    dataContainer dc;
    ipmiSensor sensor("test");

    sensor.setInventoryPtr(inventory_callback);
    sensor.init();
    sensor.collect_inventory();
    ASSERT_TRUE_TIMEOUT(callback_flag, TIMEOUT);
    sensor.finalize();

    unsetenv(MCA_DFX_FLAG);
}


TEST_F(ipmiSensorFactory_Tests, throw_factory_exception)
{
    static struct ipmiSensorFactory *factory;
    factory = ipmiSensorFactory::getInstance();
    factory->load(true, "localhost");
    mocks[PLUGIN_TEST_INIT].pushState(FAILURE);
    mocks[PLUGIN_TEST_FINALIZE].pushState(FAILURE);
    mocks[PLUGIN_INIT].pushState(FAILURE);
    mocks[PLUGIN_FINALIZE].pushState(FAILURE);

    factory->init();
    factory->close();
    factory->load(false, "localhost");
    factory->init();
    factory->close();
 }


TEST_F(ipmiSensor_Tests, ipmi_sample)
{
    dataContainer dc;
    ipmiSensor sensor("test");

    sensor.setSamplingPtr(sample_callback);
    sensor.init();
    sensor.sample();
    ASSERT_TRUE_TIMEOUT(callback_flag, TIMEOUT);
    sensor.finalize();
}

TEST_F(ipmiSensor_Tests, constructors_destructors)
{
    IpmiTestSensor *its = new IpmiTestSensor("test");
    delete its;
    ipmiSensor *is = new ipmiSensor("test");
    delete is;
}
