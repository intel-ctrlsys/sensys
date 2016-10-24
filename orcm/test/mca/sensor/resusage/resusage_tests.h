/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#ifndef RESUSAGE_TESTS_H
#define RESUSAGE_TESTS_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    // OPAL
    #include "opal/runtime/opal.h"
    #include "opal/dss/dss_internal.h"
#ifdef __cplusplus
};
#endif // __cplusplus

#include "resusage_tests.h"
#include "orte/mca/state/state.h"
#include "resusage_tests_mocking.h"

#include "gtest/gtest.h"

#include <string>
#include <vector>

class ut_resusage_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();

        static orte_job_t* GetJobDataObject1(orte_jobid_t id);
        static orte_job_t* GetJobDataObject2(orte_jobid_t id);
        static void * ProvideMockedProcess(opal_pointer_array_t *table,
                                           int element_num);
        static void MockedOrcmAnalyticsSend(orcm_analytics_value_t *data);

        static orte_job_t job1;
        static orte_job_t job2;

        static orte_node_t mocked_node;
        static opal_pointer_array_t mocked_array;
        static opal_process_name_t mocked_name;

    public:
        static int err_num;
        static orte_proc_t mocked_process;
}; // class

extern resusage_tests_mocking resusage_mocking;

#endif // RESUSAGE_TESTS_H
