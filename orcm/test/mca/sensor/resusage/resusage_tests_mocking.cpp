/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "resusage_tests_mocking.h"

#include <iostream>
using namespace std;

resusage_tests_mocking resusage_mocking;


extern "C" { // Mocking must use correct "C" linkages.
    orte_job_t* __wrap_orte_get_job_data_object(orte_jobid_t id)
    {
        if(NULL == resusage_mocking.orte_get_job_data_object_callback) {
            return __real_orte_get_job_data_object(id);
        } else {
            return resusage_mocking.orte_get_job_data_object_callback(id);
        }
    }

    void __wrap_orcm_analytics_base_send_data(orcm_analytics_value_t *data)
    {
        if(NULL == resusage_mocking.orcm_analytics_base_send_data_callback) {
            __real_orcm_analytics_base_send_data(data);
        } else {
            resusage_mocking.orcm_analytics_base_send_data_callback(data);
        }
    }
}; // extern "C"

resusage_tests_mocking::resusage_tests_mocking() :
    orte_get_job_data_object_callback(NULL),
    orcm_analytics_base_send_data_callback(NULL)
{
}
