/*
 * Copyright (c) 2016-2017 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include <iostream>
#include "orcmd_mocking.h"
using namespace std;

orcmd_tests_mocking orcmd_mocking;

extern "C" { // Mocking must use correct "C" linkages
    #include <stdarg.h>

    int __wrap_orcm_init(int flags)
    {
        if(NULL != orcmd_mocking.orcm_init_callback){
            return orcmd_mocking.orcm_init_callback();
        } else {
            return  __real_orcm_init(flags);
        }
    }

} // extern "C"

orcmd_tests_mocking::orcmd_tests_mocking() : orcm_init_callback(NULL)
{
}
