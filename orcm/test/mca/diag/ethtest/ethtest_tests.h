/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ETHTEST_TESTS_H
#define ETHTEST_TESTS_H

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

#include "gtest/gtest.h"
#include "ethtest_tests_mocking.h"

class ut_ethtest_tests: public testing::Test
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
        static void DbStoreNew(int dbhandle,
                               orcm_db_data_type_t data_type,
                               opal_list_t *input,
                               opal_list_t *ret,
                               orcm_db_callback_fn_t cbfunc,
                               void *cbdata);
        static int OpalDssPack(opal_buffer_t* buffer,
                               const void* src,
                               int32_t num_vals,
                               opal_data_type_t type);
        static orte_rml_module_send_buffer_nb_fn_t saved_send_buffer;
        static orcm_db_base_API_store_new_fn_t saved_store_new;

}; // class
#endif // ETHTEST_TESTS_H
