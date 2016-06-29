/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
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
        int rc = __real_orcm_init(flags);
        if(NULL != orcmd_mocking.orcm_init_callback){
            orcmd_mocking.orcm_init_callback();
        }
        return rc;
    }

} // extern "C"

orcmd_tests_mocking::orcmd_tests_mocking() : orcm_init_callback(NULL)
{
}
