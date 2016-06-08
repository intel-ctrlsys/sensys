/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "util_tests_mocking.h"

using namespace std;

util_tests_mocking util_mocking;

extern "C" { // Mocking must use correct "C" linkages
    #include <stdarg.h>

    void __wrap_orte_errmgr_base_log(int err, char* file, int lineno)
    {
        if(NULL == util_mocking.orte_errmgr_base_log_callback) {
            __real_orte_errmgr_base_log(err, file, lineno);
        } else {
            util_mocking.orte_errmgr_base_log_callback(err, file, lineno);
        }
    }

} // extern "C"

util_tests_mocking::util_tests_mocking() :
    orte_errmgr_base_log_callback(NULL)
{
}
