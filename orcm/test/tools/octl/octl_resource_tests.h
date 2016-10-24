/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef OCTL_RESOURCE_TESTS_H
#define OCTL_RESOURCE_TESTS_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    // OPAL
    #include "opal/dss/dss.h"
    // ORTE
    #include "orte/mca/rml/rml.h"
    #include "orcm/util/utils.h"
    #include "orcm/tools/octl/common.h"
#ifdef __cplusplus
};
#endif // __cplusplus

#include "gtest/gtest.h"
#include "octl_mocking.h"
#include "octl_tests.h"

class ut_octl_resource_tests: public ut_octl_tests
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
        static void RecvBufferTimeOut(orte_process_name_t* peer,
                                      orte_rml_tag_t tag,
                                      bool persistent,
                                      orte_rml_buffer_callback_fn_t cbfunc,
                                      void* cbdata);
        static void RecvBufferNodeErr(orte_process_name_t* peer,
                                      orte_rml_tag_t tag,
                                      bool persistent,
                                      orte_rml_buffer_callback_fn_t cbfunc,
                                      void* cbdata);
        static void RecvBufferOneNode(orte_process_name_t* peer,
                                      orte_rml_tag_t tag,
                                      bool persistent,
                                      orte_rml_buffer_callback_fn_t cbfunc,
                                      void* cbdata);
        static void RecvBufferInstNode(orte_process_name_t* peer,
                                       orte_rml_tag_t tag,
                                       bool persistent,
                                       orte_rml_buffer_callback_fn_t cbfunc,
                                       void* cbdata);
        static int UnpackBufferNode(opal_buffer_t *buffer, void *dest,
                                    int32_t *max_num_values,
                                    opal_data_type_t type);
        static int PackBufferCmd(opal_buffer_t *buffer, const void *src,
                                 int32_t num_values,
                                 opal_data_type_t type);
};


#endif
