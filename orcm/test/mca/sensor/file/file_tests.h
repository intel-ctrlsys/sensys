/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef FILE_TESTS_H
#define FILE_TESTS_H

#include "orte/mca/state/state.h"

#include "file_tests_mocking.h"

#include "gtest/gtest.h"

#include <string>
#include <vector>

typedef void (*ActivateJobState_fn_t)(orte_job_t* data, int flag);

class ut_file_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
        static orte_job_t* GetJobDataObject(orte_jobid_t id);
        static orte_job_t* GetJobDataObject2(orte_jobid_t id);
        static void MakeTestFile(const char* file);
        static void ModifyTestFile(const char* file, bool size, bool access, bool modifiy);
        static void ResetTestCase();
        static void MockActivateJobState(orte_job_t*,int);

        static orte_job_t job1;
        static orte_job_t job2;
        static const ActivateJobState_fn_t OriginalActivateJobState;
}; // class

#endif // FILE_TESTS_H
