/*
 * Copyright (c) 2016-2017 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcmd_tests.h"

#include "opal/dss/dss.h"
#include "orte/runtime/orte_globals.h"

// C++
#include <iostream>
#include <iomanip>

// Fixture
using namespace std;

extern "C" {

};

void ut_orcmd_tests::SetUpTestCase()
{

}

void ut_orcmd_tests::ResetTestCase()
{
    orcmd_mocking.orcm_init_callback = NULL;
}

void ut_orcmd_tests::TearDownTestCase()
{

}

void dummy_recv(orte_process_name_t *perr,
               orte_rml_tag_t tag,
               bool persistent,
               orte_rml_buffer_callback_fn_t cbfunc,
               void *data)
{
    return;
}

int ut_orcmd_tests::OrcmInit()
{
    orte_process_info.proc_type = ORCM_DAEMON;
    orte_event_base_active = false;
    orte_rml.recv_buffer_nb = dummy_recv;
    return ORTE_SUCCESS;
}

int ut_orcmd_tests::OrcmInitJobid()
{
    orte_process_info.proc_type = ORCM_AGGREGATOR;
    ORTE_PROC_MY_PARENT->jobid = ORTE_PROC_MY_SCHEDULER->jobid + 1;
    orte_event_base_active = false;
    return ORTE_SUCCESS;
}

int ut_orcmd_tests::OrcmInitVpid()
{
    orte_process_info.proc_type = ORCM_AGGREGATOR;
    ORTE_PROC_MY_PARENT->vpid = ORTE_PROC_MY_SCHEDULER->vpid + 1;
    orte_event_base_active = false;
    return ORTE_SUCCESS;
}

TEST_F(ut_orcmd_tests, orcmd_bad_argument)
{
    const char* cmdlist[3] = {
        "orcm",
        "-",
        NULL
    };
    int rv = run_orcmd(2, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_orcmd_tests, orcmd_no_aggregator)
{
    const char* cmdlist[2] = {
        "orcmd",
        NULL
    };
    ResetTestCase();
    orcmd_mocking.orcm_init_callback = OrcmInit;
    int rv = run_orcmd(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_orcmd_tests, orcmd_no_jobid)
{
    const char* cmdlist[2] = {
        "orcmd",
        NULL
    };
    ResetTestCase();
    orcmd_mocking.orcm_init_callback = OrcmInitJobid;
    int rv = run_orcmd(1, (char**)cmdlist);
    EXPECT_EQ(ORTE_SUCCESS, rv);
}

TEST_F(ut_orcmd_tests, orcmd_no_vpid)
{
    const char* cmdlist[2] = {
        "orcmd",
        NULL
    };
    ResetTestCase();
    orcmd_mocking.orcm_init_callback = OrcmInitVpid;
    int rv = run_orcmd(1, (char**)cmdlist);
    EXPECT_EQ(ORTE_SUCCESS, rv);
}

TEST_F(ut_orcmd_tests, orcmd_verbose_version)
{
    const char* cmdlist[4] = {
        "orcmd",
        "-v",
        "-V",
        NULL
    };
    int rv = run_orcmd(3, (char**)cmdlist);
    EXPECT_EQ(ORTE_SUCCESS, rv);
}
