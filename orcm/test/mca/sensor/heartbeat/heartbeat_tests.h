/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef HEARTBEAT_TESTS_H
#define HEARTBEAT_TESTS_H

#include "gtest/gtest.h"
#include "orcm/mca/sensor/heartbeat/sensor_heartbeat.h"

extern "C" {
    #include "opal/runtime/opal.h"
    #include "orte/runtime/orte_globals.h"
    #include "orte/mca/rml/rml.h"
}

#include <iostream>

class ut_heartbeat_tests: public testing::Test {
    protected:
        // Mocking
        static void RecvCancel(orte_process_name_t*, orte_rml_tag_t);
        static void RecvBuffer(orte_process_name_t* peer, orte_rml_tag_t tag, bool persistent,
                               orte_rml_buffer_callback_fn_t cbfunc,void* cbdata);
        static int SendBuffer(orte_process_name_t* peer, struct opal_buffer_t* buffer, orte_rml_tag_t tag,
                              orte_rml_buffer_callback_fn_t cbfunc, void* cbdata);
        static int SendBufferReturnError(orte_process_name_t* peer, struct opal_buffer_t* buffer,
                              orte_rml_tag_t tag, orte_rml_buffer_callback_fn_t cbfunc, void* cbdata);
        static void OrteErrmgrBaseLog(int err, char* file, int lineno);
        static char* OrteUtilPrintNameArgs(const orte_process_name_t *name);
        static opal_event_base_t* OpalProgressThreadInit(const char* name);
        static int OpalProgressThreadFinalize(const char* name);
        static orte_job_t* OrteGetJobDataObject(orte_jobid_t job);
        static void resetMockings();
        static void setMockings();

        static void SetUpTestCase();
        virtual void SetUp();
        static void TearDownTestCase();

        static const char* proc_name_;
        static int last_orte_error_;
        static orte_job_t *orte_job;
        static orte_rml_module_send_buffer_nb_fn_t saved_orte_send_buffer;
        static orte_rml_module_recv_buffer_nb_fn_t saved_orte_recv_buffer;
        static orte_rml_module_recv_cancel_fn_t saved_orte_recv_cancel;

};

#endif
