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
#include "orcm/mca/sensor/ipmi_ts/ipmiSensorInterface.h"
#include "orcm/mca/sensor/ipmi/ipmi_parser.h"

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
    const std::string MCA_DFX_FLAG;

    HAL() : MCA_DFX_FLAG("ORCM_MCA_sensor_ipmi_ts_dfx") {};

    bool callbackFlag = false;
    bool isSensorListEmpty = true;
    bool isSampleContainerEmpty = true;

    virtual void SetUp();
    virtual void TearDown();

    static double elapsedSecs(time_t startTime);
    static void cbFunc(std::string bmc, ipmiResponse response, void* cbData);
    static void cbFunc_sensorList(std::string bmc, ipmiResponse response, void* cbData);
    static void cbFunc_readings(std::string bmc, ipmiResponse response, void* cbData);

};

class realHAL : public testing::Test
{
protected:
    ipmiHAL *HWobject = NULL;
    std::string fileName;

    static const int MY_MODULE_PRIORITY = 20;

    virtual void SetUp();
    virtual void TearDown();

    void initParserFramework();
    void cleanParserFramework();
    void setFileName();
};

class EXTRA : public testing::Test
{
};

class TEST_SENSOR : public testing::Test
{
protected:
    ipmiSensorInterface *sensor;
    virtual void SetUp();
    virtual void TearDown();
};

class TEST_SENSOR_DFX : public testing::Test
{
protected:
    ipmiSensorInterface *sensor;
    virtual void SetUp();
    virtual void TearDown();
};

#endif // IPMIHAL_TESTS_H
