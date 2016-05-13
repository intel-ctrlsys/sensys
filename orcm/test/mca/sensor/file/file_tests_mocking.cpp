/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "file_tests_mocking.h"

#include <iostream>
using namespace std;

file_tests_mocking file_mocking;


extern "C" { // Mocking must use correct "C" linkages.
    orte_job_t* __wrap_orte_get_job_data_object(orte_jobid_t id)
    {
        if(NULL == file_mocking.orte_get_job_data_object_callback) {
            return __real_orte_get_job_data_object(id);
        } else {
            return file_mocking.orte_get_job_data_object_callback(id);
        }
    }
} // extern "C"

file_tests_mocking::file_tests_mocking() :
    orte_get_job_data_object_callback(NULL)
{
}
