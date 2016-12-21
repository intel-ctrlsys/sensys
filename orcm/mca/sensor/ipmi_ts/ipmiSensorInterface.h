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

typedef void (*ipmiDataLoggerCallback)(std::string bmc, dataContainer* dc);
typedef void (*ipmiErrorLoggerCallback)(std::string bmc, std::string errorMessage, std::string completionMessage);

class ipmiSensorInterface
{
protected:
    std::string hostname;
    ipmiDataLoggerCallback samplingPtr_;
    ipmiDataLoggerCallback inventoryPtr_;
    ipmiErrorLoggerCallback errorPtr_;
public:
    ipmiSensorInterface(std::string h) {
        hostname = h;
        samplingPtr_ = NULL;
        inventoryPtr_ = NULL;
        errorPtr_ = NULL;
    };
    std::string getHostname() {return hostname;};
    virtual void init(void){return;};
    virtual void finalize(void){return;};
    virtual void sample(void){return;};
    virtual void collect_inventory(void){return;};

    inline void setSamplingPtr(ipmiDataLoggerCallback ptr) {samplingPtr_ = ptr;};
    inline void setInventoryPtr(ipmiDataLoggerCallback ptr) {inventoryPtr_ = ptr;};
    inline void setErrorPtr(ipmiErrorLoggerCallback ptr) {errorPtr_ = ptr;};
};

#endif
