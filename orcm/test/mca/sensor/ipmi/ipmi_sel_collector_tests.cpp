// Project Includes
#include "sensor_ipmi_sel_mocked_functions.h"
#include "ipmi_sel_collector_tests.h"

#define GTEST_MOCK_TESTING
#include "orcm/mca/sensor/ipmi/ipmi_sel_collector.h"
#include "orcm/mca/sensor/ipmi/ipmi_credentials.h"
#include "orcm/mca/sensor/ipmi/sensor_ipmi_sel.h"
#undef GTEST_MOCK_TESTING

#include <fstream>

extern sensor_ipmi_sel_mocked_functions sel_mocking;

// mocking variables and "C" callback functions
extern "C" {
    extern void test_error_sink_c(int level, const char* msg);
    extern void test_ras_event_c(const char* decoded_msg, const char* hostname, void* user_object);
} // extern "C"

using namespace std;

// Data
const char* hostname = "testaggregator";
const ipmi_credentials creds("1.2.3.4", "administrator", "bob's-your-uncle");
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

// Helper functions


///////////////////////////////////////////////////////////////////////////////
// MOCKED UNIT TESTS
///////////////////////////////////////////////////////////////////////////////
TEST_F(ut_ipmi_sel_collection, positive_test_of_constructor)
{
    sel_mocking.clear_errors();
    ipmi_sel_collector collector(hostname, creds, test_error_sink_c, NULL);

    ASSERT_FALSE(collector.is_bad());
    ASSERT_EQ(sel_mocking.get_error_count(), 0);
    ASSERT_STREQ(collector.hostname_.c_str(), hostname);
}

TEST_F(ut_ipmi_sel_collection, negative_test_of_constructor)
{
    sel_mocking.clear_errors();
    sel_mocking.fail_set_lan_options_once();
    ipmi_sel_collector collector(hostname, creds, test_error_sink_c, NULL);

    ASSERT_TRUE(collector.is_bad());
    ASSERT_EQ(sel_mocking.get_error_count(), 1);
    ASSERT_STREQ(sel_mocking.reported_errors()[0].c_str(), "LEVEL 0: Failed to connect to the BMC on host 'testaggregator'");
}

TEST_F(ut_ipmi_sel_collection, negative_test_of_copy_constructor)
{
    sel_mocking.clear_errors();
    ipmi_sel_collector collector(hostname, creds, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    ipmi_sel_collector bad_copy(collector);
    ASSERT_TRUE(bad_copy.is_bad());
}

TEST_F(ut_ipmi_sel_collection, negative_test_of_copy_operator)
{
    sel_mocking.clear_errors();
    ipmi_sel_collector collector(hostname, creds, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    ipmi_sel_collector bad_copy = collector;
    ASSERT_TRUE(bad_copy.is_bad());
}

TEST_F(ut_ipmi_sel_collection, negative_test_of_methods_with_bad_instance)
{
    sel_mocking.clear_errors();
    sel_mocking.fail_set_lan_options_once();
    ipmi_sel_collector collector(hostname, creds, test_error_sink_c, NULL);

    // Test public API
    ASSERT_TRUE(collector.is_bad());
    ASSERT_FALSE(collector.load_last_record_id(persist_filename));
    ASSERT_FALSE(collector.scan_new_records(NULL));
}


TEST_F(ut_ipmi_sel_collection, positive_test_of_load_last_record_id)
{
    sel_mocking.clear_errors();
    ipmi_sel_collector collector(hostname, creds, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    ofstream strm(persist_filename);
    strm << hostname << "=" << 100 << endl;
    strm.close();
    ASSERT_TRUE(collector.load_last_record_id(persist_filename));
    ASSERT_EQ(collector.last_record_id_, (uint16_t)100);
}

TEST_F(ut_ipmi_sel_collection, positive_test_of_events_1)
{
    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();
    ipmi_sel_collector collector(hostname, creds, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    remove(persist_filename);
    collector.load_last_record_id(persist_filename);
    ASSERT_EQ(collector.last_record_id_, 0);
    ASSERT_TRUE(collector.scan_new_records(test_ras_event_c));
    ASSERT_EQ(sel_mocking.get_ras_event_count(), 731); // of the data set there are 731 events
}

TEST_F(ut_ipmi_sel_collection, positive_test_of_events_2)
{
    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();
    ipmi_sel_collector collector(hostname, creds, test_error_sink_c, NULL);
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
    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();
    ipmi_sel_collector collector(hostname, creds, test_error_sink_c, NULL);
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
    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();
    ipmi_sel_collector collector(hostname, creds, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    remove(persist_filename);
    ofstream strm(persist_filename);
    strm << hostname << "=" << 730 << endl;
    strm.close();
    collector.load_last_record_id(persist_filename);
    ASSERT_EQ(collector.last_record_id_, 730);
    ASSERT_TRUE(collector.scan_new_records(test_ras_event_c));
    ASSERT_EQ(sel_mocking.get_ras_event_count(), 1); // of the data set starting at 730 there is 1 event
    ASSERT_STREQ(sel_mocking.logged_events()[0].c_str(), "testaggregator=02db 08/11/15 06:33:29 CRT Bios Critical Interrupt #05 FP NMI   (on 00:03.0) 71 [a0 00 18]");
}

TEST_F(ut_ipmi_sel_collection, negative_test_of_events)
{
    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();
    ipmi_sel_collector collector(hostname, creds, test_error_sink_c, NULL);
    ASSERT_FALSE(collector.is_bad());
    remove(persist_filename);
    collector.load_last_record_id(persist_filename);
    ASSERT_EQ(collector.last_record_id_, 0);
    sel_mocking.fail_ipmi_cmd_once();
    ASSERT_FALSE(collector.scan_new_records(test_ras_event_c));
}

TEST_F(ut_ipmi_sel_collection, test_c_api)
{
    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();

    remove(persist_filename);
    ASSERT_TRUE(process_node_sel_logs(hostname, creds.get_bmc_ip(), creds.get_username(),
                                      creds.get_password(), persist_filename, test_error_sink_c,
                                      test_ras_event_c, NULL));
    ASSERT_NE(sel_mocking.get_ras_event_count(), 0);
    remove(persist_filename);

    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();
    sel_mocking.fail_set_lan_options_once();

    ASSERT_FALSE(process_node_sel_logs(hostname, creds.get_bmc_ip(), creds.get_username(),
                                       creds.get_password(), persist_filename, test_error_sink_c,
                                    test_ras_event_c, NULL));

    sel_mocking.clear_errors();
    sel_mocking.clear_ras_events();
    remove(persist_filename);
}
