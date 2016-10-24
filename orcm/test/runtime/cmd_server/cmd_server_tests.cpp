/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include <iostream>

extern "C" {
    #include "orte/mca/notifier/base/base.h"
    #include "orcm/util/utils.h"

    #include "orcm/tools/octl/common.h"
    #include "orcm/runtime/orcm_cmd_server.h"

    #include "orcm/mca/sensor/base/sensor_private.h"
}

#include "cmd_server_tests.h"
#include "cmd_server_tests_mocking.h"

using namespace std;

orte_rml_module_send_buffer_nb_fn_t ut_cmd_server_tests::saved_send_buffer = NULL;
orte_rml_module_recv_buffer_nb_fn_t ut_cmd_server_tests::saved_recv_buffer = NULL;
orte_rml_module_recv_cancel_fn_t ut_cmd_server_tests::saved_recv_cancel = NULL;

void ut_cmd_server_tests::SetUp(){
    opal_init_test();
    opal_dss_register_vars();
    opal_dss_open();

    // Mock orte_rml functions
    saved_send_buffer = orte_rml.send_buffer_nb;
    saved_recv_buffer = orte_rml.recv_buffer_nb;
    saved_recv_cancel = orte_rml.recv_cancel;
    orte_rml.send_buffer_nb = SendBuffer;
    orte_rml.recv_buffer_nb = RecvBuffer;
    orte_rml.recv_cancel = RecvCancel;

    // Mock flags
    is_recv_buffer_nb_called = false;
    is_recv_cancel_called = false;
    is_get_notifier_policy_expected_to_succeed = true;
    is_orte_notifier_base_get_config_expected_to_succeed = true;
    is_get_bmc_info_expected_to_succeed = true;
    is_expected_to_succeed = true;
    is_send_buffer_expected_to_succeed = true;
    is_hostname_append_expected_to_succeed = true;
    is_chassis_append_expected_to_succeed = true;
    next_state = 0x01;
    next_response = ORCM_SUCCESS;

    cout<<"Starting test"<<endl;
}

void ut_cmd_server_tests::TearDown(){
    cout<<"Finishing test"<<endl;

    // Mock orte_rml functions
    orte_rml.send_buffer_nb = saved_send_buffer;
    orte_rml.recv_buffer_nb = saved_recv_buffer;
    orte_rml.recv_cancel = saved_recv_cancel;
    saved_send_buffer = NULL;
    saved_recv_buffer = NULL;
    saved_recv_cancel = NULL;

    opal_dss_close();
}

int ut_cmd_server_tests::SendBuffer(orte_process_name_t* peer,
                      struct opal_buffer_t* buffer,
                      orte_rml_tag_t tag,
                      orte_rml_buffer_callback_fn_t cbfunc,
                      void* cbdata){
    int cnt = 1;
    char* error = NULL;
    int res = ORCM_ERROR;

    if (!is_send_buffer_expected_to_succeed)
        return ORCM_ERROR;

    EXPECT_EQ(ORCM_SUCCESS, opal_dss.unpack(buffer, &res, &cnt, OPAL_INT));
    EXPECT_EQ(next_response, res);
    return ORCM_SUCCESS;
}

void ut_cmd_server_tests::RecvBuffer(orte_process_name_t* peer,
                       orte_rml_tag_t tag,
                       bool persistent,
                       orte_rml_buffer_callback_fn_t cbfunc,
                       void* cbdata){
    is_recv_buffer_nb_called = true;
}

void ut_cmd_server_tests::RecvCancel(orte_process_name_t* peer, orte_rml_tag_t tag){
    is_recv_cancel_called = true;
}

int ut_cmd_server_tests::GetHostnameProc(char* hostname, orte_process_name_t* proc)
{
    return ORTE_SUCCESS;
}

// Tries to initialize the cmd_server
TEST_F(ut_cmd_server_tests, orcm_cmd_server_init){
    ASSERT_EQ(ORCM_SUCCESS, orcm_cmd_server_init());
    ASSERT_TRUE(is_recv_buffer_nb_called);
    orcm_cmd_server_finalize();
}

// Tries to initialize twice the cmd_server
TEST_F(ut_cmd_server_tests, orcm_cmd_server_init_call_twice){
    ASSERT_EQ(ORCM_SUCCESS, orcm_cmd_server_init());
    ASSERT_TRUE(is_recv_buffer_nb_called);
    ASSERT_EQ(ORCM_SUCCESS, orcm_cmd_server_init());
    orcm_cmd_server_finalize();
}

// Tries to finalize the cmd_server without previous initialization
TEST_F(ut_cmd_server_tests, orcm_cmd_server_finalize_only){
    orcm_cmd_server_finalize();
    ASSERT_FALSE(is_recv_cancel_called);
}

// Fails to unpack command from an empty buffer
TEST_F(ut_cmd_server_tests, orcm_cmd_server_unpack_command_failure){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    buffer = OBJ_NEW(opal_buffer_t);
    next_response = ORCM_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Fails to unpack a subcommand from a buffer with command only
TEST_F(ut_cmd_server_tests, orcm_cmd_server_unpack_subcommand_failure){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    // Pack the command only
    orcm_cmd_server_flag_t command = ORCM_SET_NOTIFIER_COMMAND;
    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));

    next_response = ORCM_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Gets a wrong command value (-1)
TEST_F(ut_cmd_server_tests, orcm_cmd_server_wrong_command){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    // Pack a wrong command
    orcm_cmd_server_flag_t command = -1;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_NOTIFIER_POLICY_COMMAND;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));

    next_response = ORCM_ERROR;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Get a wrong SET_NOTIFIER subcommand value (-1)
TEST_F(ut_cmd_server_tests, orcm_cmd_server_wrong_set_notifier_subcommand){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    // Pack a wrong subcommand
    orcm_cmd_server_flag_t command = ORCM_SET_NOTIFIER_COMMAND;
    orcm_cmd_server_flag_t subcommand = -1;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));

    next_response = ORCM_ERROR;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// SET POLICY subcommand, Failed to unpack severity
TEST_F(ut_cmd_server_tests, orcm_cmd_server_set_policy_subcommand_unpack_severity){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_NOTIFIER_COMMAND;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_NOTIFIER_POLICY_COMMAND;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));

    next_response = ORCM_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// SET POLICY subcommand, Failed to unpack action
TEST_F(ut_cmd_server_tests, orcm_cmd_server_set_policy_subcommand_unpack_action){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_NOTIFIER_COMMAND;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_NOTIFIER_POLICY_COMMAND;
    orte_notifier_severity_t severity = ORTE_NOTIFIER_EMERG;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &severity, 1, ORTE_NOTIFIER_SEVERITY_T));

    next_response = ORCM_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// SET POLICY subcommand all success
TEST_F(ut_cmd_server_tests, orcm_cmd_server_set_policy_subcommand_success){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_NOTIFIER_COMMAND;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_NOTIFIER_POLICY_COMMAND;
    orte_notifier_severity_t severity = ORTE_NOTIFIER_EMERG;
    const char* action = "dummy_action";

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &severity, 1, ORTE_NOTIFIER_SEVERITY_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &action, 1, OPAL_STRING));

    next_response = ORCM_SUCCESS;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// SET SMTP subcommand, Failed to unpack opal_value
TEST_F(ut_cmd_server_tests, orcm_cmd_server_set_smtp_unpack_opal_value){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_NOTIFIER_COMMAND;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_NOTIFIER_SMTP_COMMAND;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));

    next_response = ORCM_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// SET SMTP subcommand all success
TEST_F(ut_cmd_server_tests, orcm_cmd_server_set_smtp_success){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_NOTIFIER_COMMAND;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_NOTIFIER_SMTP_COMMAND;
    opal_value_t* kv = orcm_util_load_opal_value((char*)"test", (void*)"test", OPAL_STRING);

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &kv, 1, OPAL_VALUE));

    next_response = ORCM_SUCCESS;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(kv);
    ORCM_RELEASE(buffer);
}

// Get a wrong GET_NOTIFIER subcommand value (-1)
TEST_F(ut_cmd_server_tests, orcm_cmd_server_wrong_get_notifier_subcommand){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    // Pack a wrong subcommand
    orcm_cmd_server_flag_t command = ORCM_GET_NOTIFIER_COMMAND;
    orcm_cmd_server_flag_t subcommand = -1;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));

    next_response = ORCM_ERROR;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// GET policy subcommand, make get_notifier_policy return NULL
TEST_F(ut_cmd_server_tests, orcm_cmd_server_get_policy_failure){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_GET_NOTIFIER_COMMAND;
    orcm_cmd_server_flag_t subcommand = ORCM_GET_NOTIFIER_POLICY_COMMAND;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));

    is_get_notifier_policy_expected_to_succeed = false;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// GET policy subcommand all success
TEST_F(ut_cmd_server_tests, orcm_cmd_server_get_policy_success){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_GET_NOTIFIER_COMMAND;
    orcm_cmd_server_flag_t subcommand = ORCM_GET_NOTIFIER_POLICY_COMMAND;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));

    next_response = ORCM_SUCCESS;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// GET SMTP subcommand all success
TEST_F(ut_cmd_server_tests, orcm_cmd_server_get_smtp_success){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_GET_NOTIFIER_COMMAND;
    orcm_cmd_server_flag_t subcommand = ORCM_GET_NOTIFIER_SMTP_COMMAND;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));

    next_response = ORCM_SUCCESS;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// GET SMTP subcommand failure, orte_notifier_base_get_config returns ORCM_ERROR
TEST_F(ut_cmd_server_tests, orcm_cmd_server_get_smtp_failure){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_GET_NOTIFIER_COMMAND;
    orcm_cmd_server_flag_t subcommand = ORCM_GET_NOTIFIER_SMTP_COMMAND;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));

    next_response = ORCM_ERROR;
    is_orte_notifier_base_get_config_expected_to_succeed = false;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis GET command, wrong subcommand
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_wrong_subcommand){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_GET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = -1;
    const char* hostname = "dummy_hostname";

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));

    next_response = ORCM_ERROR;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis SET command, unable to unpack hostname
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_unpack_hostname){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_CHASSIS_ID_OFF;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));

    next_response = ORCM_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis TEMPORARY ON subcommand, unable to unpack seconds
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_unpack_seconds){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_CHASSIS_ID_TEMPORARY_ON;
    const char* hostname = "dummy_hostname";

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));

    next_response = ORCM_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis SET command, get_bmc_info fails
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_no_bmc_info){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_CHASSIS_ID_OFF;
    const char* hostname = "dummy_hostname";

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));

    next_response = ORCM_ERR_BMC_INFO_NOT_FOUND;
    is_get_bmc_info_expected_to_succeed = false;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis SET command, wrong set subcommand
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_wrong_set_subcommand){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = -1;
    const char* hostname = "dummy_hostname";

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));

    next_response = ORCM_ERROR;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis GET command, received wrong status
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_bad_status){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_GET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_GET_CHASSIS_ID_STATE;
    const char* hostname = "dummy_hostname";

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));

    next_state = -1;
    next_response = ORCM_ERROR;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis GET command, get status all success
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_get_status_success){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_GET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_GET_CHASSIS_ID_STATE;
    const char* hostname = "dummy_hostname";

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));

    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis SET command, failed to turn off LED
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_set_off_failed){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_CHASSIS_ID_OFF;
    const char* hostname = "dummy_hostname";

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));

    is_expected_to_succeed = false;
    next_response = ORCM_ERROR;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis SET command, turn off LED success
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_set_off_success){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_CHASSIS_ID_OFF;
    const char* hostname = "dummy_hostname";

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));

    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis SET command, failed to turn on LED
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_set_on_failed){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_CHASSIS_ID_ON;
    const char* hostname = "dummy_hostname";

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));

    is_expected_to_succeed = false;
    next_response = ORCM_ERROR;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis SET command, turn on LED success
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_set_on_success){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_CHASSIS_ID_ON;
    const char* hostname = "dummy_hostname";

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));

    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis SET command, failed to turn temporary on LED
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_set_temp_on_failed){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_CHASSIS_ID_TEMPORARY_ON;
    const char* hostname = "dummy_hostname";
    int seconds = 0;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &seconds, 1, OPAL_INT));

    is_expected_to_succeed = false;
    next_response = ORCM_ERROR;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis SET command, turn temporary on LED success
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_set_temp_on_success){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_CHASSIS_ID_TEMPORARY_ON;
    const char* hostname = "dummy_hostname";
    int seconds = 0;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &seconds, 1, OPAL_INT));

    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis SET command successful, but send_buffer_nb failed
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_send_failed){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_CHASSIS_ID_TEMPORARY_ON;
    const char* hostname = "dummy_hostname";
    int seconds = 0;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &seconds, 1, OPAL_INT));

    is_send_buffer_expected_to_succeed = false;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis SET command successful, but store failed to append hostname to list
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_store_failed_01){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_CHASSIS_ID_TEMPORARY_ON;
    const char* hostname = "dummy_hostname";
    int seconds = 0;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &seconds, 1, OPAL_INT));

    is_hostname_append_expected_to_succeed = false;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}

// Chassis SET command successful, but store failed to append chassis-id-state to list
TEST_F(ut_cmd_server_tests, orcm_cmd_server_chassis_store_failed_02){
    int status = 0;
    orte_process_name_t* sender = NULL;
    opal_buffer_t* buffer = NULL;
    orte_rml_tag_t tag;
    void* cbdata = NULL;

    orcm_cmd_server_flag_t command = ORCM_SET_CHASSIS_ID;
    orcm_cmd_server_flag_t subcommand = ORCM_SET_CHASSIS_ID_TEMPORARY_ON;
    const char* hostname = "dummy_hostname";
    int seconds = 0;

    buffer = OBJ_NEW(opal_buffer_t);
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &command, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &subcommand, 1, ORCM_CMD_SERVER_T));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &hostname, 1, OPAL_STRING));
    ASSERT_EQ(ORCM_SUCCESS, opal_dss.pack(buffer, &seconds, 1, OPAL_INT));

    is_chassis_append_expected_to_succeed = false;
    orcm_cmd_server_recv(status, sender, buffer, tag, cbdata);

    ORCM_RELEASE(buffer);
}
