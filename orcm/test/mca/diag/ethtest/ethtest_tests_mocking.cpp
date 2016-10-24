/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ethtest_tests_mocking.h"

#include <iostream>
using namespace std;

ethtest_tests_mocking ethtest_mocking;


extern "C" { // Mocking must use correct "C" linkages
    #include <stdarg.h>

    int __wrap_opal_dss_pack(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type)
    {
        if(NULL == ethtest_mocking.opal_dss_pack_callback) {
            __real_opal_dss_pack(buffer, src, num_vals, type);
        } else {
            ethtest_mocking.opal_dss_pack_callback(buffer, src, num_vals, type);
        }
    }
} // extern "C"

ethtest_tests_mocking::ethtest_tests_mocking() :
    opal_dss_pack_callback(NULL), fail_at(0),
    current_execution(0), finish_execution(0)
{
}
