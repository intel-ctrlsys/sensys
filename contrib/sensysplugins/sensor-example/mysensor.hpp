/*
 * Copyright (c) 2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "udsensors.h"

class mySensor : public UDSensor
{
public:
    mySensor(){};
    virtual ~mySensor(){};
    void init();
    void sample(dataContainer &dc);
    void finalize();
};
