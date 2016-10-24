/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef CMD_SERVER_TESTS_H
#define CMD_SERVER_TESTS_H

#include "gtest/gtest.h"

class ut_cmd_server_tests: public testing::Test
{
    protected:
        /* gtests */
        virtual void SetUp();
        virtual void TearDown();

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
};

#endif // CMD_SERVER_TESTS_H
