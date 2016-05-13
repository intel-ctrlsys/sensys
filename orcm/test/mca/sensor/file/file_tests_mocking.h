/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef FILE_TESTS_MOCKING_H
#define FILE_TESTS_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include "orte/mca/state/state.h"

    extern orte_job_t* __real_orte_get_job_data_object(orte_jobid_t);

#ifdef __cplusplus
}
#endif // __cplusplus

typedef orte_job_t* (*orte_get_job_data_object_fn_t)(orte_jobid_t);

class file_tests_mocking
{
    public:
        // Construction
        file_tests_mocking();

        // Public Callbacks
        orte_get_job_data_object_fn_t orte_get_job_data_object_callback;
};

extern file_tests_mocking file_mocking;

#endif // FILE_TESTS_MOCKING_H
