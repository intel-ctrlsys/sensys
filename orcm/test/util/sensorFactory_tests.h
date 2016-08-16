/*
 * Copyright (c) 2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SENSOR_FACTORY_TESTS_H
#define SENSOR_FACTORY_TESTS_H

#include <iostream>
#include <stdexcept>
#include "gtest/gtest.h"

#include "orcm/mca/sensor/udsensors/sensorFactory.h"

bool mock_dlopen;
bool mock_dlsym;
bool mock_dlerror;
bool mock_plugin;
bool throwOnInit;

class mockPlugin
{
public:
    virtual int init(void);
    virtual int finalize(void);
};

extern "C" {
    extern void *__real_dlopen(const char *filename, int flag);
    extern void *__real_dlsym(void *handle, const char *symbol);
    extern char *__real_dlerror(void);

    void * get_mockPlugin(void) {
        return new mockPlugin();
    }

    void * __wrap_dlopen(const char *filename, int flag) {
        if (mock_dlopen) {
            return NULL;
        } else {
            return __real_dlopen(filename, flag);
        }
    }

    void * __wrap_dlsym(void *handle, const char *symbol) {
        if (mock_dlsym) {
            if (mock_plugin) {
                return (void*)get_mockPlugin;
            } else {
                return NULL;
            }
        } else {
            return __real_dlsym(handle, symbol);
        }
    }

    char * __wrap_dlerror(void) {
        if (mock_dlerror) {
            return NULL;
        } else if (mock_dlopen || mock_dlsym) {
            return strdup("I'm a fake error message");
        } else {
            return __real_dlerror();
        }
    }
}

class ut_sensorFactory: public testing::Test
{
public:
    struct sensorFactory *obj;
protected:
    virtual void SetUp();
    virtual void TearDown();
};

#endif
