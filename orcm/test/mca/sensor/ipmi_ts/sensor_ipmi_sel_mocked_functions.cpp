/*
 * Copyright (c) 2015      Intel Corporation. All rights reserved.
 *
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmiutilAgent_mocks.h"
#include "orcm/mca/sensor/ipmi/sel_callback_defs.h"

// C
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <ipmicmd.h>
#include <memory.h>

// C++
#include <iostream>
#include <sstream>

// Data
extern bool FIRE_CRITICAL_ONLY;
extern std::map<supportedMocks, mockManager> mocks;
extern sensor_ipmi_sel_mocked_functions sel_mocking;

#include "sel_data.h"

using namespace std;

// Class implementation
sensor_ipmi_sel_mocked_functions::sensor_ipmi_sel_mocked_functions() :
    rename_fail_pattern_index(0), enable_rename_fail_pattern(false),
    fail_set_lan_options_once_flag(false), fail_ipmi_cmd_once_flag(false),
    capture_opal_output_verbose_flag(false), get_sdr_cache_result(0),
    find_sdr_next_result(0)
{
    username = "root";
    for(size_t i = 0; i < (int)sizeof(rename_fail_pattern); ++i) {
        rename_fail_pattern[i] = false;
    }
}

sensor_ipmi_sel_mocked_functions::~sensor_ipmi_sel_mocked_functions()
{
}

void sensor_ipmi_sel_mocked_functions::test_error_sink(int level, const std::string& msg)
{
    stringstream ss;
    ss << "LEVEL " << level << ": " << msg;
    error_list.push_back(ss.str());
}

void sensor_ipmi_sel_mocked_functions::test_ras_event_sink(const std::string& event, const std::string& hostname)
{
    stringstream ss;
    ss << hostname << "=" << event;
    ras_event_list.push_back(ss.str());
}

const std::vector<std::string>& sensor_ipmi_sel_mocked_functions::reported_errors()
{
    return error_list;
}

size_t sensor_ipmi_sel_mocked_functions::get_error_count()
{
    return error_list.size();
}

void sensor_ipmi_sel_mocked_functions::clear_errors()
{
    error_list.clear();
}

void sensor_ipmi_sel_mocked_functions::clear_ras_events()
{
    ras_event_list.clear();
}

const std::vector<std::string>& sensor_ipmi_sel_mocked_functions::logged_events()
{
    return ras_event_list;
}

size_t sensor_ipmi_sel_mocked_functions::get_ras_event_count()
{
    return ras_event_list.size();
}

void sensor_ipmi_sel_mocked_functions::setup_rename_fail_pattern(int n1, int n2, int n3)
{
    enable_rename_fail_pattern = true;
    rename_fail_pattern_index = 0;
    for(size_t i = 0; i < sizeof(rename_fail_pattern); ++i) {
        rename_fail_pattern[i] = false;
    }
    if(0 <= n1 && (int)sizeof(rename_fail_pattern) > n1) {
        rename_fail_pattern[n1] = true;
    }
    if(0 <= n2 && (int)sizeof(rename_fail_pattern) > n2) {
        rename_fail_pattern[n2] = true;
    }
    if(0 <= n3 && (int)sizeof(rename_fail_pattern) > n3) {
        rename_fail_pattern[n3] = true;
    }
}

void sensor_ipmi_sel_mocked_functions::end_rename_fail_pattern()
{
    enable_rename_fail_pattern = true;
    for(size_t i = 0; i < (int)sizeof(rename_fail_pattern); ++i) {
        rename_fail_pattern[i] = false;
    }
}

void sensor_ipmi_sel_mocked_functions::fail_set_lan_options_once()
{
    fail_set_lan_options_once_flag = true;
}

void sensor_ipmi_sel_mocked_functions::fail_ipmi_cmd_once()
{
    fail_ipmi_cmd_once_flag = true;
}

std::vector<std::string>& sensor_ipmi_sel_mocked_functions::get_opal_output_lines()
{
    return opal_output_lines;
}

void sensor_ipmi_sel_mocked_functions::set_opal_output_capture(bool mode)
{
    capture_opal_output_verbose_flag = mode;
}

// Mocked function overrides
int sensor_ipmi_sel_mocked_functions::mock_rename(const char* old_name, const char* new_name)
{
    if(true == enable_rename_fail_pattern) {
        bool current = false;
        if(sizeof(rename_fail_pattern) <= (size_t)rename_fail_pattern_index) {
            current = false;
        } else {
            current = rename_fail_pattern[rename_fail_pattern_index++];
        }
        if(true == current) {
            errno = ENOENT;
            return -1;
        } else {
            return __real_rename(old_name, new_name);
        }
    } else {
        return __real_rename(old_name, new_name);
    }
}

int sensor_ipmi_sel_mocked_functions::mock_set_lan_options(char* ip,
                                                           char* user,
                                                           char* password,
                                                           int auth_type,
                                                           int priv_level,
                                                           int cipher,
                                                           void* addr,
                                                           int len)
{
    (void)ip;
    (void)user;
    (void)password;
    (void)auth_type;
    (void)priv_level;
    (void)cipher;
    (void)addr;
    (void)len;
    if(true == fail_set_lan_options_once_flag) {
        fail_set_lan_options_once_flag = false;
        return -1;
    } else {
        return 0;
    }
}

int sensor_ipmi_sel_mocked_functions::mock_ipmi_cmd(unsigned short cmd,
                                                    unsigned char* request,
                                                    int request_size,
                                                    unsigned char* response,
                                                    int* response_size,
                                                    unsigned char* condition,
                                                    char debug)
{
    (void)cmd;
    (void)request;
    (void)request_size;
    (void)response;
    (void)response_size;
    (void)condition;
    (void)debug;
    if(true == fail_ipmi_cmd_once_flag) {
        fail_ipmi_cmd_once_flag = false;
        return -1;
    } else {
        unsigned int record_id = request[2];
        record_id |= (request[3] << 8);
        if(record_id == 0) {
            record_id = 1;
        }
        if(record_id == 0xffff) {
            return -1;
        }
        *response_size = 18;
        *condition = 0;
        memcpy(response, raw_sel_data[record_id], 18);
        return 0;
    }
}

int sensor_ipmi_sel_mocked_functions::mock_ipmi_close()
{
    return 0;
}

void sensor_ipmi_sel_mocked_functions::mock_opal_output_verbose(int level, int output_id, const char* message)
{
    if(true == capture_opal_output_verbose_flag) {
        opal_output_lines.push_back(string(message));
    } else {
        return __real_opal_output_verbose(level, output_id, message);
    }
}

int sensor_ipmi_sel_mocked_functions::mock_getlogin_r(char* user, size_t user_size)
{
    strncpy(user, username.c_str(), user_size);
    return 0;
}

__uid_t sensor_ipmi_sel_mocked_functions::mock_geteuid()
{
    if("root" == username) {
        return 0;
    } else {
        return 1000;
    }
}
