/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "heartbeat_tests.h"
#include "heartbeat_tests_mocking.h"

#include "opal/dss/dss.h"

extern "C" {
    #include "orcm/mca/sensor/heartbeat/sensor_heartbeat.c"
    #include "opal/mca/event/event.h"
}

using namespace std;

int ut_heartbeat_tests::last_orte_error_ = ORCM_SUCCESS;
const char* ut_heartbeat_tests::proc_name_ = "heartbeat_tests";
orte_rml_module_send_buffer_nb_fn_t ut_heartbeat_tests::saved_orte_send_buffer = NULL;
orte_rml_module_recv_buffer_nb_fn_t ut_heartbeat_tests::saved_orte_recv_buffer = NULL;
orte_rml_module_recv_cancel_fn_t ut_heartbeat_tests::saved_orte_recv_cancel = NULL;
orte_job_t* ut_heartbeat_tests::orte_job = NULL;

void ut_heartbeat_tests::SetUpTestCase()
{
    opal_init_test();

    saved_orte_send_buffer = orte_rml.send_buffer_nb;
    saved_orte_recv_buffer = orte_rml.recv_buffer_nb;
    saved_orte_recv_cancel = orte_rml.recv_cancel;
    orte_rml.send_buffer_nb = SendBuffer;
    orte_rml.recv_buffer_nb = RecvBuffer;
    orte_rml.recv_cancel = RecvCancel;

    orte_job = OBJ_NEW(orte_job_t);
}

void ut_heartbeat_tests::SetUp()
{
    last_orte_error_ = ORCM_SUCCESS;
    setMockings();
}

void ut_heartbeat_tests::TearDownTestCase()
{
    SAFE_RELEASE(orte_job);

    resetMockings();

    orte_rml.send_buffer_nb = saved_orte_send_buffer;
    orte_rml.recv_buffer_nb = saved_orte_recv_buffer;
    orte_rml.recv_cancel = saved_orte_recv_cancel;
}

void ut_heartbeat_tests::resetMockings()
{
    heartbeat_mocking.orte_errmgr_base_log_callback = NULL;
    heartbeat_mocking.orte_util_print_name_args_callback = NULL;
    heartbeat_mocking.opal_progress_thread_init_callback = NULL;
    heartbeat_mocking.opal_progress_thread_finalize_callback = NULL;
    heartbeat_mocking.orte_get_job_data_object_callback = NULL;
}

void ut_heartbeat_tests::setMockings()
{
    heartbeat_mocking.orte_errmgr_base_log_callback = OrteErrmgrBaseLog;
    heartbeat_mocking.orte_util_print_name_args_callback = OrteUtilPrintNameArgs;
    heartbeat_mocking.opal_progress_thread_init_callback = OpalProgressThreadInit;
    heartbeat_mocking.opal_progress_thread_finalize_callback = OpalProgressThreadFinalize;
    heartbeat_mocking.orte_get_job_data_object_callback = OrteGetJobDataObject;
}


// Mocking
int ut_heartbeat_tests::SendBuffer(orte_process_name_t* peer, struct opal_buffer_t* buffer,
                                    orte_rml_tag_t tag, orte_rml_buffer_callback_fn_t cbfunc,
                                    void* cbdata)
{
    return ORCM_SUCCESS;
}

int ut_heartbeat_tests::SendBufferReturnError(orte_process_name_t* peer, struct opal_buffer_t* buffer,
                                    orte_rml_tag_t tag, orte_rml_buffer_callback_fn_t cbfunc,
                                    void* cbdata)
{
    return ORCM_ERROR;
}

void ut_heartbeat_tests::RecvBuffer(orte_process_name_t* peer, orte_rml_tag_t tag, bool persistent,
                                    orte_rml_buffer_callback_fn_t cbfunc, void* cbdata)
{
    return;
}

void ut_heartbeat_tests::RecvCancel(orte_process_name_t* peer, orte_rml_tag_t tag)
{
    return;
}

void ut_heartbeat_tests::OrteErrmgrBaseLog(int err, char* file, int lineno)
{
    (void)file;
    (void)lineno;
    last_orte_error_ = err;
}

char* ut_heartbeat_tests::OrteUtilPrintNameArgs(const orte_process_name_t *name)
{
    (void)name;
    return (char*)proc_name_;
}

opal_event_base_t* ut_heartbeat_tests::OpalProgressThreadInit(const char* name)
{
    return NULL;
}

int ut_heartbeat_tests::OpalProgressThreadFinalize(const char* name)
{
    return ORCM_SUCCESS;
}

orte_job_t* ut_heartbeat_tests::OrteGetJobDataObject(orte_jobid_t job)
{
    return orte_job;
}

void callback_event_fn (evutil_socket_t es, short s, void *arg)
{
}

//
// TESTS
//

TEST_F(ut_heartbeat_tests, init_noHPN_noAGREGATOR)
{
    orte_proc_type_t current_type = orte_process_info.proc_type;
    orte_process_info.proc_type = 0;

    EXPECT_EQ(ORCM_SUCCESS,orcm_sensor_heartbeat_module.init());
    orcm_sensor_heartbeat_module.finalize();

    orte_process_info.proc_type = current_type;
}

TEST_F(ut_heartbeat_tests, sample_orte_finalizing)
{
    bool orte_finalizing_value = orte_finalizing;
    orte_finalizing = true;
    orcm_sensor_sampler_t *sampler = OBJ_NEW(orcm_sensor_sampler_t);

    orcm_sensor_heartbeat_module.sample(sampler);

    SAFE_RELEASE(sampler);
    orte_finalizing = orte_finalizing_value;
}

TEST_F(ut_heartbeat_tests, sample_orte_not_initialized)
{
    bool orte_initialized_value = orte_initialized;
    orte_initialized = false;
    orcm_sensor_sampler_t *sampler = OBJ_NEW(orcm_sensor_sampler_t);

    orcm_sensor_heartbeat_module.sample(sampler);

    SAFE_RELEASE(sampler);
    orte_initialized = orte_initialized_value;
}

TEST_F(ut_heartbeat_tests, sample_orte_abnormal_term_ordered)
{
    bool orte_abnormal_term_ordered_value = orte_abnormal_term_ordered;
    orte_abnormal_term_ordered = true;
    orcm_sensor_sampler_t *sampler = OBJ_NEW(orcm_sensor_sampler_t);

    orcm_sensor_heartbeat_module.sample(sampler);

    SAFE_RELEASE(sampler);
    orte_abnormal_term_ordered = orte_abnormal_term_ordered_value;
}

TEST_F(ut_heartbeat_tests, sample_send_buffer_error)
{
    int jobid = ORTE_PROC_MY_HNP->jobid;
    int vpid = ORTE_PROC_MY_HNP->vpid;
    bool orte_initialized_value = orte_initialized;
    orcm_sensor_sampler_t *sampler = OBJ_NEW(orcm_sensor_sampler_t);
    ORTE_PROC_MY_HNP->jobid = 4444;
    ORTE_PROC_MY_HNP->vpid = 4444;
    orte_initialized = true;
    orte_rml.send_buffer_nb = SendBufferReturnError;
    orcm_sensor_heartbeat_module.sample(sampler);

    SAFE_RELEASE(sampler);
    orte_initialized = orte_initialized_value;
    ORTE_PROC_MY_HNP->jobid = jobid;
    ORTE_PROC_MY_HNP->vpid = vpid;
    orte_rml.send_buffer_nb = SendBuffer;
}

TEST_F(ut_heartbeat_tests, sample_invalid_vpid)
{
    int current_value = ORTE_PROC_MY_HNP->jobid;
    bool orte_initialized_value = orte_initialized;
    orcm_sensor_sampler_t *sampler = OBJ_NEW(orcm_sensor_sampler_t);
    ORTE_PROC_MY_HNP->jobid = 4444;
    orte_initialized = true;

    orcm_sensor_heartbeat_module.sample(sampler);

    SAFE_RELEASE(sampler);
    orte_initialized = orte_initialized_value;
    ORTE_PROC_MY_HNP->jobid = current_value;
}

TEST_F(ut_heartbeat_tests, sample_proc_type_CM)
{
    orte_proc_type_t current_type = orte_process_info.proc_type;
    bool orte_initialized_value = orte_initialized;
    orcm_sensor_sampler_t *sampler = OBJ_NEW(orcm_sensor_sampler_t);
    orte_process_info.proc_type = ORTE_PROC_CM;
    orte_initialized = true;

    orcm_sensor_heartbeat_module.sample(sampler);

    SAFE_RELEASE(sampler);
    orte_initialized = orte_initialized_value;
    orte_process_info.proc_type = current_type;
}

TEST_F(ut_heartbeat_tests, api_proc_type_HPN)
{
    orte_proc_type_t current_type = orte_process_info.proc_type;
    orte_process_info.proc_type = ORTE_PROC_HNP;
    orte_jobid_t job = 4444;
    bool orte_initialized_value = orte_initialized;
    orte_initialized = true;
    opal_event_base_t *base_event = opal_event_base_create();
    opal_event_t *event = opal_event_new(base_event, -1, EV_TIMEOUT, callback_event_fn,
                                         (char *)"Sample event");

    EXPECT_EQ(ORCM_SUCCESS,orcm_sensor_heartbeat_module.init());
    orcm_sensor_heartbeat_module.start(job);
    orcm_sensor_heartbeat_module.start(job);
    check_heartbeat(-1, -1, event);
    orcm_sensor_heartbeat_module.finalize();

    orte_initialized = orte_initialized_value;
    orte_process_info.proc_type = current_type;
    opal_event_free(event);
    opal_event_base_free(base_event);
}

TEST_F(ut_heartbeat_tests, check_heartbeat_orte_not_initialized)
{
    opal_event_t event;
    check_heartbeat(0, 0, &event);
    EXPECT_FALSE(check_active);
}

TEST_F(ut_heartbeat_tests, check_heartbeat_orte_abnormal_term_ordered)
{
    bool orte_abnormal_term_ordered_value = orte_abnormal_term_ordered;
    orte_abnormal_term_ordered = true;
    opal_event_t event;

    check_heartbeat(0, 0, &event);
    EXPECT_FALSE(check_active);

    orte_abnormal_term_ordered = orte_abnormal_term_ordered_value;
}

TEST_F(ut_heartbeat_tests, check_heartbeat_orte_finalizing)
{
    bool orte_finalizing_value = orte_finalizing;
    orte_finalizing = true;
    opal_event_t event;

    check_heartbeat(0, 0, &event);
    EXPECT_FALSE(check_active);

    orte_finalizing = orte_finalizing_value;
}

TEST_F(ut_heartbeat_tests, check_heartbeat_null_daemons)
{
    bool orte_initialized_value = orte_initialized;
    orte_initialized = true;
    opal_event_t event;

    check_heartbeat(0, 0, &event);

    orte_initialized = orte_initialized_value;
}
