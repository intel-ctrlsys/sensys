/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMIHAL_H
#define IPMIHAL_H

#include "orcm/mca/sensor/ipmi_ts/ipmiLibInterface.h"

#include <string>
#include <stdexcept>

typedef void (*ipmiHAL_callback)(std::string bmc, ipmiResponse response, void* cbData);

class ipmiHAL
{
private:
    static ipmiHAL* s_instance;
    static ipmiLibInterface* agent;

    bool consuming;

    ipmiHAL();
    ~ipmiHAL();

    void initialize_();
    void initializeConsumerLoop_();
    void initializeDispatchingAgents_();
    void initializeDispatchThreads_(int n);
    void finalizeThreads_();
    void releaseHandlers_();
    const char* getThreadName_(int index);

public:
    static ipmiHAL* getInstance();

    inline std::set<std::string> getBmcList() {return ((ipmiLibInterface*) agent)->getBmcList();};
    static void throwWhenNullPointer(void* ptr);

    bool isQueueEmpty();
    void addRequest(ipmiCommands command,
                    buffer data,
                    std::string bmc,
                    ipmiHAL_callback cbFunction,
                    void* cbData);

    /*
     * These functions are only for testing purposes.
     * Please do not use them in production code.
     * (Unless you know what you are doing).
     */
    void startAgents();
    static void terminateInstance();

};

namespace ipmiHAL_objects
{
    class unableToAllocateObj : public std::runtime_error
    {
    public:
        unableToAllocateObj() : std::runtime_error( "Unable to allocate object in memory" ) {}
    };
}

#endif //GREI_IPMIHAL_H
