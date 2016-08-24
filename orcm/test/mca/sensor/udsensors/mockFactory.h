/*
 * Copyright (c) 2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MOCK_SENSOR_FACTORY_TESTS_H
#define MOCK_SENSOR_FACTORY_TESTS_H

#include <iostream>
#include <stdexcept>
#include "gtest/gtest.h"

#include "orcm/mca/sensor/udsensors/sensorFactory.h"

bool mock_readdir;
bool mock_dlopen;
bool mock_dlclose;
bool mock_dlsym;
bool mock_dlerror;
bool mock_plugin;
bool throwOnInit;
bool throwOnSample;
bool dlopenReturnHandler;
bool getPluginNameMock;
bool initPluginMock;
bool emptyContainer;
int n_mocked_plugins;

class mockPlugin
{
public:
    virtual int init(void);
    virtual int finalize(void);
    virtual void sample(dataContainer &dc);
};

extern "C" {
    extern void *__real_dlopen(const char *filename, int flag);
    extern void *__real_dlsym(void *handle, const char *symbol);
    extern char *__real_dlerror(void);
    extern int *__real_dlclose(void *handle);
    extern struct dirent *__real_readdir(DIR *dirp);

    const char *fake_dlerror = "Im a fake error message.";
    dirent *fake_dirent = NULL;

    static void * get_mockPlugin(void) {
        return new mockPlugin();
    }

    static void * get_mockPluginName(void) {
        return strdup("MyFakePlugin");
    }

    void * getMockedSymbol(const char *symbol) {
        if (!strcmp(symbol, "getPluginName") && getPluginNameMock)
            return (void*)get_mockPluginName;
        else if (!strcmp(symbol, "initPlugin") && initPluginMock)
            return (void*)get_mockPlugin;
        else
            return NULL;
    }

    struct dirent* __wrap_readdir(DIR *dirp)
    {
        if (mock_readdir) {
            const char *fake_name = "libudplugin_fake";
            if (fake_dirent) {
                free(fake_dirent);
                fake_dirent = 0 ;
            }
            if (n_mocked_plugins > 0) {
                fake_dirent = (dirent*)malloc(sizeof(dirent));
                if (fake_dirent) {
                    snprintf(fake_dirent->d_name,
                             sizeof(fake_dirent->d_name),
                             "%s%d.so", fake_name, n_mocked_plugins);
                }
                n_mocked_plugins--;
                return fake_dirent;
            }
            return NULL;
        } else {
            return __real_readdir(dirp);
        }
    }

    void * __wrap_dlopen(const char *filename, int flag) {
        if (mock_dlopen) {
            if (dlopenReturnHandler)
                return (void*)0xdeadbeef;
            else
                return NULL;
        } else {
            return __real_dlopen(filename, flag);
        }
    }

    void * __wrap_dlsym(void *handle, const char *symbol) {
        if (mock_dlsym) {
            if (mock_plugin) {
                return getMockedSymbol(symbol);
            } else {
                return NULL;
            }
        } else {
            return __real_dlsym(handle, symbol);
        }
    }

    char * __wrap_dlerror(void) {
        if (mock_dlerror)
            return NULL;
        else if (mock_dlopen || mock_dlsym)
            return (char*)fake_dlerror;
        else
            return __real_dlerror();
    }

    int * __wrap_dlclose(void *handle) {
        if (mock_dlclose)
            return 0;
        else
            return __real_dlclose(handle);
    }
}

int mockPlugin::init()
{
    if (throwOnInit) {
        throw std::runtime_error("Failing on mockPlugin.init");
    }
    return 0;
}

int mockPlugin::finalize()
{
    return 0;
}

void mockPlugin::sample(dataContainer &dc)
{
    if (throwOnSample) {
        throw std::runtime_error("Failing on mockPlugin.sample");
    }
    if (!emptyContainer) {
        dc.put("intValue_1", 1234, "ints");
    }
    return;
}

#endif
