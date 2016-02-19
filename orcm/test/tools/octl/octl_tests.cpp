/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "octl_tests.h"

// OPAL
#include "opal/dss/dss.h"

// ORCM

// C++
#include <iostream>
#include <iomanip>

// Fixture
using namespace std;

extern "C" {
    int orcm_octl_sensor_change_sampling(int command, char** cmdlist);
};

orte_rml_module_send_buffer_nb_fn_t ut_octl_tests::saved_send_buffer = NULL;
orte_rml_module_recv_buffer_nb_fn_t ut_octl_tests::saved_recv_buffer = NULL;
int ut_octl_tests::next_send_result = ORTE_SUCCESS;
int ut_octl_tests::next_proc_result = ORTE_SUCCESS;

void ut_octl_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();

    saved_send_buffer = orte_rml.send_buffer_nb;
    orte_rml.send_buffer_nb = SendBuffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;
    orte_rml.recv_buffer_nb = /*(orte_rml_module_recv_buffer_nb_fn_t)*/RecvBuffer;

    octl_mocking.orcm_cfgi_base_get_hostname_proc_callback = GetHostnameProc;
}

void ut_octl_tests::TearDownTestCase()
{
    octl_mocking.orcm_cfgi_base_get_hostname_proc_callback = NULL;

    orte_rml.recv_buffer_nb = saved_recv_buffer;
    saved_recv_buffer = NULL;

    orte_rml.send_buffer_nb = saved_send_buffer;
    saved_send_buffer = NULL;

    // Release OPAL level resources
    opal_dss_close();
}

void ut_octl_tests::RecvBuffer(orte_process_name_t* peer,
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
    int rv = ORTE_SUCCESS;
    opal_dss.pack(&xfer->data, &rv, 1, OPAL_INT);
    xfer->active = false;
    OBJ_RELEASE(xfer);
}

int ut_octl_tests::SendBuffer(orte_process_name_t* peer,
                              struct opal_buffer_t* buffer,
                              orte_rml_tag_t tag,
                              orte_rml_buffer_callback_fn_t cbfunc,
                              void* cbdata)
{
    int rv = ORTE_SUCCESS;
    rv = next_send_result;
    next_send_result = ORTE_SUCCESS;
    if(ORTE_SUCCESS == rv) {
        OBJ_RELEASE(buffer);
    }
    return rv;
}

int ut_octl_tests::GetHostnameProc(char* hostname, orte_process_name_t* proc)
{
    int rv = next_proc_result;
    next_proc_result = ORTE_SUCCESS;
    return rv;
}


// Testing the data collection class
TEST_F(ut_octl_tests, positive_test)
{
    const char* cmdlist[5] = {
        "sensor",
        "dummy",
        "tn01,tn02",
        "sensor_plugin:sensor_label",
        NULL
    };
    int rv = orcm_octl_sensor_change_sampling(0, (char**)cmdlist);
    EXPECT_EQ(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, negative_test_1)
{
    const char* cmdlist[5] = {
        "sensor",
        "dummy",
        "tn01,tn02",
        "sensor_plugin:sensor_label",
        NULL
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_sensor_change_sampling(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, negative_test_2)
{
    const char* cmdlist[5] = {
        "sensor",
        "dummy",
        "tn01,tn02",
        "sensor_plugin:sensor_label",
        NULL
    };
    next_proc_result = ORTE_ERROR;
    int rv = orcm_octl_sensor_change_sampling(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, negative_test_3)
{
    const char* cmdlist[5] = {
        "sensor",
        "dummy",
        "",
        "sensor_plugin:sensor_label",
        NULL
    };
    int rv = orcm_octl_sensor_change_sampling(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, negative_test_4)
{
    const char* cmdlist[4] = {
        "sensor",
        "dummy",
        "sensor_plugin:sensor_label",
        NULL
    };
    const char* cmdlist2[6] = {
        "sensor",
        "dummy",
        "tn01,tn02",
        "sensor_plugin:sensor_label",
        "extra",
        NULL
    };
    int rv = orcm_octl_sensor_change_sampling(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
    rv = orcm_octl_sensor_change_sampling(1, (char**)cmdlist2);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, negative_test_5)
{
    const char* cmdlist[5] = {
        "sensor",
        "dummy",
        "tn01,tn02",
        "sensor_plugin:sensor_label",
        NULL
    };
    int rv = orcm_octl_sensor_change_sampling(3, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
    rv = orcm_octl_sensor_change_sampling(-1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}
