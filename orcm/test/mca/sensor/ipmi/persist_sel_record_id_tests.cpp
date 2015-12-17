/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

// Project Includes
#include "sensor_ipmi_sel_mocked_functions.h"
#include "persist_sel_record_id_tests.h"
#include "orcm/mca/sensor/ipmi/persist_sel_record_id.h"

// C++ Includes
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>

extern sensor_ipmi_sel_mocked_functions sel_mocking;

extern "C" {
    extern void test_error_sink_c(int level, const char* msg);
}


using namespace std;

const string ut_persist_sel_record_id::test_filename = "test_persist_sel_record_id.conf";
const string ut_persist_sel_record_id::backup_filename = test_filename + ".backup";
const string ut_persist_sel_record_id::new_filename = test_filename + ".new";

vector<string> ut_persist_sel_record_id::messy_file;
vector<string> ut_persist_sel_record_id::messy_file_golden1;
vector<string> ut_persist_sel_record_id::messy_file_golden2;
vector<string> ut_persist_sel_record_id::messy_file_golden3;
vector<string> ut_persist_sel_record_id::messy_file_golden4;
vector<string> ut_persist_sel_record_id::messy_file_golden5;

// Test Helper Methods in Fixture
void ut_persist_sel_record_id::SetUpTestCase()
{
    messy_file.push_back("");
    messy_file.push_back("test_aggregator=49");
    messy_file.push_back("test_node01=3");
    messy_file.push_back("=1");
    messy_file.push_back("test_node02=3");
    messy_file.push_back("test_node03=3");
    messy_file.push_back("  test_node04 =\t3  ");
    messy_file.push_back("test_node05=3");
    messy_file.push_back("test_node06=3");
    messy_file.push_back("test_node07=3");
    messy_file.push_back("");
    messy_file.push_back("test_node08=invalid3u4more");
    messy_file.push_back("test_node09=3");
    messy_file.push_back("");
    messy_file.push_back("missing_node");
    messy_file.push_back("test_node10=3");
    messy_file.push_back("test_node11=blue3red");
    messy_file.push_back("test_node12=3");
    messy_file.push_back("test_node13=3");
    messy_file.push_back("");

    messy_file_golden1.push_back("test_aggregator=100");
    messy_file_golden1.push_back("test_node01=3");
    messy_file_golden1.push_back("test_node02=3");
    messy_file_golden1.push_back("test_node03=3");
    messy_file_golden1.push_back("test_node04=3");
    messy_file_golden1.push_back("test_node05=3");
    messy_file_golden1.push_back("test_node06=3");
    messy_file_golden1.push_back("test_node07=3");
    messy_file_golden1.push_back("test_node08=invalid3u4more");
    messy_file_golden1.push_back("test_node09=3");
    messy_file_golden1.push_back("test_node10=3");
    messy_file_golden1.push_back("test_node11=blue3red");
    messy_file_golden1.push_back("test_node12=3");
    messy_file_golden1.push_back("test_node13=3");

    messy_file_golden2.push_back("test_aggregator=49");
    messy_file_golden2.push_back("test_node01=3");
    messy_file_golden2.push_back("test_node02=3");
    messy_file_golden2.push_back("test_node03=3");
    messy_file_golden2.push_back("test_node04=3");
    messy_file_golden2.push_back("test_node05=3");
    messy_file_golden2.push_back("test_node06=3");
    messy_file_golden2.push_back("test_node07=3");
    messy_file_golden2.push_back("test_node08=invalid3u4more");
    messy_file_golden2.push_back("test_node09=3");
    messy_file_golden2.push_back("test_node10=3");
    messy_file_golden2.push_back("test_node11=77");
    messy_file_golden2.push_back("test_node12=3");
    messy_file_golden2.push_back("test_node13=3");

    messy_file_golden3.push_back("test_aggregator=49");
    messy_file_golden3.push_back("test_node01=3");
    messy_file_golden3.push_back("test_node02=3");
    messy_file_golden3.push_back("test_node03=3");
    messy_file_golden3.push_back("test_node04=3");
    messy_file_golden3.push_back("test_node05=3");
    messy_file_golden3.push_back("test_node06=3");
    messy_file_golden3.push_back("test_node07=3");
    messy_file_golden3.push_back("test_node08=34");
    messy_file_golden3.push_back("test_node09=3");
    messy_file_golden3.push_back("test_node10=3");
    messy_file_golden3.push_back("test_node11=blue3red");
    messy_file_golden3.push_back("test_node12=3");
    messy_file_golden3.push_back("test_node13=3");

    messy_file_golden4.push_back("test_aggregator=49");
    messy_file_golden4.push_back("test_node01=3");
    messy_file_golden4.push_back("test_node02=3");
    messy_file_golden4.push_back("test_node03=3");
    messy_file_golden4.push_back("test_node04=3");
    messy_file_golden4.push_back("test_node05=3");
    messy_file_golden4.push_back("test_node06=3");
    messy_file_golden4.push_back("test_node07=3");
    messy_file_golden4.push_back("test_node08=invalid3u4more");
    messy_file_golden4.push_back("test_node09=3");
    messy_file_golden4.push_back("test_node10=3");
    messy_file_golden4.push_back("test_node11=blue3red");
    messy_file_golden4.push_back("test_node12=3");
    messy_file_golden4.push_back("test_node13=3");
    messy_file_golden4.push_back("test_node15=53");

    messy_file_golden5.push_back("test_aggregator=171");
    messy_file_golden5.push_back("test_node01=3");
    messy_file_golden5.push_back("test_node02=3");
    messy_file_golden5.push_back("test_node03=3");
    messy_file_golden5.push_back("test_node04=3");
    messy_file_golden5.push_back("test_node05=3");
    messy_file_golden5.push_back("test_node06=3");
    messy_file_golden5.push_back("test_node07=3");
    messy_file_golden5.push_back("test_node08=invalid3u4more");
    messy_file_golden5.push_back("test_node09=3");
    messy_file_golden5.push_back("test_node10=3");
    messy_file_golden5.push_back("test_node11=blue3red");
    messy_file_golden5.push_back("test_node12=3");
    messy_file_golden5.push_back("test_node13=3");
}

void ut_persist_sel_record_id::TearDownTestCase()
{
    messy_file.clear();
    messy_file_golden1.clear();
    messy_file_golden2.clear();
    messy_file_golden3.clear();
    messy_file_golden4.clear();
    messy_file_golden5.clear();

    test_reset();
}

void ut_persist_sel_record_id::test_reset()
{
    sel_mocking.clear_errors();

    remove(backup_filename.c_str());
    remove(test_filename.c_str());
    remove(new_filename.c_str());
}

void ut_persist_sel_record_id::test_save_file(const string& filename, const vector<string>& contents)
{
    ofstream strm(filename.c_str());
    for(size_t i = 0; i < contents.size(); ++i) {
        strm << contents[i] << endl;
    }
    strm.close();
}

vector<string> ut_persist_sel_record_id::test_load_file(const string& filename)
{
    static char line_buffer[80]; // Plenty long...
    vector<string> results;
    ifstream strm(filename.c_str());
    if(false == strm.fail()) {
        while(false == strm.eof()) {
            strm.getline(line_buffer, sizeof(line_buffer)-1);
            if(false == strm.eof()) {
                line_buffer[sizeof(line_buffer)-1] = '\0';
                results.push_back(string(line_buffer));
            }
        }
    }
    strm.close();
    return results;
}

///////////////////////////////////////////////////////////////////////////////
// MOCKED UNIT TESTS
///////////////////////////////////////////////////////////////////////////////
TEST_F(ut_persist_sel_record_id, old_hand_editted_file)
{
    test_reset();
    test_save_file(test_filename, messy_file);
    persist_sel_record_id* obj = new persist_sel_record_id("test_aggregator", test_error_sink_c);
    obj->load_last_record_id(test_filename.c_str());
    int id = obj->get_record_id();
    ASSERT_EQ(id, 49);
    obj->set_record_id(100);
    id = obj->get_record_id();
    ASSERT_EQ(id, 100);
    delete obj;
    vector<string> new_content = test_load_file(test_filename);
    ASSERT_EQ(messy_file_golden1.size(), new_content.size());
    for(size_t i = 0; i < max(messy_file_golden1.size(), new_content.size()); ++i) {
        ASSERT_STREQ(messy_file_golden1[i].c_str(), new_content[i].c_str());
    }

    ASSERT_EQ(sel_mocking.get_error_count(), 0);
}

TEST_F(ut_persist_sel_record_id, bad_value_correction)
{
    test_reset();
    test_save_file(test_filename, messy_file);
    persist_sel_record_id* obj = new persist_sel_record_id("test_node11", test_error_sink_c);
    obj->load_last_record_id(test_filename.c_str());
    int id = obj->get_record_id();
    ASSERT_EQ(id, 3);
    obj->set_record_id(77);
    id = obj->get_record_id();
    ASSERT_EQ(id, 77);
    delete obj;
    vector<string> new_content = test_load_file(test_filename);
    ASSERT_EQ(messy_file_golden2.size(), new_content.size());
    for(size_t i = 0; i < max(messy_file_golden2.size(), new_content.size()); ++i) {
        ASSERT_STREQ(messy_file_golden2[i].c_str(), new_content[i].c_str());
    }

    ASSERT_EQ(sel_mocking.get_error_count(), 0);
}

TEST_F(ut_persist_sel_record_id, illegal_value_default)
{
    test_reset();
    test_save_file(test_filename, messy_file);
    persist_sel_record_id* obj = new persist_sel_record_id("test_node08", test_error_sink_c);
    obj->load_last_record_id(test_filename.c_str());
    int id = obj->get_record_id();
    ASSERT_EQ(id, 3);
    obj->set_record_id(34);
    id = obj->get_record_id();
    ASSERT_EQ(id, 34);
    delete obj;
    vector<string> new_content = test_load_file(test_filename);
    ASSERT_EQ(messy_file_golden3.size(), new_content.size());
    for(size_t i = 0; i < max(messy_file_golden3.size(), new_content.size()); ++i) {
        ASSERT_STREQ(messy_file_golden3[i].c_str(), new_content[i].c_str());
    }

    ASSERT_EQ(sel_mocking.get_error_count(), 0);
}


TEST_F(ut_persist_sel_record_id, new_entry_added_to_file)
{
    test_reset();
    test_save_file(test_filename, messy_file);
    persist_sel_record_id* obj = new persist_sel_record_id("test_node15", test_error_sink_c);
    obj->load_last_record_id(test_filename.c_str());
    int id = obj->get_record_id();
    ASSERT_EQ(id, 0);
    obj->set_record_id(53);
    id = obj->get_record_id();
    ASSERT_EQ(id, 53);
    delete obj;
    vector<string> new_content = test_load_file(test_filename);
    ASSERT_EQ(messy_file_golden4.size(), new_content.size());
    for(size_t i = 0; i < max(messy_file_golden4.size(), new_content.size()); ++i) {
        ASSERT_STREQ(messy_file_golden4[i].c_str(), new_content[i].c_str());
    }
}

TEST_F(ut_persist_sel_record_id, no_existing_file)
{
    test_reset();
    persist_sel_record_id* obj = new persist_sel_record_id("test_aggregator", test_error_sink_c);
    obj->load_last_record_id(test_filename.c_str());
    int id = obj->get_record_id();
    ASSERT_EQ(id, 0);
    obj->set_record_id(501);
    id = obj->get_record_id();
    ASSERT_EQ(id, 501);
    delete obj;
    vector<string> new_content = test_load_file(test_filename);
    ASSERT_EQ(new_content.size(), 1);
    ASSERT_STREQ(new_content[0].c_str(), "test_aggregator=501");

    ASSERT_EQ(sel_mocking.get_error_count(), 0);
}

TEST_F(ut_persist_sel_record_id, rename_failing_first)
{
    test_reset();
    test_save_file(test_filename, messy_file_golden1);
    persist_sel_record_id* obj = new persist_sel_record_id("test_aggregator", test_error_sink_c);
    obj->load_last_record_id(test_filename.c_str());
    int id = obj->get_record_id();
    ASSERT_EQ(id, 100);
    obj->set_record_id(171);
    id = obj->get_record_id();
    ASSERT_EQ(id, 171);
    sel_mocking.setup_rename_fail_pattern(0);
    delete obj;
    sel_mocking.end_rename_fail_pattern();
    vector<string> new_content = test_load_file(test_filename);
    ASSERT_EQ(messy_file_golden1.size(), new_content.size());
    for(size_t i = 0; i < max(messy_file_golden1.size(), new_content.size()); ++i) {
        ASSERT_STREQ(messy_file_golden1[i].c_str(), new_content[i].c_str());
    }

    EXPECT_EQ(sel_mocking.get_error_count(), 1);
    EXPECT_STREQ(sel_mocking.reported_errors()[0].c_str(), "LEVEL 1: Failed to backup original file; aborting file update; original file is ok; new file renamed with .new suffix");
    sel_mocking.clear_errors();

    new_content = test_load_file(new_filename);
    ASSERT_EQ(messy_file_golden5.size(), new_content.size());
    for(size_t i = 0; i < max(messy_file_golden5.size(), new_content.size()); ++i) {
        ASSERT_STREQ(messy_file_golden5[i].c_str(), new_content[i].c_str());
    }
}

TEST_F(ut_persist_sel_record_id, rename_failing_after_second_time)
{
    test_reset();
    test_save_file(test_filename, messy_file_golden1);
    persist_sel_record_id* obj = new persist_sel_record_id("test_aggregator", test_error_sink_c);

    obj->load_last_record_id(test_filename.c_str());
    int id = obj->get_record_id();
    ASSERT_EQ(id, 100);
    obj->set_record_id(171);
    id = obj->get_record_id();
    ASSERT_EQ(id, 171);
    sel_mocking.setup_rename_fail_pattern(1, 2);
    delete obj;
    sel_mocking.end_rename_fail_pattern();
    vector<string> new_content = test_load_file(new_filename);
    EXPECT_EQ(messy_file_golden1.size(), new_content.size());
    for(size_t i = 0; i < max(messy_file_golden5.size(), new_content.size()); ++i) {
        EXPECT_STREQ(messy_file_golden5[i].c_str(), new_content[i].c_str());
    }
    remove(new_filename.c_str());

    EXPECT_EQ(sel_mocking.get_error_count(), 1);
    if(1 != sel_mocking.get_error_count()) {
        sel_mocking.clear_errors();
        FAIL() << "Expected an error but didn't get one: count=" << sel_mocking.get_error_count();
    } else {
        EXPECT_STREQ(sel_mocking.reported_errors()[0].c_str(), "LEVEL 0: Failed to restore backup file to original file; cannot recover user intervention required");
        sel_mocking.clear_errors();
    }
}

TEST_F(ut_persist_sel_record_id, make_temp_filename_failure)
{
    test_reset();

    // Test Setup
    for(int i = 0; i < 4096; ++i) {
        stringstream ss;
        ss << test_filename << "." << hex << setw(3) << setfill('0') << i << ".tmp";
        test_save_file(ss.str(), messy_file_golden1);
    }
    persist_sel_record_id* obj = new persist_sel_record_id("test_aggregator", test_error_sink_c);
    obj->load_last_record_id(test_filename.c_str());
    obj->set_record_id(500);
    delete obj;

    EXPECT_EQ(sel_mocking.get_error_count(), 1);
    if(0 < sel_mocking.get_error_count()) {
        EXPECT_STREQ(sel_mocking.reported_errors()[0].c_str(), "LEVEL 0: Unable to make a temporary filename used to rewrite the new record_id");
    }

    // Cleanup
    for(int i = 0; i < 4096; ++i) {
        stringstream ss;
        ss << test_filename << "." << hex << setw(3) << setfill('0') << i << ".tmp";
        remove(ss.str().c_str());
    }
}
