/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <string>
#include <vector>


#include "ipmi_test_sensor.hpp"

using namespace std;
using namespace ipmi;

vector<metrics> IpmiTestSensor::metricsData = IpmiTestSensor::metricsDataList();
vector<staticMetrics> IpmiTestSensor::staticMetricsData = IpmiTestSensor::staticMetricsDataList();
vector<staticMetrics> IpmiTestSensor::staticMetricsInventory = IpmiTestSensor::staticMetricsInventoryList();

void IpmiTestSensor::init()
{
}

vector<metrics> IpmiTestSensor::metricsDataList()
{
    vector<metrics> retValue;
    retValue.push_back(metrics("Exhaust Temperature", 55.0, "C"));
    retValue.push_back(metrics("PSU 1 Power", 100.0, "W"));
    retValue.push_back(metrics("PSU 2 Power", 50.0, "W"));
    retValue.push_back(metrics("CPU FAN 1", 1200.0, "rpm"));
    retValue.push_back(metrics("CPU FAN 2", 800.0, "rpm"));
    return retValue;
}

vector<staticMetrics> IpmiTestSensor::staticMetricsDataList()
{
    vector<staticMetrics> retValue;
    retValue.push_back(staticMetrics("bmcfwrev", "4.2"));
    retValue.push_back(staticMetrics("ipmiver", "2.0"));
    retValue.push_back(staticMetrics("manufacturer_id", "some_long_id"));
    retValue.push_back(staticMetrics("sys_power_state", "ON"));
    retValue.push_back(staticMetrics("dev_power_state", "ON"));
    return retValue;
}

vector<staticMetrics> IpmiTestSensor::staticMetricsInventoryList()
{
    vector<staticMetrics> retValue;
    retValue.push_back(staticMetrics("bmc_ver", "4.2" ));
    retValue.push_back(staticMetrics("ipmi_ver", "2.0" ));
    retValue.push_back(staticMetrics("bb_serial", "some_long_id" ));
    retValue.push_back(staticMetrics("bb_vendor", "Intel Corporation" ));
    retValue.push_back(staticMetrics("sensor_ipmi_1", "bmcfwrev" ));
    retValue.push_back(staticMetrics("sensor_ipmi_2", "ipmiver" ));
    retValue.push_back(staticMetrics("sensor_ipmi_3", "manufacturer_id" ));
    retValue.push_back(staticMetrics("sensor_ipmi_4", "sys_power_state" ));
    retValue.push_back(staticMetrics("sensor_ipmi_5", "dev_power_state" ));
    retValue.push_back(staticMetrics("sensor_ipmi_6", "Exhaust Temperature" ));
    retValue.push_back(staticMetrics("sensor_ipmi_7", "PSU 1 Power" ));
    retValue.push_back(staticMetrics("sensor_ipmi_8", "PSU 2 Power" ));
    retValue.push_back(staticMetrics("sensor_ipmi_9", "CPU FAN 1" ));
    retValue.push_back(staticMetrics("sensor_ipmi_10", "CPU FAN 2" ));
    return retValue;
}

void IpmiTestSensor::addMetricsData(dataContainer &dc, const vector<metrics> &list)
{
    for (vector<metrics>::const_iterator it = list.begin(); it != list.end(); ++it)
        dc.put(it->label, it->value, it->units);
}

void IpmiTestSensor::addStaticMetricsData(dataContainer &dc, const vector<staticMetrics> &list)
{
    for (vector<staticMetrics>::const_iterator it = list.begin(); it != list.end(); ++it)
        dc.put(it->label, it->value, "");
}

void IpmiTestSensor::sample()
{
    dataContainer *dc = new dataContainer();
    addMetricsData(*dc, metricsData);
    addStaticMetricsData(*dc, staticMetricsData);

    if (NULL != samplingPtr_)
        samplingPtr_(hostname, dc);
    delete dc;
}

void IpmiTestSensor::collect_inventory()
{
    dataContainer *dc = new dataContainer();
    addStaticMetricsData(*dc, staticMetricsInventory);
    dc->put(hostname, hostname, hostname);

    if (NULL != inventoryPtr_)
        inventoryPtr_(hostname, dc);
    delete dc;
}

void IpmiTestSensor::finalize()
{
}

