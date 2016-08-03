/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "analytics_factory_test_mocking.h"
#include <stdlib.h>
#include <stdio.h>

AnalyticsFactoryTestMocking analytics_factory_mocking;

extern "C" { // Mocking must use correct "C" linkages
    struct dirent* __wrap_readdir(DIR *dirstream)
    {
        if (NULL == analytics_factory_mocking.readdir_callback) {
            return __real_readdir(dirstream);
        } else {
            return analytics_factory_mocking.readdir_callback(dirstream);
        }
    }

    void* __wrap_dlopen(const char *__file, int __mode)
    {
        if (NULL == analytics_factory_mocking.dlopen_callback) {
            return __real_dlopen(__file, __mode);
        } else {
            return analytics_factory_mocking.dlopen_callback(__file, __mode);
        }
    }

    char* __wrap_dlerror()
    {
        if (NULL == analytics_factory_mocking.dlerror_callback) {
            return __real_dlerror();
        } else {
            return analytics_factory_mocking.dlerror_callback();
        }
    }

    void* __wrap_dlsym(void *__restrict __handle, const char *__restrict __name)
    {
        if (NULL == analytics_factory_mocking.dlsym_callback) {
            return __real_dlsym(__handle, __name);
        } else {
            return analytics_factory_mocking.dlsym_callback(__handle, __name);
        }
    }

    int __wrap_dlclose(void *__handle)
    {
        if (NULL == analytics_factory_mocking.dlclose_callback) {
            return __real_dlclose(__handle);
        } else {
            return analytics_factory_mocking.dlclose_callback(__handle);
        }
    }
} // extern "C"
