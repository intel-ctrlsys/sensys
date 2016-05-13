/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <iostream>
#include "octl_queue_tests.h"

using namespace std;

#define MY_ORCM_SUCCESS 0

void ut_octl_queue_tests::SetUpTestCase()
{
    ut_octl_tests::SetUpTestCase();

}

void ut_octl_queue_tests::TearDownTestCase()
{
    ut_octl_tests::TearDownTestCase();

}

void ut_octl_queue_tests::NegAllocRecvBuffer(orte_process_name_t* peer,
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
    int num_queues = 1;
    opal_dss.pack(&xfer->data, &num_queues, 1, OPAL_INT);
    const char* name = "status";
    opal_dss.pack(&xfer->data, &name, 1, OPAL_STRING);
    int num_sessions = 1;
    opal_dss.pack(&xfer->data, &num_sessions, 1, OPAL_INT);
    opal_dss.pack(&xfer->data, &num_sessions, 1, OPAL_INT);
    xfer->active = false;
}

void ut_octl_queue_tests::NegQueuesRecvBuffer(orte_process_name_t* peer,
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

void ut_octl_queue_tests::NegNameRecvBuffer(orte_process_name_t* peer,
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
    int num_queues = 1;
    opal_dss.pack(&xfer->data, &num_queues, 1, OPAL_INT);
    int num_sessions = 1;
    opal_dss.pack(&xfer->data, &num_sessions, 1, OPAL_INT);
    xfer->active = false;
}


void ut_octl_queue_tests::NegSessionsRecvBuffer(orte_process_name_t* peer,
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
    int num_queues = 1;
    opal_dss.pack(&xfer->data, &num_queues, 1, OPAL_INT);
    const char* name = "status";
    opal_dss.pack(&xfer->data, &name, 1, OPAL_STRING);
    opal_dss.pack(&xfer->data, &name, 1, OPAL_STRING);
    xfer->active = false;
}


void ut_octl_queue_tests::ZeroSessionRecvBuffer(orte_process_name_t* peer,
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
    int num_queues = 1;
    opal_dss.pack(&xfer->data, &num_queues, 1, OPAL_INT);
    const char* name = "status";
    opal_dss.pack(&xfer->data, &name, 1, OPAL_STRING);
    int num_sessions = 0;
    opal_dss.pack(&xfer->data, &num_sessions, 1, OPAL_INT);
    opal_dss.pack(&xfer->data, &num_sessions, 1, OPAL_INT);
    xfer->active = false;
}

void ut_octl_queue_tests::ZeroQueuesRecvBuffer(orte_process_name_t* peer,
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
    int num_queues = 0;
    opal_dss.pack(&xfer->data, &num_queues, 1, OPAL_INT);
    xfer->active = false;
}


TEST_F(ut_octl_queue_tests, queue_status_negative)
{
    const char* cmdlist[3] = {
        "queue",
        "status",
        NULL
    };

    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_queue_status((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_queue_tests, queue_status_negative_send)
{
    const char* cmdlist[3] = {
        "queue",
        "status",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = NegQueuesRecvBuffer;

    int rv = orcm_octl_queue_status((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb = saved_recv_buffer;
}

TEST_F(ut_octl_queue_tests, queue_status_negative_alloc)
{
    const char* cmdlist[3] = {
        "queue",
        "status",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = NegAllocRecvBuffer;

    int rv = orcm_octl_queue_status((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb = saved_recv_buffer;
}

TEST_F(ut_octl_queue_tests, queue_status_negative_name)
{
    const char* cmdlist[3] = {
        "queue",
        "status",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = NegNameRecvBuffer;

    int rv = orcm_octl_queue_status((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb = saved_recv_buffer;
}

TEST_F(ut_octl_queue_tests, queue_status_negative_sessions)
{
    const char* cmdlist[3] = {
        "queue",
        "status",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = NegSessionsRecvBuffer;

    int rv = orcm_octl_queue_status((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb = saved_recv_buffer;
}

TEST_F(ut_octl_queue_tests, queue_status_zero_sessions)
{
    const char* cmdlist[3] = {
        "queue",
        "status",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = ZeroSessionRecvBuffer;

    int rv = orcm_octl_queue_status((char**)cmdlist);
    ASSERT_EQ(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb = saved_recv_buffer;
}

TEST_F(ut_octl_queue_tests, queue_status_zero_queues)
{
    const char* cmdlist[3] = {
        "queue",
        "status",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = ZeroQueuesRecvBuffer;

    int rv = orcm_octl_queue_status((char**)cmdlist);
    ASSERT_EQ(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb = saved_recv_buffer;
}

