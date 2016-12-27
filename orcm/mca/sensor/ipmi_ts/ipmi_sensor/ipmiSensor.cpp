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
#include <sstream>

#include "ipmiSensor.hpp"
#include "orcm/util/string_utils.h"

using namespace std;

static double powerReading(uint64_t accu,
                           uint64_t accu_prev,
                           uint64_t cnt,
                           uint64_t cnt_prev);

typedef struct
{
    uint64_t accu_a;
    uint64_t cnt_a;
    uint64_t accu_b;
    uint64_t cnt_b;
} previousReadings_t;
static map<string, previousReadings_t> prevReadings;

class ipmiSensorCallbackDirectory
{
public:
    ipmiDataLoggerCallback samplingCbFn;
    ipmiDataLoggerCallback inventoryCbFn;
    ipmiErrorLoggerCallback errorCbFn;

    ipmiSensorCallbackDirectory(ipmiDataLoggerCallback smpl, ipmiDataLoggerCallback inv, ipmiErrorLoggerCallback err) :
        samplingCbFn(smpl), inventoryCbFn(inv), errorCbFn(err) {} ;
};
static ipmiSensorCallbackDirectory* getDirectoryPtr();

void ipmiSensor::init() {}

void ipmiSensor::sample()
{
    ipmiSensorCallbackDirectory *directory = NULL;
    ipmiHAL *cmd = ipmiHAL::getInstance();
    cmd->startAgents();

    buffer data;
    directory = new ipmiSensorCallbackDirectory(samplingPtr_, inventoryPtr_, errorPtr_);
    cmd->addRequest(GETDEVICEID, data, hostname, get_sensor_readings_cb, (void*)directory);

    directory = new ipmiSensorCallbackDirectory(samplingPtr_, inventoryPtr_, errorPtr_);
    cmd->addRequest(GETACPIPOWER, data, hostname, get_sensor_readings_cb, (void*)directory);

    directory = new ipmiSensorCallbackDirectory(samplingPtr_, inventoryPtr_, errorPtr_);
    cmd->addRequest(GETSELRECORDS, data, hostname, get_sensor_readings_cb, (void*)directory);

    directory = new ipmiSensorCallbackDirectory(samplingPtr_, inventoryPtr_, errorPtr_);
    cmd->addRequest(GETSENSORREADINGS, data, hostname, get_sensor_readings_cb, (void*)directory);

    directory = new ipmiSensorCallbackDirectory(samplingPtr_, inventoryPtr_, errorPtr_);
    cmd->addRequest(GETPSUPOWER, data, hostname, get_nodepower_readings_cb, (void*)directory);
}

void ipmiSensor::collect_inventory()
{
    ipmiSensorCallbackDirectory *directory = NULL;
    ipmiHAL *cmd = ipmiHAL::getInstance();
    cmd->startAgents();

    buffer data;
    directory = new ipmiSensorCallbackDirectory(samplingPtr_, inventoryPtr_, errorPtr_);
    cmd->addRequest(GETDEVICEID, data, hostname,
        get_sensor_list_cb, (void*)directory);

    directory = new ipmiSensorCallbackDirectory(samplingPtr_, inventoryPtr_, errorPtr_);
    cmd->addRequest(READFRUDATA, data, hostname,
        get_sensor_list_cb, (void*)directory);

    directory = new ipmiSensorCallbackDirectory(samplingPtr_, inventoryPtr_, errorPtr_);
    cmd->addRequest(GETSENSORLIST, data, hostname,
        get_sensor_list_cb, (void*)directory);
}

void ipmiSensor::finalize() {}

static void get_sensor_list_cb(string bmc, ipmiResponse response, void* cbData)
{
    ipmiSensorCallbackDirectory *directory = (ipmiSensorCallbackDirectory*) cbData;

    if (response.wasSuccessful())
    {
        dataContainer* dc = new dataContainer(response.getReadings());
        if (NULL != directory->inventoryCbFn)
            directory->inventoryCbFn(bmc, dc);
        delete dc;
    } else {
        if (NULL != directory->errorCbFn)
            directory->errorCbFn(bmc, response.getErrorMessage(), response.getCompletionMessage());
    }

    delete directory;
}

static void get_sensor_readings_cb(string bmc, ipmiResponse response, void* cbData)
{
    ipmiSensorCallbackDirectory *directory = (ipmiSensorCallbackDirectory*) cbData;

    if (response.wasSuccessful())
    {
        dataContainer *dc = new dataContainer(response.getReadings());
        if (NULL != directory->samplingCbFn)
            directory->samplingCbFn(bmc, dc);
        delete dc;
    } else {
        if (NULL != directory->errorCbFn)
            directory->errorCbFn(bmc, response.getErrorMessage(), response.getCompletionMessage());
    }

    delete directory;
}

static double powerReading(uint64_t accu,
                           uint64_t accu_prev,
                           uint64_t cnt,
                           uint64_t cnt_prev)
{
    double divisor = ((double) cnt - cnt_prev);
    double reading =((double) accu - accu_prev);
    if (0 < divisor)
        reading /= divisor;
    if (reading > 1000.0)
        reading = -1;

    return reading;
}

static void get_nodepower_readings_cb(string bmc, ipmiResponse response, void* cbData)
{
    typedef enum {NO_ERROR = 0, ERROR_A, ERROR_B} errorStates;
    errorStates errorFlag = NO_ERROR;

    ipmiSensorCallbackDirectory *directory = (ipmiSensorCallbackDirectory*) cbData;

    if (response.wasSuccessful())
    {
        dataContainer readings = response.getReadings();
        uint64_t accu_a = readings.getValue<uint64_t>("accu_a");
        uint64_t cnt_a = readings.getValue<uint64_t>("cnt_a");
        uint64_t accu_b = readings.getValue<uint64_t>("accu_b");
        uint64_t cnt_b = readings.getValue<uint64_t>("cnt_b");

        // Overflow detection
        if (prevReadings[bmc].accu_a > accu_a || prevReadings[bmc].cnt_a > cnt_a)
            errorFlag = ERROR_A;

        if (prevReadings[bmc].accu_b > accu_b || prevReadings[bmc].cnt_b > cnt_b)
            errorFlag = ERROR_B;

        double reading = powerReading(accu_a, prevReadings[bmc].accu_a, cnt_a, prevReadings[bmc].cnt_a) +
                         powerReading(accu_b, prevReadings[bmc].accu_b, cnt_b, prevReadings[bmc].cnt_b);

        prevReadings[bmc].accu_a = accu_a;
        prevReadings[bmc].cnt_a = cnt_a;
        prevReadings[bmc].accu_b = accu_b;
        prevReadings[bmc].cnt_b = cnt_b;

        switch (errorFlag)
        {
            case NO_ERROR:
                {
                    dataContainer *dc = new dataContainer();
                    dc->put("nodepower", reading, "W");

                    if (NULL != directory->samplingCbFn)
                        directory->samplingCbFn(bmc, dc);

                    delete dc;
                }
                break;
            case ERROR_A:
                if (NULL != directory->errorCbFn)
                    directory->errorCbFn(bmc, "Detected overflow on reading A", "");
                break;
            case ERROR_B:
                if (NULL != directory->errorCbFn)
                    directory->errorCbFn(bmc, "Detected overflow on reading B", "");
                break;
        }
    } else {
        if (NULL != directory->errorCbFn)
            directory->errorCbFn(bmc, response.getErrorMessage(), response.getCompletionMessage());
    }

    delete directory;
}
