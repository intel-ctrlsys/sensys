/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef UTIL_TESTS_MOCKING_H
#define UTIL_TESTS_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include <stdlib.h>
    #include <stdio.h>
    #include "orte/runtime/orte_globals.h"

    extern void __real_orte_errmgr_base_log(int err, char* file, int lineno);

#ifdef __cplusplus
}
#endif // __cplusplus

typedef void (*orte_errmgr_base_log_callback_fn_t)(int err, char* file, int lineno);

class util_tests_mocking
{
    public:
        // Construction
        util_tests_mocking();

        // Public Callbacks
        orte_errmgr_base_log_callback_fn_t orte_errmgr_base_log_callback;
};

extern util_tests_mocking util_mocking;

#endif // UTIL_TESTS_MOCKING_H
