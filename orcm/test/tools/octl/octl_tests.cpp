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
    int orcm_octl_set_notifier_policy(int command, char** cmdlist);
    int orcm_octl_get_notifier_policy(int command, char** cmdlist);
    int orcm_octl_set_notifier_smtp(int command, char** cmdlist);
    int orcm_octl_get_notifier_smtp(int command, char** cmdlist);
    int orcm_octl_sensor_store(int command, char** cmdlist);
};

orte_rml_module_send_buffer_nb_fn_t ut_octl_tests::saved_send_buffer = NULL;
orte_rml_module_recv_buffer_nb_fn_t ut_octl_tests::saved_recv_buffer = NULL;
orte_rml_module_recv_cancel_fn_t ut_octl_tests::saved_recv_cancel = NULL;
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
    saved_recv_cancel = orte_rml.recv_cancel;
    orte_rml.recv_cancel = RecvCancel;

    saved_recv_cancel = orte_rml.recv_cancel;
    orte_rml.recv_cancel = /*(orte_rml_module_recv_cancel_fn_t)*/RecvCancel;

    octl_mocking.orcm_cfgi_base_get_hostname_proc_callback = GetHostnameProc;
}

void ut_octl_tests::TearDownTestCase()
{
    octl_mocking.orcm_cfgi_base_get_hostname_proc_callback = NULL;
    octl_mocking.opal_dss_pack_callback = NULL;

    orte_rml.recv_buffer_nb = saved_recv_buffer;
    saved_recv_buffer = NULL;

    orte_rml.send_buffer_nb = saved_send_buffer;
    saved_send_buffer = NULL;

    orte_rml.recv_cancel = saved_recv_cancel;
    saved_recv_cancel = NULL;

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
    int count = 0;
    opal_dss.pack(&xfer->data, &rv, 1, OPAL_INT);
    opal_dss.pack(&xfer->data, &count, 1, OPAL_INT);
    xfer->active = false;
}

void ut_octl_tests::RecvBufferError(orte_process_name_t* peer,
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
    int rv = ORTE_ERROR;
    int count = 0;
    opal_dss.pack(&xfer->data, &rv, 1, OPAL_INT);
    opal_dss.pack(&xfer->data, &count, 1, OPAL_INT);
    xfer->active = false;
}

void ut_octl_tests::RecvBufferCount(orte_process_name_t* peer,
                              orte_rml_tag_t tag,
                              bool persistent,
                              orte_rml_buffer_callback_fn_t cbfunc,
                              void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    struct timeval now;
    opal_buffer_t *data = OBJ_NEW(opal_buffer_t);
    gettimeofday(&now, NULL);
    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);
    xfer->name.jobid = 0;
    xfer->name.vpid = 0;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    int rv = ORTE_SUCCESS;
    int count = 1;
    opal_dss.pack(&xfer->data, &rv, 1, OPAL_INT);
    opal_dss.pack(&xfer->data, &count, 1, OPAL_INT);
    opal_dss.pack(&xfer->data, &data, 1, OPAL_BUFFER);
    xfer->active = false;
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


void ut_octl_tests::RecvCancel(orte_process_name_t* peer,
                              orte_rml_tag_t tag)
{

}

int ut_octl_tests::GetHostnameProc(char* hostname, orte_process_name_t* proc)
{
    int rv = next_proc_result;
    next_proc_result = ORTE_SUCCESS;
    return rv;
}

int ut_octl_tests::OpalDssPack(opal_buffer_t* buffer,
                                  const void* src,
                                  int32_t num_vals,
                                  opal_data_type_t type)
{
    int rc = __real_opal_dss_pack(buffer, src, num_vals, type);
    if(octl_mocking.fail_at == octl_mocking.current_execution++)
        return ORTE_ERROR;
    return rc;
}

// Testing the data collection class
TEST_F(ut_octl_tests, sensor_sampling_positive_test)
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

TEST_F(ut_octl_tests, sensor_sampling_negative_test_1)
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

TEST_F(ut_octl_tests, sensor_sampling_negative_test_2)
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

TEST_F(ut_octl_tests, sensor_sampling_negative_test_3)
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

TEST_F(ut_octl_tests, sensor_sampling_negative_test_4)
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

TEST_F(ut_octl_tests, sensor_sampling_negative_test_5)
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

// Testing the notifier set policy
TEST_F(ut_octl_tests, notifier_set_policy_test)
{
    const char* cmdlist[7] = {
        "notifier",
        "set",
        "policy",
        "emerg",
        "smtp",
        "tn01,tn02",
        NULL
    };
    next_proc_result = ORTE_SUCCESS;
    next_send_result = ORTE_SUCCESS;
    int rv = orcm_octl_set_notifier_policy(0, (char**)cmdlist);
    EXPECT_EQ(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_set_policy_negative_test_1)
{
    const char* cmdlist[6] = {
        "notifier",
        "set",
        "policy",
        "emerg",
        "smtp",
        ""
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_set_notifier_policy(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_set_policy_negative_test_2)
{
    const char* cmdlist[6] = {
        "notifier",
        "set",
        "policy",
        "emerg",
        "sntp",
        "tn01,tn02"
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_set_notifier_policy(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_set_policy_negative_test_3)
{
    const char* cmdlist[6] = {
        "notifier",
        "set",
        "policy",
        "emeg",
        "smtp",
        "tn01,tn02"
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_set_notifier_policy(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_set_policy_negative_test_4)
{
    const char* cmdlist[6] = {
        "notifier",
        "set",
        "icy",
        "emerg",
        "smtp",
        "tn01,tn02"
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_set_notifier_policy(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

// Testing the notifier get policy
TEST_F(ut_octl_tests, notifier_get_policy_test)
{
    const char* cmdlist[5] = {
        "notifier",
        "get",
        "policy",
        "tn01,tn02",
        NULL
    };
    next_proc_result = ORTE_SUCCESS;
    next_send_result = ORTE_SUCCESS;
    int rv = orcm_octl_get_notifier_policy(0, (char**)cmdlist);
    EXPECT_EQ(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_get_policy_negative_test_1)
{
    const char* cmdlist[4] = {
        "notifier",
        "get",
        "policy",
        ""
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_get_notifier_policy(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_get_policy_negative_test_2)
{
    const char* cmdlist[4] = {
        "notifier",
        "get",
        "",
        "tn01,tn02"
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_get_notifier_policy(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_get_policy_negative_test_3)
{
    const char* cmdlist[4] = {
        "notifer",
        "get",
        "policy",
        "tn01,tn02"
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_get_notifier_policy(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, negative_test_sensor_storage_1)
{
    const char* cmdlist[] = {
        "sensor",
        "store",
        "all",
        "dummy",
        "tn01",
        NULL
    };
    int rv = orcm_octl_sensor_store(2, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, negative_test_sensor_storage_2)
{
    const char* cmdlist[] = {
        "sensor",
        "store",
        "all",
        "tn01",
        NULL
    };
    int rv = orcm_octl_sensor_store(-1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, negative_test_sensor_storage_3)
{
    const char* cmdlist[] = {
        "sensor",
        "store",
        "all",
        "",
        NULL
    };
    int rv = orcm_octl_sensor_store(0, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, negative_test_sensor_storage_4)
{
    const char* cmdlist[] = {
        "sensor",
        "store",
        "",
        "",
        NULL
    };
    int rv = orcm_octl_sensor_store(0, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, negative_test_sensor_storage_5)
{
    const char* cmdlist[] = {
        "sensor",
        "store",
        "all",
        "tn01,tn02",
        NULL
    };
    next_proc_result = ORTE_ERROR;
    int rv = orcm_octl_sensor_store(2, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, negative_test_sensor_storage_6)
{
    const char* cmdlist[] = {
        "sensor",
        "store",
        "all",
        "tn01,tn02",
        NULL
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_sensor_store(2, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, positive_test_sensor_storage_1)
{
    const char* cmdlist[] = {
        "sensor",
        "store",
        "all",
        "tn01",
        NULL
    };
    int rv = orcm_octl_sensor_store(2, (char**)cmdlist);
    EXPECT_EQ(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, positive_test_sensor_storage_2)
{
    const char* cmdlist[] = {
        "sensor",
        "store",
        "all",
        "tn01,tn02",
        NULL
    };
    int rv = orcm_octl_sensor_store(2, (char**)cmdlist);
    EXPECT_EQ(ORTE_SUCCESS, rv);
}

// Testing the notifier set smtp policy
TEST_F(ut_octl_tests, notifier_set_smtp_policy_test1)
{
    const char* cmdlist[7] = {
        "notifier",
        "set",
        "smtp-policy",
        "server_name",
        "OutlookOR.intel.com",
        "tn01,tn02",
        NULL
    };
    next_proc_result = ORTE_SUCCESS;
    next_send_result = ORTE_SUCCESS;
    int rv = orcm_octl_set_notifier_smtp(0, (char**)cmdlist);
    EXPECT_EQ(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_set_smtp_policy_test2)
{
    const char* cmdlist[7] = {
        "notifier",
        "set",
        "smtp-policy",
        "server_port",
        "40",
        "tn01,tn02",
        NULL
    };
    next_proc_result = ORTE_SUCCESS;
    next_send_result = ORTE_SUCCESS;
    int rv = orcm_octl_set_notifier_smtp(0, (char**)cmdlist);
    EXPECT_EQ(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_set_smtp_policy_test3)
{
    const char* cmdlist[7] = {
        "notifier",
        "set",
        "smtp-policy",
        "priority",
        "1",
        "tn01,tn02",
        NULL
    };
    next_proc_result = ORTE_SUCCESS;
    next_send_result = ORTE_SUCCESS;
    int rv = orcm_octl_set_notifier_smtp(0, (char**)cmdlist);
    EXPECT_EQ(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_set_smtp_policy_negative_test_1)
{
    const char* cmdlist[7] = {
        "notifier",
        "set",
        "smtp-policy",
        "server_name",
        "OutlookOR.intel.com",
        "",
         NULL
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_set_notifier_smtp(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_set_smtp_policy_negative_test_2)
{
    const char* cmdlist[7] = {
        "notifier",
        "set",
        "smtp-policy",
        "server",
        "OutlookOR.intel.com",
        "tn01,tn02",
         NULL
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_set_notifier_smtp(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_set_smtp_policy_negative_test_3)
{
    const char* cmdlist[7] = {
        "notifier",
        "set",
        "smtp-policy",
        "port",
        "smtp",
        "tn01,tn02",
         NULL
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_set_notifier_smtp(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_set_smtp_policy_negative_test_4)
{
    const char* cmdlist[7] = {
        "notifier",
        "set",
        "smtp-policy",
        "priority",
        "xxxx",
        "tn01,tn02",
         NULL
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_set_notifier_smtp(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_set_smtp_policy_negative_test_5)
{
    const char* cmdlist[4] = {
        "notifier",
        "set",
        "smtp-policy",
         NULL
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_set_notifier_smtp(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_set_smtp_policy_negative_test_6)
{
    const char* cmdlist[7] = {
        "notifier",
        "set",
        "smtp-policy",
        "server_name",
        "OutlookOR.intel.com",
        "tn01,tn02",
        NULL
    };
    int return_value = 0;
    octl_mocking.opal_dss_pack_callback = OpalDssPack;
    next_proc_result = ORTE_SUCCESS;
    next_send_result = ORTE_SUCCESS;

    for(octl_mocking.fail_at = 1; octl_mocking.fail_at <= 3; octl_mocking.fail_at++){
            octl_mocking.current_execution = 1;
            return_value = orcm_octl_set_notifier_smtp(0, (char**)cmdlist);

    }
    octl_mocking.opal_dss_pack_callback = NULL;
    EXPECT_NE(ORTE_SUCCESS, return_value);
}

// Testing the notifier get smtp policy
TEST_F(ut_octl_tests, notifier_get_smtp_policy_test)
{
    const char* cmdlist[5] = {
        "notifier",
        "get",
        "smtp-policy",
        "tn01,tn02",
        NULL
    };
    next_proc_result = ORTE_SUCCESS;
    next_send_result = ORTE_SUCCESS;
    int rv = orcm_octl_get_notifier_smtp(0, (char**)cmdlist);
    EXPECT_EQ(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_get_smtp_policy_negative_test_1)
{
    const char* cmdlist[5] = {
        "notifier",
        "get",
        "smtp-policy",
        "",
        NULL
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_get_notifier_smtp(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_get_smtp_policy_negative_test_2)
{
    const char* cmdlist[5] = {
        "notifier",
        "get",
        "",
        "tn01,tn02",
        NULL
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_get_notifier_smtp(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_get_smtp_policy_negative_test_3)
{
    const char* cmdlist[5] = {
        "notifer",
        "get",
        "smtp-policy",
        "tntn02",
        NULL
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_get_notifier_smtp(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_get_smtp_policy_negative_test_4)
{
    const char* cmdlist[3] = {
        "notifer",
        "get",
        NULL
    };
    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_get_notifier_smtp(1, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_get_smtp_policy_negative_test_5)
{
    const char* cmdlist[5] = {
        "notifier",
        "get",
        "smtp-policy",
        "tn01,tn02",
        NULL
    };
    int return_value = 0;
    octl_mocking.opal_dss_pack_callback = OpalDssPack;

    for(octl_mocking.fail_at = 1; octl_mocking.fail_at <= 2; octl_mocking.fail_at++){
            octl_mocking.current_execution = 1;
            return_value = orcm_octl_get_notifier_smtp(0, (char**)cmdlist);

    }

    EXPECT_NE(ORTE_SUCCESS, return_value);
}

TEST_F(ut_octl_tests, notifier_get_smtp_policy_negative_test_6)
{
    const char* cmdlist[5] = {
        "notifier",
        "get",
        "smtp-policy",
        "tn01,tn02",
        NULL
    };
    next_proc_result = ORTE_ERROR;
    next_send_result = ORTE_SUCCESS;
    int rv = orcm_octl_get_notifier_smtp(0, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_get_smtp_policy_negative_test_7)
{
    const char* cmdlist[5] = {
        "notifier",
        "get",
        "smtp-policy",
        "tn01,tn02",
        NULL
    };
    orte_rml.recv_buffer_nb = RecvBufferError;
    next_proc_result = ORTE_SUCCESS;
    next_send_result = ORTE_SUCCESS;
    int rv = orcm_octl_get_notifier_smtp(0, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}

TEST_F(ut_octl_tests, notifier_get_smtp_policy_negative_test_8)
{
    const char* cmdlist[5] = {
        "notifier",
        "get",
        "smtp-policy",
        "tn01,tn02",
        NULL
    };
    orte_rml.recv_buffer_nb = RecvBufferCount;
    next_proc_result = ORTE_SUCCESS;
    next_send_result = ORTE_SUCCESS;
    int rv = orcm_octl_get_notifier_smtp(0, (char**)cmdlist);
    EXPECT_NE(ORTE_SUCCESS, rv);
}
