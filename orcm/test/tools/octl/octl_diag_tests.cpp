/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <iostream>
#include "octl_diag_tests.h"

using namespace std;

#define MY_ORCM_SUCCESS 0

void ut_octl_diag_tests::SetUpTestCase()
{
    ut_octl_tests::SetUpTestCase();

}

void ut_octl_diag_tests::TearDownTestCase()
{
    ut_octl_tests::TearDownTestCase();

}

void ut_octl_diag_tests::NegRecvBuffer(orte_process_name_t* peer,
                                           orte_rml_tag_t tag,
                                           bool persistent,
                                           orte_rml_buffer_callback_fn_t cbfunc,
                                           void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    xfer->name.jobid = 0;
    xfer->name.vpid = 0;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    int rv = 1;
    opal_dss.pack(&xfer->data, &rv, 1, OPAL_INT);
    xfer->active = false;
}

void ut_octl_diag_tests::NegFloatRecvBuffer(orte_process_name_t* peer,
                                           orte_rml_tag_t tag,
                                           bool persistent,
                                           orte_rml_buffer_callback_fn_t cbfunc,
                                           void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    xfer->name.jobid = 0;
    xfer->name.vpid = 0;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    float rv = 1.0;
    opal_dss.pack(&xfer->data, &rv, 1, OPAL_FLOAT);
    xfer->active = false;
}

TEST_F(ut_octl_diag_tests, diag_cpu_negative)
{
    const char* cmdlist[3] = {
        "diag",
        "cpu",
        NULL
    };
    int rv = orcm_octl_diag_cpu((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}


TEST_F(ut_octl_diag_tests, diag_cpu_negative_1)
{
    const char* cmdlist[4] = {
        "diag",
        "cpu",
        "master[",
        NULL
    };
    int rv = orcm_octl_diag_cpu((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_diag_tests, diag_cpu_negative_2)
{
    const char* cmdlist[4] = {
        "diag",
        "cpu",
        "master",
        NULL
    };

    next_proc_result = ORTE_ERROR;

    int rv = orcm_octl_diag_cpu((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}


TEST_F(ut_octl_diag_tests, diag_cpu_negative_3)
{
    const char* cmdlist[4] = {
        "diag",
        "cpu",
        "master",
        NULL
    };

    next_send_result = ORTE_ERROR;

    int rv = orcm_octl_diag_cpu((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_diag_tests, diag_cpu_negative_4)
{
    const char* cmdlist[4] = {
        "diag",
        "cpu",
        "master",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = NegRecvBuffer;

    int rv = orcm_octl_diag_cpu((char**)cmdlist);
    ASSERT_EQ(MY_ORCM_SUCCESS, rv);
    orte_rml.recv_buffer_nb = saved_recv_buffer;
}

TEST_F(ut_octl_diag_tests, diag_cpu_negative_5)
{
    const char* cmdlist[4] = {
        "diag",
        "cpu",
        "master",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = NegFloatRecvBuffer;

    int rv = orcm_octl_diag_cpu((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
    orte_rml.recv_buffer_nb = saved_recv_buffer;
}

TEST_F(ut_octl_diag_tests, diag_eth_negative)
{
    const char* cmdlist[3] = {
        "diag",
        "eth",
        NULL
    };
    int rv = orcm_octl_diag_eth((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}


TEST_F(ut_octl_diag_tests, diag_eth_negative_1)
{
    const char* cmdlist[4] = {
        "diag",
        "eth",
        "master[",
        NULL
    };
    int rv = orcm_octl_diag_eth((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_diag_tests, diag_eth_negative_2)
{
    const char* cmdlist[4] = {
        "diag",
        "eth",
        "master",
        NULL
    };

    next_proc_result = ORTE_ERROR;

    int rv = orcm_octl_diag_eth((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}


TEST_F(ut_octl_diag_tests, diag_eth_negative_3)
{
    const char* cmdlist[4] = {
        "diag",
        "eth",
        "master",
        NULL
    };

    next_send_result = ORTE_ERROR;

    int rv = orcm_octl_diag_eth((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_diag_tests, diag_eth_negative_4)
{
    const char* cmdlist[4] = {
        "diag",
        "eth",
        "master",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = NegRecvBuffer;

    int rv = orcm_octl_diag_eth((char**)cmdlist);
    ASSERT_EQ(MY_ORCM_SUCCESS, rv);
    orte_rml.recv_buffer_nb = saved_recv_buffer;
}

TEST_F(ut_octl_diag_tests, diag_eth_negative_5)
{
    const char* cmdlist[4] = {
        "diag",
        "eth",
        "master",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = NegFloatRecvBuffer;

    int rv = orcm_octl_diag_eth((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
    orte_rml.recv_buffer_nb = saved_recv_buffer;
}

TEST_F(ut_octl_diag_tests, diag_mem_negative)
{
    const char* cmdlist[3] = {
        "diag",
        "mem",
        NULL
    };
    int rv = orcm_octl_diag_mem((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}


TEST_F(ut_octl_diag_tests, diag_mem_negative_1)
{
    const char* cmdlist[4] = {
        "diag",
        "mem",
        "master[",
        NULL
    };
    int rv = orcm_octl_diag_mem((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_diag_tests, diag_mem_negative_2)
{
    const char* cmdlist[4] = {
        "diag",
        "mem",
        "master",
        NULL
    };

    next_proc_result = ORTE_ERROR;

    int rv = orcm_octl_diag_mem((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}


TEST_F(ut_octl_diag_tests, diag_mem_negative_3)
{
    const char* cmdlist[4] = {
        "diag",
        "mem",
        "master",
        NULL
    };

    next_send_result = ORTE_ERROR;

    int rv = orcm_octl_diag_mem((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_diag_tests, diag_mem_negative_4)
{
    const char* cmdlist[4] = {
        "diag",
        "mem",
        "master",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = NegRecvBuffer;

    int rv = orcm_octl_diag_mem((char**)cmdlist);
    ASSERT_EQ(MY_ORCM_SUCCESS, rv);
    orte_rml.recv_buffer_nb = saved_recv_buffer;
}

TEST_F(ut_octl_diag_tests, diag_mem_negative_5)
{
    const char* cmdlist[4] = {
        "diag",
        "mem",
        "master",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = NegFloatRecvBuffer;

    int rv = orcm_octl_diag_mem((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
    orte_rml.recv_buffer_nb = saved_recv_buffer;
}

