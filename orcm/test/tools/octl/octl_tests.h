/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
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
        static void RecvCancel(orte_process_name_t* peer,
                               orte_rml_tag_t tag);
        static int GetHostnameProc(char* hostname, orte_process_name_t* proc);

        static orte_rml_module_send_buffer_nb_fn_t saved_send_buffer;
        static orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;
        static orte_rml_module_recv_cancel_fn_t saved_recv_cancel;
        static int next_send_result;
        static int next_proc_result;
}; // class

class ut_octl_query: public testing::Test
{
  protected:
    static void SetUpTestCase();
    static void TearDownTestCase();

    static int ReplaceWildcard(const char *input_string,
                               const char *expected_string,
                               bool stop_on_first);
    static int AssembleDatetime(const char *date,
                                const char *time,
                                const char *expected_datetime);
    static int EventDataFilter(int argc,
                               const char **args);
    static int EventSensorDataFilter(int argc,
                                     const char **args);
    static int AddToStrDate(const char *date,
                            const char *expected_date,
                            int seconds);
    static opal_list_t* CreateFiltersList(int filters_size);
    static int QueryDbStreamVaryingFilters(uint32_t *results_count,
                                           int *stream_index, int filters_size);
    static int CreateBufferFromFilters(opal_buffer_t **buffer,
                                       opal_list_t *filters,
                                       orcm_rm_cmd_flag_t cmd);
    static int CreateBufferFromVaryingFilters(opal_buffer_t **buffer,
                                       opal_list_t *filters,
                                       orcm_rm_cmd_flag_t cmd,
                                       int filters_size);
    static orcm_db_filter_t* CreateStringFilter(const char *field,
                                                const char *string,
                                                orcm_db_comparison_op_t op);
    static void MockedRecvBuffer(orte_process_name_t* peer, orte_rml_tag_t tag,
                                 bool persistent,
                                 orte_rml_buffer_callback_fn_t cbfunc,
                                 void* cbdata)
    static int MockedSendBufferFromDbQuery(orte_process_name_t* peer,
                                 struct opal_buffer_t* buffer,
                                  orte_rml_tag_t tag,
                                  orte_rml_buffer_callback_fn_t cbfunc,
                                  void* cbdata);
    static int MockedSendBufferFromPrint(orte_process_name_t* peer,
                                 struct opal_buffer_t* buffer,
                                  orte_rml_tag_t tag,
                                  orte_rml_buffer_callback_fn_t cbfunc,
                                  void* cbdata);
    static int MockedFailure1SendBuffer(orte_process_name_t* peer,
                                  struct opal_buffer_t* buffer,
                                  orte_rml_tag_t tag,
                                  orte_rml_buffer_callback_fn_t cbfunc,
                                  void* cbdata);
    static int MockedFailure2SendBuffer(orte_process_name_t* peer,
                                  struct opal_buffer_t* buffer,
                                  orte_rml_tag_t tag,
                                  orte_rml_buffer_callback_fn_t cbfunc,
                                  void* cbdata);
    static int MockedFailure3SendBuffer(orte_process_name_t* peer,
                                  struct opal_buffer_t* buffer,
                                  orte_rml_tag_t tag,
                                  orte_rml_buffer_callback_fn_t cbfunc,
                                  void* cbdata);
    static int MockedFailure4SendBuffer(orte_process_name_t* peer,
                                  struct opal_buffer_t* buffer,
                                  orte_rml_tag_t tag,
                                  orte_rml_buffer_callback_fn_t cbfunc,
                                  void* cbdata);
    static int MockedFailure5SendBuffer(orte_process_name_t* peer,
                                  struct opal_buffer_t* buffer,
                                  orte_rml_tag_t tag,
                                  orte_rml_buffer_callback_fn_t cbfunc,
                                  void* cbdata);
    static int PrintResultsFromStream(uint32_t results_count, int stream_index,
                                      double start_time, double stop_time);
};
#endif // OCTL_TESTS_H
