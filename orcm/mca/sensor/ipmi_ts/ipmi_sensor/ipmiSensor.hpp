/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/sensor/ipmi_ts/ipmiSensorInterface.h"
#include "orcm/mca/sensor/ipmi_ts/ipmiHAL.h"

class ipmiSensor : public ipmiSensorInterface
{
public:
    ipmiSensor(std::string hostname) : ipmiSensorInterface(hostname) {};
    virtual ~ipmiSensor(){};
    void init();
    void sample();
    void collect_inventory();
    void finalize();
};

static dataContainer* add_to_datacontainer(dataContainer list,
                                           std::string prefix,
                                           std::string bmc);

static void get_sensor_list_cb(std::string bmc,
        ipmiResponse response, void* cbData);

static void get_sensor_readings_cb(std::string bmc,
        ipmiResponse response, void* cbData);

static void get_nodepower_readings_cb(std::string bmc,
        ipmiResponse response, void* cbData);
