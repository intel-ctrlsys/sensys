/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

//#include "opal/dss/dss.h"

#include <iostream>
#include <string>

#include "orcm/tools/octl/common.h"

#include "octl_led_tests.h"
#define MAX_ELEMENTS 10

using namespace std;

orte_rml_module_send_buffer_nb_fn_t ut_octl_led_tests::saved_send_buffer = NULL;
orte_rml_module_recv_buffer_nb_fn_t ut_octl_led_tests::saved_recv_buffer = NULL;
orte_rml_module_recv_cancel_fn_t ut_octl_led_tests::saved_recv_cancel = NULL;
orcm_cmd_server_flag_t ut_octl_led_tests::command = 0;
orcm_cmd_server_flag_t ut_octl_led_tests::subcommand = 0;
int ut_octl_led_tests::next_state = LED_OFF;
int ut_octl_led_tests::next_return = ORCM_SUCCESS;
bool ut_octl_led_tests::is_send_expected_to_succeed = true;
bool ut_octl_led_tests::is_unpack_state_expected_to_succeed = true;
bool ut_octl_led_tests::is_get_hostname_proc_expected_to_succeed = true;

void ut_octl_led_tests::SetUp()
{
    saved_send_buffer = orte_rml.send_buffer_nb;
    orte_rml.send_buffer_nb = SendBuffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;
    orte_rml.recv_buffer_nb = RecvBuffer;

    saved_recv_cancel = orte_rml.recv_cancel;
    orte_rml.recv_cancel = RecvCancel;

    octl_mocking.orcm_cfgi_base_get_hostname_proc_callback = GetHostnameProc;

    command = 0;
    subcommand = 0;
    next_state = LED_OFF;
    next_return = ORCM_SUCCESS;
    is_send_expected_to_succeed = true;
    is_unpack_state_expected_to_succeed = true;
    is_mca_base_framework_open_expected_to_succeed = true;
    is_orcm_parser_base_select_expected_to_succeed = true;
    is_get_bmc_info_expected_to_succeed = true;
    is_orcm_logical_group_parse_array_string_expected_to_succeed = true;
    orcm_cfgi_base.config_file = strdup("orcm-site.xml");
}

void ut_octl_led_tests::TearDown()
{
    octl_mocking.orcm_cfgi_base_get_hostname_proc_callback = NULL;

    orte_rml.recv_buffer_nb = saved_recv_buffer;
    saved_recv_buffer = NULL;

    orte_rml.send_buffer_nb = saved_send_buffer;
    saved_send_buffer = NULL;

    orte_rml.recv_cancel = saved_recv_cancel;
    saved_recv_cancel = NULL;
    free(orcm_cfgi_base.config_file);
}

void ut_octl_led_tests::SetUpTestCase()
{
    opal_init_test();
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();
}

void ut_octl_led_tests::TearDownTestCase()
{
    octl_mocking.orcm_cfgi_base_get_hostname_proc_callback = NULL;

    orte_rml.recv_buffer_nb = saved_recv_buffer;
    saved_recv_buffer = NULL;

    orte_rml.send_buffer_nb = saved_send_buffer;
    saved_send_buffer = NULL;

    orte_rml.recv_cancel = saved_recv_cancel;
    saved_recv_cancel = NULL;

    is_send_expected_to_succeed = true;
    is_unpack_state_expected_to_succeed = true;
    is_mca_base_framework_open_expected_to_succeed = true;
    is_orcm_parser_base_select_expected_to_succeed = true;
    is_get_bmc_info_expected_to_succeed = true;
    is_orcm_logical_group_parse_array_string_expected_to_succeed = true;

    // Release OPAL level resources
    opal_dss_close();
}

int ut_octl_led_tests::SendBuffer(orte_process_name_t* peer,
                      struct opal_buffer_t* buffer,
                      orte_rml_tag_t tag,
                      orte_rml_buffer_callback_fn_t cbfunc,
                      void* cbdata){
    int elements=1;
    int seconds=0;
    char* noderaw;
    int rc = ORCM_SUCCESS;
    rc = opal_dss.unpack(buffer, &command, &elements, ORCM_CMD_SERVER_T);
    EXPECT_EQ(ORCM_SUCCESS, rc);
    rc = opal_dss.unpack(buffer, &subcommand, &elements, ORCM_CMD_SERVER_T);
    EXPECT_EQ(ORCM_SUCCESS, rc);
    rc = opal_dss.unpack(buffer, &noderaw, &elements, OPAL_STRING);
    EXPECT_EQ(ORCM_SUCCESS, rc);
    if (ORCM_SET_CHASSIS_ID_TEMPORARY_ON == subcommand){
        rc = opal_dss.unpack(buffer, &seconds, &elements, OPAL_INT);
        EXPECT_EQ(ORCM_SUCCESS, rc);
    }

    rc = opal_dss.pack(buffer, &command, elements, ORCM_CMD_SERVER_T);
    EXPECT_EQ(ORCM_SUCCESS, rc);
    rc = opal_dss.pack(buffer, &subcommand, elements, ORCM_CMD_SERVER_T);
    EXPECT_EQ(ORCM_SUCCESS, rc);
    rc = opal_dss.pack(buffer, &noderaw, elements, OPAL_STRING);
    EXPECT_EQ(ORCM_SUCCESS, rc);
    if (ORCM_SET_CHASSIS_ID_TEMPORARY_ON == subcommand){
        rc = opal_dss.pack(buffer, &seconds, elements, OPAL_INT);
        EXPECT_EQ(ORCM_SUCCESS, rc);
    }

    if (!is_send_expected_to_succeed)
        return ORCM_ERROR;
    return rc;
}

void ut_octl_led_tests::RecvBuffer(orte_process_name_t* peer,
                       orte_rml_tag_t tag,
                       bool persistent,
                       orte_rml_buffer_callback_fn_t cbfunc,
                       void* cbdata){
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    xfer->name.jobid = 0;
    xfer->name.vpid = 0;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    EXPECT_EQ(ORCM_SUCCESS, opal_dss.pack(&xfer->data, &next_return, 1, OPAL_INT));
    if (is_unpack_state_expected_to_succeed)
        EXPECT_EQ(ORCM_SUCCESS, opal_dss.pack(&xfer->data, &next_state, 1, OPAL_INT));
    xfer->active = false;
    OBJ_RELEASE(xfer);
}

void ut_octl_led_tests::RecvCancel(orte_process_name_t* peer, orte_rml_tag_t tag){
}

int ut_octl_led_tests::GetHostnameProc(char* hostname, orte_process_name_t* proc)
{
    return is_get_hostname_proc_expected_to_succeed ? ORCM_SUCCESS : ORCM_ERROR;
}

TEST_F(ut_octl_led_tests, state_expected_tag){
    const char* cmdlist[4] = {"chassis-id", "state", "node[2:1-10]", NULL};
    next_state = -1;
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_state((char**)cmdlist));
    ASSERT_EQ(ORCM_GET_CHASSIS_ID, ut_octl_led_tests::command);
    ASSERT_EQ(ORCM_GET_CHASSIS_ID_STATE, ut_octl_led_tests::subcommand);
}

TEST_F(ut_octl_led_tests, state_indefinite_on){
    const char* cmdlist[4] = {"chassis-id", "state", "node[2:1-10]", NULL};
    next_state = LED_INDEFINITE_ON;
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_state((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, state_on){
    const char* cmdlist[4] = {"chassis-id", "state", "node[2:1-10]", NULL};
    next_state = LED_TEMPORARY_ON;
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_state((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, state_off){
    const char* cmdlist[4] = {"chassis-id", "state", "node[2:1-10]", NULL};
    next_state = LED_OFF;
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_state((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, disable_expected_tag){
    const char* cmdlist[4] = {"chassis-id", "disable", "node[2:1-10]", NULL};
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_off((char**)cmdlist));
    ASSERT_EQ(ORCM_SET_CHASSIS_ID, ut_octl_led_tests::command);
    ASSERT_EQ(ORCM_SET_CHASSIS_ID_OFF, ut_octl_led_tests::subcommand);
}

TEST_F(ut_octl_led_tests, enable_expected_tag_01){
    const char* cmdlist[5] = {"chassis-id", "enable", "10", "node[2:1-10]", NULL};
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_on((char**)cmdlist));
    ASSERT_EQ(ORCM_SET_CHASSIS_ID, ut_octl_led_tests::command);
    ASSERT_EQ(ORCM_SET_CHASSIS_ID_TEMPORARY_ON, ut_octl_led_tests::subcommand);
}

TEST_F(ut_octl_led_tests, enable_expected_tag_02){
    const char* cmdlist[4] = {"chassis-id", "enable", "node[2:1-10]", NULL};
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_on((char**)cmdlist));
    ASSERT_EQ(ORCM_SET_CHASSIS_ID, ut_octl_led_tests::command);
    ASSERT_EQ(ORCM_SET_CHASSIS_ID_ON, ut_octl_led_tests::subcommand);
}

TEST_F(ut_octl_led_tests, get_chassis_id_state_more_arguments_than_expected){
    const char* cmdlist[5] = {"chassis-id", "state", "node", "extra_arg", NULL};
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, orcm_octl_chassis_id_state((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, get_chassis_id_state_less_arguments_than_expected){
    const char* cmdlist[3] = {"chassis-id", "state", NULL};
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, orcm_octl_chassis_id_state((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, get_chassis_id_state_null_arguments){
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, orcm_octl_chassis_id_state(NULL));
}

TEST_F(ut_octl_led_tests, set_chassis_id_off_more_arguments_than_expected){
    const char* cmdlist[5] = {"chassis-id", "disable", "node", "extra_arg", NULL};
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, orcm_octl_chassis_id_off((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, set_chassis_id_off_less_arguments_than_expected){
    const char* cmdlist[3] = {"chassis-id", "disable", NULL};
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, orcm_octl_chassis_id_off((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, set_chassis_id_off_null_arguments){
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, orcm_octl_chassis_id_off(NULL));
}

TEST_F(ut_octl_led_tests, set_chassis_id_on_more_arguments_than_expected){
    const char* cmdlist[6] = {"chassis-id", "enable", "10", "node", "extra_arg", NULL};
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, orcm_octl_chassis_id_on((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, set_chassis_id_on_less_arguments_than_expected){
    const char* cmdlist[3] = {"chassis-id", "enable", NULL};
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, orcm_octl_chassis_id_on((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, set_chassis_id_on_null_arguments){
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, orcm_octl_chassis_id_on(NULL));
}

TEST_F(ut_octl_led_tests, get_chassis_id_state_ipmi_conflict){
    const char* cmdlist[4] = {"chassis-id", "state", "node[2:1-10]", NULL};
    next_state = LED_INDEFINITE_ON;
    next_return = ORCM_ERR_IPMI_CONFLICT;
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_state((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, get_chassis_id_state_no_bmc_info){
    const char* cmdlist[4] = {"chassis-id", "state", "node[2:1-10]", NULL};
    next_state = LED_INDEFINITE_ON;
    next_return = ORCM_ERR_BMC_INFO_NOT_FOUND;
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_state((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, get_chassis_id_state_not_success){
    const char* cmdlist[4] = {"chassis-id", "state", "node[2:1-10]", NULL};
    next_state = LED_INDEFINITE_ON;
    next_return = ORCM_ERROR;
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_state((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, get_chassis_id_state_no_hostname){
    const char* cmdlist[4] = {"chassis-id", "state", "", NULL};
    ASSERT_EQ(ORCM_ERROR, orcm_octl_chassis_id_state((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, set_chassis_id_on_send_failed){
    const char* cmdlist[5] = {"chassis-id", "enable", "10", "node[2:1-10]", NULL};
    is_send_expected_to_succeed = false;
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_on((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, set_chassis_id_on_open_parser_fw_failed){
    const char* cmdlist[4] = {"chassis-id", "enable", "node[2:1-10]", NULL};
    is_mca_base_framework_open_expected_to_succeed = false;
    ASSERT_EQ(ORCM_ERROR, orcm_octl_chassis_id_on((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, set_chassis_id_on_open_parser_fw_failed_on_select){
    const char* cmdlist[4] = {"chassis-id", "enable", "node[2:1-10]", NULL};
    is_orcm_parser_base_select_expected_to_succeed = false;
    ASSERT_EQ(ORCM_ERROR, orcm_octl_chassis_id_on((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, set_chassis_id_off_unable_to_get_bmc_info){
    const char* cmdlist[4] = {"chassis-id", "disable", "node[2:1-10]", NULL};
    is_get_bmc_info_expected_to_succeed = false;
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_off((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, get_chassis_id_state_unable_to_unpack_state){
    const char* cmdlist[4] = {"chassis-id", "state", "node[2:1-10]", NULL};
    is_unpack_state_expected_to_succeed = false;
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_state((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, get_chassis_id_state_wrong_hostname_proc){
    const char* cmdlist[4] = {"chassis-id", "state", "node[2:1-10]", NULL};
    is_get_hostname_proc_expected_to_succeed = false;
    ASSERT_EQ(ORCM_SUCCESS, orcm_octl_chassis_id_state((char**)cmdlist));
}

TEST_F(ut_octl_led_tests, get_chassis_id_state_wrong_group_parse){
    const char* cmdlist[4] = {"chassis-id", "state", "node[2:1-10]", NULL};
    is_orcm_logical_group_parse_array_string_expected_to_succeed = false;
    ASSERT_EQ(ORCM_ERROR, orcm_octl_chassis_id_state((char**)cmdlist));
}
