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
    cmd->addRequest(GETDEVICEID, data, hostname,
        get_sensor_readings_cb, (void*)directory);

    directory = new ipmiSensorCallbackDirectory(samplingPtr_, inventoryPtr_, errorPtr_);
    cmd->addRequest(GETACPIPOWER, data, hostname,
        get_sensor_readings_cb, (void*)directory);

    directory = new ipmiSensorCallbackDirectory(samplingPtr_, inventoryPtr_, errorPtr_);
    cmd->addRequest(GETSELRECORDS, data, hostname,
        get_sensor_readings_cb, (void*)directory);

    directory = new ipmiSensorCallbackDirectory(samplingPtr_, inventoryPtr_, errorPtr_);
    cmd->addRequest(GETSENSORREADINGS, data, hostname,
        get_sensor_readings_cb, (void*)directory);
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
