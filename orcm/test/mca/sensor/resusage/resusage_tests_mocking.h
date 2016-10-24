/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef RESUSAGE_TESTS_MOCKING_H
#define RESUSAGE_TESTS_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include "orte/mca/state/state.h"
    #include "orcm/mca/analytics/analytics_types.h"

    extern orte_job_t* __real_orte_get_job_data_object(orte_jobid_t);
    extern void __real_orcm_analytics_base_send_data(
                orcm_analytics_value_t *data);

#ifdef __cplusplus
}
#endif // __cplusplus

typedef orte_job_t* (*orte_get_job_data_object_fn_t)(orte_jobid_t);
typedef void (*orcm_analytics_base_send_data_fn_t)(orcm_analytics_value_t *data);


class resusage_tests_mocking
{
    public:
        // Construction
        resusage_tests_mocking();

        // Public Callbacks
        orte_get_job_data_object_fn_t orte_get_job_data_object_callback;
        orcm_analytics_base_send_data_fn_t orcm_analytics_base_send_data_callback;
};

extern resusage_tests_mocking resusage_mocking;

#endif // RESUSAGE_TESTS_MOCKING_H
