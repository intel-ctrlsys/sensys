/*
 * Copyright (c) 2016-2017 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include <iostream>

#include "orcm/util/led_control/ipmicmd_wrapper.h"
#include "orcm/util/led_control/led_control_interface.cpp"

#include "led_tests.h"
#include "led_tests_mocking.h"

#define HOSTNAME (char*)("hostame")
#define USER (char*)("user")
#define PASS (char*)("password")
#define AUTH 2
#define PRIV 4

using namespace std;

class IPMICmdWrapperMocked : public IPMICmdWrapper
{
public:
    IPMICmdWrapperMocked(void)
    {
        _ipmi_cmd = ipmi_cmd_mocked;
        _set_lan_options = set_lan_options_mocked;
        _ipmi_close = ipmi_close_mocked;
    }

    ~IPMICmdWrapperMocked(void){}

    void disableIPMICmdCalls(void)
    {
        _ipmi_cmd = NULL;
    }

    void disableSetLanOptionsCalls(void)
    {
        _set_lan_options = NULL;
    }

    void disableIPMICloseCalls(void)
    {
        _ipmi_close = NULL;
    }
};

class LedControlMocked : public LedControl
{
public:
    LedControlMocked(void)
    {
        ipmi = new IPMICmdWrapperMocked();
    }

    LedControlMocked(const char *hostname, const char *user, const char *pass,
        int auth, int priv): LedControl(hostname, user, pass, auth, priv)
    {
        ipmi = new IPMICmdWrapperMocked();
    }

    virtual ~LedControlMocked(void){}

    void setIPMIObject(IPMICmdWrapper *ipmi)
    {
        this->ipmi = ipmi;
    }
};

void ut_control_led_tests::SetUp(){
    cout<<"Starting test"<<endl;
    ipmi_open_session_count = 0;
    is_remote_node = false;
    is_supported = true;
}

void ut_control_led_tests::TearDown(){
    cout<<"Finishing test"<<endl;
    ASSERT_EQ(0, ipmi_open_session_count);
}

TEST_F(ut_control_led_tests, new_led_object){
    LedControlMocked lc;
    ASSERT_EQ(0, lc.disableChassisID());
    ASSERT_FALSE(is_remote_node);
}

TEST_F(ut_control_led_tests, new_led_object_destroy_ipmi_object){
    LedControlMocked lc;
    ASSERT_NO_THROW(lc.setIPMIObject(NULL));
}

TEST_F(ut_control_led_tests, new_led_object_no_ipmi_support_01){
    LedControlMocked lc;
    IPMICmdWrapperMocked *ipmi = new IPMICmdWrapperMocked();
    ipmi->disableIPMICmdCalls();
    ipmi->disableIPMICloseCalls();
    lc.setIPMIObject(ipmi);
    ASSERT_EQ(-1, lc.disableChassisID());
}

TEST_F(ut_control_led_tests, new_led_object_no_ipmi_support_02){
    LedControlMocked lc(HOSTNAME, USER, PASS, AUTH, PRIV);
    IPMICmdWrapperMocked *ipmi = new IPMICmdWrapperMocked();
    ipmi->disableSetLanOptionsCalls();
    lc.setIPMIObject(ipmi);
    ASSERT_EQ(-1, lc.disableChassisID());
}

TEST_F(ut_control_led_tests, new_dlopenhelper_no_library_name){
    class DlopenHelper_test : public DlopenHelper
    {
    public:
        DlopenHelper_test(void){};
        void* getHandler(){ return _handler; };
    };

    DlopenHelper_test dl;
    ASSERT_TRUE(NULL == dl.getHandler());
}

void ut_control_led_interface_tests::SetUp(){
    cout<<"Starting test"<<endl;
    ipmi_open_session_count = 0;
    is_remote_node = false;
    is_supported = true;
    init_led_control(HOSTNAME, USER, PASS, AUTH, PRIV);
}

void ut_control_led_interface_tests::TearDown(){
    cout<<"Finishing test"<<endl;
    fini_led_control();
    ASSERT_EQ(0, ipmi_open_session_count);
}

TEST_F(ut_control_led_interface_tests, new_led_object_remote){
    lc = new LedControlMocked(HOSTNAME, USER, PASS, AUTH, PRIV);
    ASSERT_EQ(0, disable_chassis_id());
    ASSERT_TRUE(is_remote_node);
}

TEST_F(ut_control_led_interface_tests, enable_chassis_id_n_seconds){
    lc = new LedControlMocked(HOSTNAME, USER, PASS, AUTH, PRIV);
    ASSERT_EQ(0, enable_chassis_id_with_timeout(20));
    ASSERT_EQ(LED_TEMPORARY_ON, get_chassis_id_state());
}

TEST_F(ut_control_led_interface_tests, enable_chassis_id_indefinitely){
    lc = new LedControlMocked(HOSTNAME, USER, PASS, AUTH, PRIV);
    ASSERT_EQ(0, enable_chassis_id());
    ASSERT_EQ(LED_INDEFINITE_ON, get_chassis_id_state());
}

TEST_F(ut_control_led_interface_tests, disable_chassis_id){
    lc = new LedControlMocked(HOSTNAME, USER, PASS, AUTH, PRIV);
    ASSERT_EQ(0, disable_chassis_id());
    ASSERT_EQ(LED_OFF, get_chassis_id_state());
}

TEST_F(ut_control_led_interface_tests, wrong_remote_data){
    lc = new LedControlMocked(HOSTNAME,(char*)(""), PASS, AUTH, PRIV);
    ASSERT_NE(0, disable_chassis_id());
}

TEST_F(ut_control_led_interface_tests, chassis_status_query_not_supported){
    lc = new LedControlMocked(HOSTNAME, USER, PASS, AUTH, PRIV);
    is_supported = false;
    ASSERT_EQ(-1, get_chassis_id_state());
}
