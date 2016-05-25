/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "componentpower_tests_mocking.h"

#include <iostream>
using namespace std;

int componentpower_tests_mocking::ntimes = 0;
bool componentpower_tests_mocking::keep_mocking = false;

componentpower_tests_mocking componentpower_mocking;

extern "C" { // Mocking must use correct "C" linkages
    #include <stdarg.h>

    void __wrap_orte_errmgr_base_log(int err, char* file, int lineno)
    {
        if(NULL == componentpower_mocking.orte_errmgr_base_log_callback) {
            __real_orte_errmgr_base_log(err, file, lineno);
        } else {
            componentpower_mocking.orte_errmgr_base_log_callback(err, file, lineno);
        }
    }

    FILE* __wrap_fopen(const char * filename, const char * mode)
    {
        if(NULL == componentpower_mocking.fopen_callback) {
            return __real_fopen(filename, mode);
        } else {
            return componentpower_mocking.fopen_callback(filename, mode);
        }
    }

    int __wrap_open(char *filename, int access, int permission)
    {
        if(NULL == componentpower_mocking.open_callback) {
            return __real_open(filename, access, permission);
        } else {
            return componentpower_mocking.open_callback(filename, access, permission);
        }
    }

    opal_event_base_t* __wrap_opal_progress_thread_init(const char* name)
    {
        if(NULL == componentpower_mocking.opal_progress_thread_init_callback) {
            return __real_opal_progress_thread_init(name);
        } else {
            return componentpower_mocking.opal_progress_thread_init_callback(name);
        }
    }

    int __wrap_read(int handle, void *buffer, int nbyte)
    {
        if(NULL == componentpower_mocking.read_callback) {
            return __real_read(handle, buffer, nbyte);
        } else {
            if (0 < componentpower_mocking.ntimes) {
                componentpower_mocking.ntimes--;
            }
            if (0 == componentpower_mocking.ntimes) {
                if (NULL == componentpower_mocking.read_return_error_callback) {
                    return componentpower_mocking.read_callback(handle, buffer, nbyte);
                } else {
                    int rc = componentpower_mocking.read_return_error_callback(handle, buffer, nbyte);
                    if (!componentpower_mocking.keep_mocking) {
                        componentpower_mocking.read_return_error_callback = NULL;
                    }
                    return rc;
                }
            } else {
                if (NULL == componentpower_mocking.read_callback) {
                    return __real_read(handle, buffer, nbyte);
                } else {
                    return componentpower_mocking.read_callback(handle, buffer, nbyte);
                }
            }
        }
    }
} // extern "C"

componentpower_tests_mocking::componentpower_tests_mocking() :
    orte_errmgr_base_log_callback(NULL),
    read_callback(NULL), read_return_error_callback(NULL), fopen_callback(NULL),
    open_callback(NULL), opal_progress_thread_init_callback(NULL)
{
}
