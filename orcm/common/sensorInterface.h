/*
 * Copyright (c) 2016-2017  Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SENSOR_INTERFACE_H
#define SENSOR_INTERFACE_H

#include "dataContainer.hpp"

enum supportedTypes
{
    IB,
    OOB
};

class sensorInterface
{
public:
    supportedTypes sensor_type;
    virtual void init(void);
    virtual void finalize(void);
    virtual void sample(dataContainer &data);
};

#endif
