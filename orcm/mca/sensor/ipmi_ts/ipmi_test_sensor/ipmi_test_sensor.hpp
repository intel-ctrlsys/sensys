/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/sensor/ipmi_ts/ipmiSensorInterface.h"
#ifndef ORCM_SENSOR_IPMI_TS_TEST_SENSOR_H
#define ORCM_SENSOR_IPMI_TS_TEST_SENSOR_H

namespace ipmi
{
    class metrics
    {
    public:
        std::string label;
        float value;
        std::string units;
        metrics(std::string l, float v, std::string u) : label(l), value(v), units(u) {};
    };

    class staticMetrics
    {
    public:
        std::string label;
        std::string value;
        staticMetrics(std::string l, std::string v) : label(l), value(v) {};
    };
}

class IpmiTestSensor : public ipmiSensorInterface
{
private:
    static std::vector<ipmi::metrics>metricsData;
    static std::vector<ipmi::staticMetrics>staticMetricsData;
    static std::vector<ipmi::staticMetrics>staticMetricsInventory;

    // Initializers
    static vector<ipmi::metrics> metricsDataList();
    static vector<ipmi::staticMetrics> staticMetricsDataList();
    static vector<ipmi::staticMetrics> staticMetricsInventoryList();

    // Methods
    void addMetricsData(dataContainer &dc, const vector<ipmi::metrics> &list);
    void addStaticMetricsData(dataContainer &dc, const vector<ipmi::staticMetrics> &list);

public:
    IpmiTestSensor(std::string hostname) : ipmiSensorInterface(hostname) {};
    virtual ~IpmiTestSensor(){};
    void init();
    void sample(dataContainer &dc);
    void collect_inventory(dataContainer &dc);
    void finalize();
};

#endif
