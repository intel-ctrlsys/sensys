/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMI_SENSOR_MOCKS_H
#define IPMI_SENSOR_MOCKS_H

#include "testUtils.h"
#include "orcm/mca/sensor/ipmi_ts/ipmiSensorFactory.hpp"

enum supportedMocks
{
    PLUGIN_INIT,
    PLUGIN_FINALIZE,
    PLUGIN_TEST_INIT,
    PLUGIN_TEST_FINALIZE
};

static std::map<supportedMocks, mockManager> mocks;

enum mockStates
{
    NO_MOCK = 0,
    SUCCESS,
    FAILURE,
};

extern "C"
{
    extern void __real__ZN14IpmiTestSensor4initEv();
    void __wrap__ZN14IpmiTestSensor4initEv()
    {
        mockStates state = (mockStates) mocks[PLUGIN_TEST_INIT].nextMockState();
        if (NO_MOCK == state)
        {
            __real__ZN14IpmiTestSensor4initEv();
            return;
        }

        switch (state)
        {
            default:
                throw std::runtime_error("Some error");
        }
    }
    extern void __real__ZN14IpmiTestSensor8finalizeEv();
    void __wrap__ZN14IpmiTestSensor8finalizeEv()
    {
        mockStates state = (mockStates) mocks[PLUGIN_TEST_FINALIZE].nextMockState();
        if (NO_MOCK == state)
        {
            __real__ZN14IpmiTestSensor8finalizeEv();
            return;
        }

        switch (state)
        {
            default:
                throw std::runtime_error("Some error");
        }
    }
    extern void __real__ZN10ipmiSensor8finalizeEv();
    void __wrap__ZN10ipmiSensor8finalizeEv()
    {
        mockStates state = (mockStates) mocks[PLUGIN_FINALIZE].nextMockState();
        if (NO_MOCK == state)
        {
            __real__ZN10ipmiSensor8finalizeEv;
            return;
        }

        switch (state)
        {
            default:
                throw std::runtime_error("Some error");
        }
    }
    extern void __real__ZN10ipmiSensor4initEv();
    void __wrap__ZN10ipmiSensor4initEv()
    {
        mockStates state = (mockStates) mocks[PLUGIN_INIT].nextMockState();
        if (NO_MOCK == state)
        {
            __real__ZN10ipmiSensor4initEv();
            return;
        }

        switch (state)
        {
            default:
                throw std::runtime_error("Some error");
        }
    }
};
#endif // IPMI_SENSOR_MOCKS_H
