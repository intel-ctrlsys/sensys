/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMI_RESPONSE_FACTORY_MOCKS_H
#define IPMI_RESPONSE_FACTORY_MOCKS_H

#include "testUtils.h"
extern "C"
{
    #include <time.h>
};

enum supportedMocks
{
    LOCALTIME
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
    extern struct tm *__real_localtime(const time_t *timep);
    struct tm *__wrap_localtime(const time_t *timep)
   {
        mockStates state = (mockStates) mocks[LOCALTIME].nextMockState();
        if (NO_MOCK == state)
            return __real_localtime(timep);

        switch (state)
        {
            default:
                return NULL;
        }
    }
};
#endif // IPMI_RESPONSE_FACTORY_MOCKS_H
