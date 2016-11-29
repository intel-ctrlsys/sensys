/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SENSOR_INTERFACE_H
#define SENSOR_INTERFACE_H

#include "orcm/common/dataContainer.hpp"

class ipmiSensorInterface
{
protected:
    std::string hostname;
public:
    ipmiSensorInterface(std::string h) {hostname = h;};
    std::string getHostname() {return hostname;};
    virtual void init(void){return;};
    virtual void finalize(void){return;};
    virtual void sample(dataContainer &data){return;};
    virtual void collect_inventory(dataContainer &data){return;};
};

#endif
