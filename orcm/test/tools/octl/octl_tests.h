/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef OCTL_TESTS_H
#define OCTL_TESTS_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    // OPAL
    #include "opal/dss/dss.h"
    // ORTE
    #include "orte/mca/rml/rml.h"
    // ORCM
    #include "orcm/runtime/orcm_globals.h"
    #include "orcm/mca/db/db.h"
#ifdef __cplusplus
};
#endif // __cplusplus

#include "octl_mocking.h"
#include "gtest/gtest.h"

#include <string>
#include <vector>

class ut_octl_tests: public testing::Test
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
        static void RecvBufferError(orte_process_name_t* peer,
                               orte_rml_tag_t tag,
                               bool persistent,
                               orte_rml_buffer_callback_fn_t cbfunc,
                               void* cbdata);
        static void RecvBufferCount(orte_process_name_t* peer,
                               orte_rml_tag_t tag,
                               bool persistent,
                               orte_rml_buffer_callback_fn_t cbfunc,
                               void* cbdata);
        static void RecvCancel(orte_process_name_t* peer,
                               orte_rml_tag_t tag);
        static int GetHostnameProc(char* hostname, orte_process_name_t* proc);
        static int OpalDssPack(opal_buffer_t* buffer,
                               const void* src,
                               int32_t num_vals,
                               opal_data_type_t type);

        static orte_rml_module_send_buffer_nb_fn_t saved_send_buffer;
        static orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;
        static orte_rml_module_recv_cancel_fn_t saved_recv_cancel;
        static int next_send_result;
        static int next_proc_result;
}; // class

class ut_octl_output_handler_tests: public testing::Test
{
  protected:
    static void SetUpTestCase();
    static void TearDownTestCase();
};
#endif // OCTL_TESTS_H
