/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ethtest_tests.h"

// OPAL
#include "opal/dss/dss.h"

// ORCM
#include "orcm/mca/diag/diag.h"
#include "orcm/mca/diag/base/base.h"
#include "orcm/mca/diag/ethtest/diag_ethtest.h"
// C++
#include <iostream>
#include <iomanip>

// Fixture
using namespace std;

extern "C" {

};
orte_rml_module_send_buffer_nb_fn_t ut_ethtest_tests::saved_send_buffer = NULL;
orcm_db_base_API_store_new_fn_t ut_ethtest_tests::saved_store_new = NULL;

void ut_ethtest_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();
    orcm_diag_ethtest_module.init();
    saved_send_buffer = orte_rml.send_buffer_nb;
    saved_store_new = orcm_db.store_new;
}

void ut_ethtest_tests::TearDownTestCase()
{
    // Release OPAL level resources
    opal_dss_close();
    orte_rml.send_buffer_nb = saved_send_buffer;
    orcm_db.store_new = saved_store_new;
    ethtest_mocking.opal_dss_pack_callback = NULL;
    saved_store_new = NULL;
    saved_send_buffer = NULL;
}

int ut_ethtest_tests::OpalDssPack(opal_buffer_t* buffer,
                                  const void* src,
                                  int32_t num_vals,
                                  opal_data_type_t type)
{
    int rc = __real_opal_dss_pack(buffer, src, num_vals, type);
    if(ethtest_mocking.fail_at == ethtest_mocking.current_execution++)
        return ORCM_ERR_PACK_FAILURE;
    return rc;
}

int ut_ethtest_tests::SendBuffer(orte_process_name_t* peer,
                              struct opal_buffer_t* buffer,
                              orte_rml_tag_t tag,
                              orte_rml_buffer_callback_fn_t cbfunc,
                              void* cbdata)
{
    ethtest_mocking.finish_execution = 1;
    return ORTE_SUCCESS;
}

void ut_ethtest_tests::DbStoreNew(int dbhandle,
                                  orcm_db_data_type_t data_type,
                                  opal_list_t *input,
                                  opal_list_t *ret,
                                  orcm_db_callback_fn_t cbfunc,
                                  void *cbdata)
{
    return;
}

TEST_F(ut_ethtest_tests, ethtest_log_negative_test_0_parameters)
{
    int rc = 0;
    opal_buffer_t *data = OBJ_NEW(opal_buffer_t);

    rc = orcm_diag_ethtest_module.log(data);
    EXPECT_NE(ORTE_SUCCESS, rc);
}

TEST_F(ut_ethtest_tests, ethtest_log_negative_test_1_parameter)
{
    struct timeval now;
    int rc = 0;
    opal_buffer_t *data = OBJ_NEW(opal_buffer_t);
    gettimeofday(&now, NULL);

    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);

    rc = orcm_diag_ethtest_module.log(data);
    EXPECT_NE(ORTE_SUCCESS, rc);
}

TEST_F(ut_ethtest_tests, ethtest_log_negative_test_2_parameters)
{
    struct timeval now;
    int rc = 0;
    opal_buffer_t *data = OBJ_NEW(opal_buffer_t);
    gettimeofday(&now, NULL);

    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);
    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);

    rc = orcm_diag_ethtest_module.log(data);
    EXPECT_NE(ORTE_SUCCESS, rc);
}

TEST_F(ut_ethtest_tests, ethtest_log_negative_test_3_parameters)
{
    struct timeval now;
    int rc = 0;
    char *hostname = strdup("fakehost");
    opal_buffer_t *data = OBJ_NEW(opal_buffer_t);
    gettimeofday(&now, NULL);

    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);
    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);
    opal_dss.pack(data, &hostname, 1, OPAL_STRING);

    rc = orcm_diag_ethtest_module.log(data);
    EXPECT_NE(ORTE_SUCCESS, rc);
}

TEST_F(ut_ethtest_tests, ethtest_log_negative_test_4_parameters)
{
    struct timeval now;
    int rc = 0;
    char *hostname = strdup("fakehost");
    char *result = strdup("FAIL");
    opal_buffer_t *data = OBJ_NEW(opal_buffer_t);
    gettimeofday(&now, NULL);

    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);
    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);
    opal_dss.pack(data, &hostname, 1, OPAL_STRING);
    opal_dss.pack(data, &result, 1, OPAL_STRING);

    rc = orcm_diag_ethtest_module.log(data);
    EXPECT_NE(ORTE_SUCCESS, rc);
}

TEST_F(ut_ethtest_tests, ethtest_log_negative_test_5_parameters)
{
    struct timeval now;
    int rc = 0;
    char *hostname = strdup("fakehost");
    char *result = strdup("FAIL");
    int result_num = 1;
    opal_buffer_t *data = OBJ_NEW(opal_buffer_t);
    gettimeofday(&now, NULL);
    orcm_db.store_new = DbStoreNew;
    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);
    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);
    opal_dss.pack(data, &hostname, 1, OPAL_STRING);
    opal_dss.pack(data, &result, 1, OPAL_STRING);
    opal_dss.pack(data, &result_num, 1, OPAL_INT);

    rc = orcm_diag_ethtest_module.log(data);

    EXPECT_NE(ORTE_SUCCESS, rc);
}

TEST_F(ut_ethtest_tests, ethtest_log_negative_test_6_parameters)
{
    struct timeval now;
    int rc = 0;
    char *hostname = strdup("fakehost");
    char *result = strdup("FAIL");
    char *subtype = strdup("SUBTYPE");
    int result_num = 1;
    opal_buffer_t *data = OBJ_NEW(opal_buffer_t);
    gettimeofday(&now, NULL);
    orcm_db.store_new = DbStoreNew;
    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);
    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);
    opal_dss.pack(data, &hostname, 1, OPAL_STRING);
    opal_dss.pack(data, &result, 1, OPAL_STRING);
    opal_dss.pack(data, &result_num, 1, OPAL_INT);
    opal_dss.pack(data, &subtype, 1, OPAL_STRING);

    rc = orcm_diag_ethtest_module.log(data);

    EXPECT_NE(ORTE_SUCCESS, rc);
}

TEST_F(ut_ethtest_tests, ethtest_log_negative_test_7_parameters)
{
    struct timeval now;
    int rc = 0;
    char *hostname = strdup("fakehost");
    char *result = strdup("FAIL");
    char *subtype = strdup("SUBTYPE");
    int result_num = 1;
    opal_buffer_t *data = OBJ_NEW(opal_buffer_t);
    gettimeofday(&now, NULL);
    orcm_db.store_new = DbStoreNew;
    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);
    opal_dss.pack(data, &now, 1, OPAL_TIMEVAL);
    opal_dss.pack(data, &hostname, 1, OPAL_STRING);
    opal_dss.pack(data, &result, 1, OPAL_STRING);
    opal_dss.pack(data, &result_num, 1, OPAL_INT);
    opal_dss.pack(data, &subtype, 1, OPAL_STRING);
    opal_dss.pack(data, &result_num, 1, OPAL_INT);

    rc = orcm_diag_ethtest_module.log(data);

    EXPECT_EQ(ORTE_SUCCESS, rc);
}

TEST_F(ut_ethtest_tests, ethtest_run_negative_pack_test)
{
    int rc = 0;
    opal_buffer_t *caddy = OBJ_NEW(opal_buffer_t);

    ethtest_mocking.opal_dss_pack_callback = OpalDssPack;
    orte_rml.send_buffer_nb = SendBuffer;
    ethtest_mocking.finish_execution = 0;

    for(ethtest_mocking.fail_at = 1; ethtest_mocking.fail_at <= 10; ethtest_mocking.fail_at++){
        ethtest_mocking.current_execution = 1;
        orcm_diag_ethtest_module.run(0, 0, caddy);

    }

    ethtest_mocking.opal_dss_pack_callback = NULL;
    EXPECT_NE(0, ethtest_mocking.finish_execution);
}

