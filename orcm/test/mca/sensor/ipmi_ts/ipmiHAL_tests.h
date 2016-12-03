/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMIHAL_TESTS_H
#define IPMIHAL_TESTS_H

#include <ctime>

#include "gtest/gtest.h"

#include "orcm/mca/sensor/ipmi_ts/ipmiHAL.h"

extern "C" {
    #include "opal/runtime/opal.h"
    #include "orte/runtime/orte_globals.h"
};

class HAL: public testing::Test
{
protected:
    ipmiHAL *HWobject = NULL;
    buffer *emptyBuffer;
    std::string bmc;
    static const double TIMEOUT = 5; //sec
    bool callbackFlag = false;
    bool isSensorListEmpty = true;

    virtual void SetUp();
    virtual void TearDown();

    static double elapsedSecs(time_t startTime);
    static void cbFunc(std::string bmc, ipmiResponse response, void* cbData);
    static void cbFunc_sensorList(std::string bmc, ipmiResponse response, void* cbData);

};

class EXTRA : public testing::Test
{
};

#endif // IPMIHAL_TESTS_H
