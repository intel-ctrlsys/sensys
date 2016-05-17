/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ETHTEST_TESTS_MOCKING_H
#define ETHTEST_TESTS_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    #include <stdlib.h>
    #include <stdio.h>
    #include "orte/include/orte/types.h"
    #include "orcm/mca/analytics/analytics_types.h"

    extern int __real_opal_dss_pack(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type);


#ifdef __cplusplus
}
#endif // __cplusplus

typedef int (*opal_dss_pack_fn_t)(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type);

class ethtest_tests_mocking
{
    public:
        // Construction
        ethtest_tests_mocking();

        // Public Callbacks
        opal_dss_pack_fn_t opal_dss_pack_callback;

        //Public variables
        int fail_at;
        int current_execution;
        int finish_execution;

};

extern ethtest_tests_mocking ethtest_mocking;

#endif // ETHTEST_TESTS_MOCKING_H
