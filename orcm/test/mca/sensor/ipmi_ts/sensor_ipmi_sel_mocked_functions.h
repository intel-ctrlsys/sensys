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

#ifndef IPMI_TS_SEL_MOCKED_FUNCTIONS_H
#define IPMI_TS_SEL_MOCKED_FUNCTIONS_H

#include <string>
#include <vector>

class sensor_ipmi_sel_mocked_functions
{
    private:
        bool rename_fail_pattern[10];
        std::vector<std::string> error_list;
        std::vector<std::string> ras_event_list;
        std::vector<std::string> opal_output_lines;
        int rename_fail_pattern_index;
        bool enable_rename_fail_pattern;
        bool fail_set_lan_options_once_flag;
        bool fail_ipmi_cmd_once_flag;
        bool capture_opal_output_verbose_flag;

    public:
        std::string username;
        int get_sdr_cache_result;
        int find_sdr_next_result;

    public:
        sensor_ipmi_sel_mocked_functions();
        virtual ~sensor_ipmi_sel_mocked_functions();

        void test_error_sink(int level, const std::string& msg);
        void test_ras_event_sink(const std::string& msg, const std::string& hostname);
        const std::vector<std::string>& reported_errors();
        void clear_errors();
        size_t get_error_count();
        void clear_ras_events();
        const std::vector<std::string>& logged_events();
        size_t get_ras_event_count();
        std::vector<std::string>& get_opal_output_lines();
        void set_opal_output_capture(bool mode);

        void setup_rename_fail_pattern(int n1, int n2 = -1, int n3 = -1);
        void end_rename_fail_pattern();
        void fail_set_lan_options_once();
        void fail_ipmi_cmd_once();

        virtual int mock_rename(const char* old_name, const char* new_name);
        virtual int mock_set_lan_options(char* ip,
                                         char* user,
                                         char* password,
                                         int auth_type,
                                         int priv_level,
                                         int cipher,
                                         void* addr,
                                         int len);
        virtual int mock_ipmi_cmd(unsigned short cmd,
                                  unsigned char* request,
                                  int request_size,
                                  unsigned char* reponse,
                                  int* response_size,
                                  unsigned char* condition,
                                  char debug);
        virtual int mock_ipmi_close();
        virtual void mock_opal_output_verbose(int level, int output_id, const char* message);
        virtual int mock_getlogin_r(char* user, size_t user_size);
        virtual __uid_t mock_geteuid();
};

#endif // SENSOR_IPMI_SEL_MOCKED_FUNCTIONS_H
