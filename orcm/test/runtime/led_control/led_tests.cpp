/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include <iostream>

#include "orcm/runtime/led_control.h"

extern "C" {
#include "orcm/runtime/led_control_interface.h"
};

#include "led_tests.h"
#include "led_tests_mocking.h"

#define HOSTNAME (char*)("hostame")
#define USER (char*)("user")
#define PASS (char*)("password")
#define AUTH 2
#define PRIV 4

using namespace std;

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
    LedControl lc;
    ASSERT_EQ(0, lc.disableChassisID());
    ASSERT_FALSE(is_remote_node);
}

TEST_F(ut_control_led_tests, new_led_object_remote){
    init_led_control(HOSTNAME, USER, PASS, AUTH, PRIV);
    ASSERT_EQ(0, disable_chassis_id());
    ASSERT_TRUE(is_remote_node);
    fini_led_control();
}

TEST_F(ut_control_led_tests, enable_chassis_id_n_seconds){
    init_led_control(HOSTNAME, USER, PASS, AUTH, PRIV);
    ASSERT_EQ(0, enable_chassis_id_with_timeout(20));
    ASSERT_EQ(LED_TEMPORARY_ON, get_chassis_id_state());
    fini_led_control();
}

TEST_F(ut_control_led_tests, enable_chassis_id_indefinitely){
    init_led_control(HOSTNAME, USER, PASS, AUTH, PRIV);
    ASSERT_EQ(0, enable_chassis_id());
    ASSERT_EQ(LED_INDEFINITE_ON, get_chassis_id_state());
    fini_led_control();
}

TEST_F(ut_control_led_tests, disable_chassis_id){
    init_led_control(HOSTNAME, USER, PASS, AUTH, PRIV);
    ASSERT_EQ(0, disable_chassis_id());
    ASSERT_EQ(LED_OFF, get_chassis_id_state());
    fini_led_control();
}

TEST_F(ut_control_led_tests, wrong_remote_data){
    init_led_control(HOSTNAME,(char*)(""), PASS, AUTH, PRIV);
    ASSERT_NE(0, disable_chassis_id());
    fini_led_control();
}

TEST_F(ut_control_led_tests, chassis_status_query_not_supported){
    init_led_control(HOSTNAME, USER, PASS, AUTH, PRIV);
    is_supported = false;
    ASSERT_EQ(-1, get_chassis_id_state());
    fini_led_control();
}
