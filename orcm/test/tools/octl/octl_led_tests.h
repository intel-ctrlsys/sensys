/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef OCTL_LED_TESTS_H
#define OCTL_LED_TESTS_H

#include "gtest/gtest.h"
#include "octl_mocking.h"
#include "orcm/runtime/led_control.h"
#include "orcm/mca/sensor/ipmi/ipmi_parser.h"
#include "orcm/mca/sensor/ipmi/ipmi_parser_interface.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    // OPAL
    #include "opal/dss/dss.h"
    // ORTE
    #include "orte/mca/rml/rml.h"
    #include "orcm/util/utils.h"
    #include "orcm/tools/octl/common.h"
    #include "orcm/mca/parser/parser.h"
    #include "orcm/mca/parser/base/base.h"
#ifdef __cplusplus
};
#endif // __cplusplus

class ut_octl_led_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();

        static int SendBuffer(orte_process_name_t* peer,
                              struct opal_buffer_t* buffer,
                              orte_rml_tag_t tag,
                              orte_rml_buffer_callback_fn_t cbfunc,
                              void* cbdata);
        static void RecvBuffer(orte_process_name_t* peer,
                               orte_rml_tag_t tag,
                               bool persistent,
                               orte_rml_buffer_callback_fn_t cbfunc,
                               void* cbdata);
        static void RecvCancel(orte_process_name_t*, orte_rml_tag_t);
        static int GetHostnameProc(char* hostname, orte_process_name_t* proc);
        static orte_rml_module_send_buffer_nb_fn_t saved_send_buffer;
        static orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;
        static orte_rml_module_recv_cancel_fn_t saved_recv_cancel;
        static orcm_cmd_server_flag_t command;
        static orcm_cmd_server_flag_t subcommand;
        static int next_state;
        static int next_return;
}; // class

#endif // OCTL_LED_TESTS_H
