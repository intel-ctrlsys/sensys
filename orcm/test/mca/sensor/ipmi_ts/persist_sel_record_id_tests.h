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

#ifndef IPMI_TS_PERSIST_SEL_RECORD_ID_TESTS_H
#define IPMI_TS_PERSIST_SEL_RECORD_ID_TESTS_H

#include "gtest/gtest.h"
#include <string>
#include <vector>

class ut_persist_sel_record_id: public testing::Test
{
    protected: // gtest required methods
        static void SetUpTestCase();
        static void TearDownTestCase();


    protected:
        // Variables for tests
        const static std::string test_filename;
        const static std::string backup_filename;
        const static std::string new_filename;

        static std::vector<std::string> messy_file;
        static std::vector<std::string> messy_file_golden1;
        static std::vector<std::string> messy_file_golden2;
        static std::vector<std::string> messy_file_golden3;
        static std::vector<std::string> messy_file_golden4;
        static std::vector<std::string> messy_file_golden5;

        static std::vector<std::string> error_list;
        static int fail_when_zero_counter;

    protected: // Helper functions for tests
        static void test_save_file(const std::string& filename, const std::vector<std::string>& contents);
        static std::vector<std::string> test_load_file(const std::string& filename);
        static void test_reset();

        static void fail_on_rename_n(int n, int n2 = -1, int n3 = -1);
        static void fail_on_remove_n(int n, int n2 = -1, int n3 = -1);
};

#endif
