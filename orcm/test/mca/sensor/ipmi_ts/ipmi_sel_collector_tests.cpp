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

// Project Includes
#include "ipmiutilAgent_mocks.h"
#include "ipmi_sel_collector_tests.h"

#define GTEST_MOCK_TESTING
#include "orcm/mca/sensor/ipmi_ts/ipmi_ts_sel_collector.h"
#undef GTEST_MOCK_TESTING

#include <fstream>

extern sensor_ipmi_sel_mocked_functions sel_mocking;
extern std::map<supportedMocks, mockManager> mocks;

// mocking variables and "C" callback functions
extern "C" {
    extern void test_error_sink_c(int level, const char* msg);
    extern void test_ras_event_c(const char* decoded_msg, const char* hostname, void* user_object);
} // extern "C"

using namespace std;

// Data
const char* hostname = "testaggregator";
const char* persist_filename = "test_persist_file.conf";

// Fixture methods
void ut_ipmi_sel_collection::SetUpTestCase()
{
    string backup = string(persist_filename) + ".backup";
    remove(persist_filename);
    remove(backup.c_str());
}

void ut_ipmi_sel_collection::TearDownTestCase()
{
    string backup = string(persist_filename) + ".backup";
    remove(persist_filename);
    remove(backup.c_str());
}

void ut_ipmi_sel_collection::SetUp()
{
    mocks[IPMI_CMD].restartMock();
}

// Helper functions
bool ut_ipmi_sel_collection::WildcardCompare(const char* actual_value, const char* pattern)
{
    string value = actual_value;
    string patt = pattern;
    vector<string> parts;
    size_t pos = 0;
    size_t next;

    // Get parts to match from mask...
    string part;
    while(pos < patt.size() && string::npos != (next = patt.find('*', pos))) {
        part = patt.substr(pos, next - pos);
        if(false == part.empty()) {
            parts.push_back(part);
        }
        pos = next + 1;
    }
    if(0 != pos) {
        part = patt.substr(pos);
        if(false == part.empty()) {
            parts.push_back(part);
        }
    }

    // Match value to pattern
    if(string::npos == patt.find_first_not_of("*")) {
        return true;
    }

    if(0 == parts.size()) {
        // No wildcards, just compare...
        return (value == patt);
    } else {
        // Scan value for parts...
        size_t pos = 0;
        size_t next;
        for(size_t i = 0; i < parts.size(); ++i) {
            next = value.find(parts[i], pos);
            if(string::npos == next) {
                return false;
            }
            pos = next + parts[i].size();
        }
        return true;
    }
}


///////////////////////////////////////////////////////////////////////////////
// MOCKED UNIT TESTS
///////////////////////////////////////////////////////////////////////////////
TEST_F(ut_ipmi_sel_collection, positive_test_of_constructor)
{
    sel_mocking.clear_errors();
    ipmi_ts_sel_collector collector(hostname, test_error_sink_c, NULL);

    ASSERT_FALSE(collector.is_bad());
    ASSERT_EQ(sel_mocking.get_error_count(), 0);
    ASSERT_STREQ(collector.hostname_.c_str(), hostname);
}

TEST_F(ut_ipmi_sel_collection, negative_test_of_copy_constructor)
{
    sel_mocking.clear_errors();
    ipmi_ts_sel_collector collector(hostname, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    ipmi_ts_sel_collector bad_copy(collector);
    ASSERT_TRUE(bad_copy.is_bad());
}

TEST_F(ut_ipmi_sel_collection, negative_test_of_copy_operator)
{
    sel_mocking.clear_errors();
    ipmi_ts_sel_collector collector(hostname, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    ipmi_ts_sel_collector bad_copy = collector;
    ASSERT_TRUE(bad_copy.is_bad());
}

TEST_F(ut_ipmi_sel_collection, positive_test_of_load_last_record_id)
{
    mocks[IPMI_CMD].pushState(LEGACY);

    sel_mocking.clear_errors();
    ipmi_ts_sel_collector collector(hostname, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    ofstream strm(persist_filename);
    strm << hostname << "=" << 100 << endl;
    strm.close();
    ASSERT_TRUE(collector.load_last_record_id(persist_filename));
    ASSERT_EQ(collector.last_record_id_, (uint16_t)100);
}

TEST_F(ut_ipmi_sel_collection, positive_test_of_events_1)
{
    for (int i = 0; i < 1000; ++i) //More than enought legacy states in the mock queue
        mocks[IPMI_CMD].pushState(LEGACY);

    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();
    ipmi_ts_sel_collector collector(hostname, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    remove(persist_filename);
    collector.load_last_record_id(persist_filename);
    ASSERT_EQ(collector.last_record_id_, 0);
    ASSERT_TRUE(collector.scan_new_records(test_ras_event_c));
    ASSERT_EQ(sel_mocking.get_ras_event_count(), 731); // of the data set there are 731 events
}

TEST_F(ut_ipmi_sel_collection, positive_test_of_events_2)
{
    for (int i = 0; i < 1000; ++i) //More than enought legacy states in the mock queue
        mocks[IPMI_CMD].pushState(LEGACY);

    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();
    ipmi_ts_sel_collector collector(hostname, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    remove(persist_filename);
    ofstream strm(persist_filename);
    strm << hostname << "=" << 600 << endl;
    strm.close();
    collector.load_last_record_id(persist_filename);
    ASSERT_EQ(collector.last_record_id_, 600);
    ASSERT_TRUE(collector.scan_new_records(test_ras_event_c));
    ASSERT_EQ(sel_mocking.get_ras_event_count(), 131); // of the data set starting at 600 there are 131 events
}

TEST_F(ut_ipmi_sel_collection, positive_test_of_events_3)
{
    mocks[IPMI_CMD].pushState(LEGACY);
    mocks[IPMI_CMD].pushState(LEGACY);

    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();
    ipmi_ts_sel_collector collector(hostname, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    remove(persist_filename);
    ofstream strm(persist_filename);
    strm << hostname << "=" << 731 << endl;
    strm.close();
    collector.load_last_record_id(persist_filename);
    ASSERT_EQ(collector.last_record_id_, 731);
    ASSERT_TRUE(collector.scan_new_records(test_ras_event_c));
    ASSERT_EQ(sel_mocking.get_ras_event_count(), 0); // Should be no new events...
}

TEST_F(ut_ipmi_sel_collection, positive_test_of_events_4)
{
    mocks[IPMI_CMD].pushState(LEGACY);
    mocks[IPMI_CMD].pushState(LEGACY);

    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();
    ipmi_ts_sel_collector collector(hostname, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    remove(persist_filename);
    ofstream strm(persist_filename);
    strm << hostname << "=" << 730 << endl;
    strm.close();
    collector.load_last_record_id(persist_filename);
    ASSERT_EQ(collector.last_record_id_, 730);
    ASSERT_TRUE(collector.scan_new_records(test_ras_event_c));
    ASSERT_EQ(sel_mocking.get_ras_event_count(), 1); // of the data set starting at 730 there is 1 event
    ASSERT_TRUE(WildcardCompare(sel_mocking.logged_events()[0].c_str(), "testaggregator=02db * CRT Bios Critical Interrupt #05 FP NMI   (on 00:03.0) 71 [a0 00 18]"));
}

TEST_F(ut_ipmi_sel_collection, negative_test_of_events)
{
    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();
    ipmi_ts_sel_collector collector(hostname, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    remove(persist_filename);
    collector.load_last_record_id(persist_filename);
    ASSERT_EQ(collector.last_record_id_, 0);
    sel_mocking.fail_ipmi_cmd_once();
    ASSERT_FALSE(collector.scan_new_records(test_ras_event_c));
}
