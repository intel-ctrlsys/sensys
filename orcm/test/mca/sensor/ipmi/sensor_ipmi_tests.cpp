/*
 * Copyright (c) 2015-2016      Intel Corporation. All rights reserved.
 *
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sensor_ipmi_tests.h"
#include "sensor_ipmi_sel_mocked_functions.h"
#include "sensor_ipmi_negative_mocks.h"

// OPAL
#include "opal/dss/dss.h"

// ORTE
#include "orte/runtime/orte_globals.h"

// ORCM
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/freq/sensor_freq.h"

extern sensor_ipmi_sel_mocked_functions sel_mocking;

// mocking variables and "C" callback functions
extern "C" {
    ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
    extern orcm_sensor_base_module_t orcm_sensor_ipmi_module;
    extern int first_sample;

    extern void collect_ipmi_sample(orcm_sensor_sampler_t *sampler);

    extern void test_error_sink_c(int level, const char* msg);
    extern void test_ras_event_c(const char* decoded_msg, const char* hostname, void* user_object);

    #include "opal/class/opal_list.h"
    #include "orcm/mca/sensor/ipmi/sensor_ipmi_decls.h"
    #include "orcm/mca/sensor/ipmi/sensor_ipmi.h"
    extern void orcm_sensor_ipmi_get_sel_events(ipmi_capsule_t* capsule);
    extern int ipmi_enable_sampling(const char* sensor_specification);
    extern int ipmi_disable_sampling(const char* sensor_specification);
    extern int ipmi_reset_sampling(const char* sensor_specification);
    extern int orcm_sensor_ipmi_open(void);
    extern int orcm_sensor_ipmi_close(void);
    extern int ipmi_component_register(void);
    extern int orcm_sensor_ipmi_query(mca_base_module_t **module, int *priority);
} // extern "C"

using namespace std;

// Fixture methods
void ut_sensor_ipmi_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();

    mca_sensor_ipmi_component.collect_metrics = true;
}

void ut_sensor_ipmi_tests::TearDownTestCase()
{
    // Release OPAL level resources
    opal_dss_close();
}

void ut_sensor_ipmi_tests::SetUp()
{
    is_load_ipmi_config_file_expected_to_succeed = true;
    is_get_bmcs_for_aggregator_expected_to_succeed = true;
    is_opal_progress_thread_init_expected_to_succeed = true;
    is_get_sdr_cache_expected_to_succeed = true;
    is_get_bmc_info_expected_to_succeed = true;
    find_sdr_next_success_count = 3;
}

// Helper functions
void ut_sensor_ipmi_tests::populate_capsule(ipmi_capsule_t* cap)
{
    strncpy(cap->node.name, "test_host_name", sizeof(cap->node.name));
    strncpy(cap->node.bmc_ip, "192.168.254.254", sizeof(cap->node.bmc_ip));
    strncpy(cap->node.user, "orcmtestuser", sizeof(cap->node.user));
    strncpy(cap->node.pasw, "testpassword", sizeof(cap->node.pasw));
    cap->prop.collected_sel_records = OBJ_NEW(opal_list_t);
}

bool ut_sensor_ipmi_tests::WildcardCompare(const char* actual_value, const char* pattern)
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
TEST_F(ut_sensor_ipmi_tests, orcm_sensor_ipmi_get_sel_events_positive)
{
    ipmi_capsule_t cap;
    populate_capsule(&cap);

    char* saved = mca_sensor_ipmi_component.sel_state_filename;
    mca_sensor_ipmi_component.sel_state_filename = NULL;
    sel_mocking.set_opal_output_capture(true);

    orcm_sensor_ipmi_get_sel_events(&cap);

    sel_mocking.set_opal_output_capture(false);
    mca_sensor_ipmi_component.sel_state_filename = saved;

    size_t data_size = opal_list_get_size(cap.prop.collected_sel_records);
    EXPECT_EQ((size_t)731, data_size);
    if((size_t)731 == data_size) {
        opal_value_t* item;
        int count = 0;
        OPAL_LIST_FOREACH(item, cap.prop.collected_sel_records, opal_value_t) {
            if(count == 0) {
                EXPECT_TRUE(WildcardCompare(item->data.string, "0001 * INF BMC  Event Log #07 Log Cleared 6f [02 ff ff]")) << "First record was not as expected!";
            } else if(730 == count) {
                EXPECT_TRUE(WildcardCompare(item->data.string, "02db * CRT Bios Critical Interrupt #05 FP NMI   (on 00:03.0) 71 [a0 00 18]")) << "Last record was not as expected!";
            }
            ++count;
        }
    }
    OBJ_RELEASE(cap.prop.collected_sel_records); // Done with sel records

    EXPECT_EQ(731, (size_t)sel_mocking.get_opal_output_lines().size());

    sel_mocking.get_opal_output_lines().clear();
}

TEST_F(ut_sensor_ipmi_tests, orcm_sensor_ipmi_get_sel_events_negative)
{
    ipmi_capsule_t cap;
    populate_capsule(&cap);

    char* saved = mca_sensor_ipmi_component.sel_state_filename;
    mca_sensor_ipmi_component.sel_state_filename = NULL;
    sel_mocking.set_opal_output_capture(true);
    sel_mocking.fail_ipmi_cmd_once();

    orcm_sensor_ipmi_get_sel_events(&cap);

    sel_mocking.set_opal_output_capture(false);
    mca_sensor_ipmi_component.sel_state_filename = saved;

    size_t data_size = opal_list_get_size(cap.prop.collected_sel_records);
    EXPECT_EQ((size_t)0, data_size) << "IPMI Error so was expecting 0 SEL records returned!";
    OBJ_RELEASE(cap.prop.collected_sel_records); // Done with sel records

    EXPECT_EQ((size_t)1, sel_mocking.get_opal_output_lines().size()) << "Expected one error message!";
    if(1 == sel_mocking.get_opal_output_lines().size()) {
        EXPECT_STREQ("ERROR: collecting IPMI SEL records: Failed to retrieve IPMI SEL record ID '0x0000' from host: test_host_name\n", sel_mocking.get_opal_output_lines()[0].c_str());
    }

    sel_mocking.get_opal_output_lines().clear();
}

TEST_F(ut_sensor_ipmi_tests, orcm_sensor_ipmi_sample_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("ipmi", false, false);
    mca_sensor_ipmi_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    // Tests
    mca_sensor_ipmi_component.collect_metrics = false;
    mca_sensor_ipmi_component.test = true;
    collect_ipmi_sample(sampler);
    mca_sensor_ipmi_component.test = false;
    EXPECT_EQ(0, (mca_sensor_ipmi_component.diagnostics & 0x1));

    mca_sensor_ipmi_component.collect_metrics = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "ipmi");
    mca_sensor_ipmi_component.test = true;
    collect_ipmi_sample(sampler);
    mca_sensor_ipmi_component.test = false;
    EXPECT_EQ(1, (mca_sensor_ipmi_component.diagnostics & 0x1));

    // Cleanup
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_ipmi_component.runtime_metrics = NULL;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_api_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("ipmi", false, false);
    mca_sensor_ipmi_component.runtime_metrics = object;

    // Tests
    ipmi_enable_sampling("ipmi");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    ipmi_disable_sampling("ipmi");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    ipmi_enable_sampling("ipmi");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    ipmi_reset_sampling("ipmi");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    ipmi_enable_sampling("not_the_right_datagroup");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    ipmi_enable_sampling("all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_ipmi_component.runtime_metrics = NULL;
}


TEST_F(ut_sensor_ipmi_tests, ipmi_api_tests_2)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("ipmi", false, false);
    mca_sensor_ipmi_component.runtime_metrics = object;
    orcm_sensor_base_runtime_metrics_track(object, "bmcfwrev");
    orcm_sensor_base_runtime_metrics_track(object, "ipmiver");
    orcm_sensor_base_runtime_metrics_track(object, "manufacturer_id");
    orcm_sensor_base_runtime_metrics_track(object, "sys_power_state");

    // Tests
    mca_sensor_ipmi_component.test = true;
    ipmi_enable_sampling("ipmi:sys_power_state");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "ipmiver"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "sys_power_state"));
    EXPECT_EQ(1,orcm_sensor_base_runtime_metrics_active_label_count(object));
    ipmi_disable_sampling("ipmi:sys_power_state");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "sys_power_state"));
    ipmi_enable_sampling("ipmi:manufacturer_id");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "manufacturer_id"));
    ipmi_reset_sampling("ipmi:manufacturer_id");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "manufacturer_id"));
    ipmi_enable_sampling("ipmi:other_sensor");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "manufacturer_id"));
    ipmi_enable_sampling("ipmi:all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "bmcfwrev"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "sys_power_state"));
    EXPECT_EQ(4,orcm_sensor_base_runtime_metrics_active_label_count(object));
    mca_sensor_ipmi_component.test = false;

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_ipmi_component.runtime_metrics = NULL;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_open_close_register_test)
{
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_open());
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_close());
    EXPECT_EQ(ORCM_SUCCESS, ipmi_component_register());
    EXPECT_FALSE(mca_sensor_ipmi_component.test);
    EXPECT_STREQ("*", mca_sensor_ipmi_component.sensor_group);
}

TEST_F(ut_sensor_ipmi_tests, ipmi_query_test)
{
    mca_base_module_t* module = NULL;
    int priority = 0;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_query(&module, &priority));
    EXPECT_EQ(50, priority);
    EXPECT_NE(0, (uint64_t)module);
}

TEST_F(ut_sensor_ipmi_tests, ipmi_init_finalize_test)
{
    mca_sensor_ipmi_component.test = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.finalize();
    sel_mocking.username = "random";
    EXPECT_EQ(ORCM_ERR_PERM, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.finalize();
    sel_mocking.username = "root";
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.finalize();
    mca_sensor_ipmi_component.test = false;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_start_stop_test)
{
    mca_sensor_ipmi_component.test = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.start(0);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();
    mca_sensor_ipmi_component.use_progress_thread = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.start(0);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();
    mca_sensor_ipmi_component.use_progress_thread = false;
    mca_sensor_ipmi_component.test = false;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_start_twice)
{
    mca_sensor_ipmi_component.test = true;
    mca_sensor_ipmi_component.use_progress_thread = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    // Call start twice
    orcm_sensor_ipmi_module.start(0);
    orcm_sensor_ipmi_module.start(0);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();
    mca_sensor_ipmi_component.use_progress_thread = false;
    mca_sensor_ipmi_component.test = false;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_init_get_inventory_success){
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.finalize();
}

TEST_F(ut_sensor_ipmi_tests, ipmi_init_unable_to_load_ipmi_config_file){
    is_load_ipmi_config_file_expected_to_succeed = false;
    EXPECT_EQ(ORCM_ERROR, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.finalize();
}

TEST_F(ut_sensor_ipmi_tests, ipmi_init_unable_to_get_bmcs_for_aggregator){
    is_get_bmcs_for_aggregator_expected_to_succeed = false;
    EXPECT_EQ(ORCM_ERROR, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.finalize();
}

TEST_F(ut_sensor_ipmi_tests, ipmi_init_get_sdr_cache_failed){
    is_get_sdr_cache_expected_to_succeed = false;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.finalize();
}

TEST_F(ut_sensor_ipmi_tests, ipmi_start_unable_to_start_thread){
    mca_sensor_ipmi_component.test = true;
    mca_sensor_ipmi_component.use_progress_thread = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    is_opal_progress_thread_init_expected_to_succeed = false;
    orcm_sensor_ipmi_module.start(0);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();
    mca_sensor_ipmi_component.use_progress_thread = false;
    mca_sensor_ipmi_component.test = false;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_set_and_get_sample_rate){
    int sample_rate = 0;
    int old_sample_rate = mca_sensor_ipmi_component.sample_rate;
    int new_sample_rate = 100;

    mca_sensor_ipmi_component.test = true;
    mca_sensor_ipmi_component.use_progress_thread = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());

    orcm_sensor_ipmi_module.set_sample_rate(new_sample_rate);
    orcm_sensor_ipmi_module.get_sample_rate(&sample_rate);
    EXPECT_EQ(sample_rate, new_sample_rate);
    orcm_sensor_ipmi_module.finalize();

    mca_sensor_ipmi_component.use_progress_thread = false;
    mca_sensor_ipmi_component.test = false;
    mca_sensor_ipmi_component.sample_rate = old_sample_rate;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_set_and_get_sample_rate_no_progress_thread){
    int sample_rate = 0;
    int new_sample_rate = 100;

    mca_sensor_ipmi_component.test = true;
    mca_sensor_ipmi_component.use_progress_thread = false;

    orcm_sensor_ipmi_module.set_sample_rate(new_sample_rate);
    orcm_sensor_ipmi_module.get_sample_rate(&sample_rate);
    EXPECT_NE(sample_rate, new_sample_rate);

    mca_sensor_ipmi_component.test = false;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_get_sample_rate_null_check){
    orcm_sensor_ipmi_module.get_sample_rate(NULL);
}

TEST_F(ut_sensor_ipmi_tests, ipmi_start_with_sample_rate){
    int old_sample_rate = mca_sensor_ipmi_component.sample_rate;
    mca_sensor_ipmi_component.test = true;
    mca_sensor_ipmi_component.use_progress_thread = true;

    mca_sensor_ipmi_component.sample_rate = 100;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.start(0);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();

    mca_sensor_ipmi_component.use_progress_thread = false;
    mca_sensor_ipmi_component.test = false;
    mca_sensor_ipmi_component.sample_rate = old_sample_rate;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_log_zero_hosts){
    int host_count = 0;
    opal_buffer_t *sample = NULL;
    struct timeval sampletime;
    const char* dummy_str = "dummy_str";

    sample = OBJ_NEW(opal_buffer_t);
    opal_dss.pack(sample, &host_count, 1, OPAL_INT);
    opal_dss.pack(sample, &sampletime, 1, OPAL_TIMEVAL);
    // node_name
    opal_dss.pack(sample, &dummy_str, 1, OPAL_STRING);
    // host_ip
    opal_dss.pack(sample, &dummy_str, 1, OPAL_STRING);
    // bmcip
    opal_dss.pack(sample, &dummy_str, 1, OPAL_STRING);
    // manufacture date
    opal_dss.pack(sample, &dummy_str, 1, OPAL_STRING);
    // manufacturer name
    opal_dss.pack(sample, &dummy_str, 1, OPAL_STRING);
    // product name
    opal_dss.pack(sample, &dummy_str, 1, OPAL_STRING);
    // serial number
    opal_dss.pack(sample, &dummy_str, 1, OPAL_STRING);
    // part number
    opal_dss.pack(sample, &dummy_str, 1, OPAL_STRING);

    orcm_analytics_API_module_send_data_fn_t saved = orcm_analytics.send_data;
    orcm_analytics.send_data = ut_sensor_ipmi_tests::SendData;

    orcm_sensor_ipmi_module.log(sample);
    OBJ_RELEASE(sample);

    orcm_analytics.send_data = saved;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_log_zero_hosts_no_extra_info){
    int host_count = 0;
    opal_buffer_t *sample = NULL;
    struct timeval sampletime;

    sample = OBJ_NEW(opal_buffer_t);
    opal_dss.pack(sample, &host_count, 1, OPAL_INT);
    opal_dss.pack(sample, &sampletime, 1, OPAL_TIMEVAL);
    // notice the lack of node_name, host_ip, bmcip, etc...


    orcm_analytics_API_module_send_data_fn_t saved = orcm_analytics.send_data;
    orcm_analytics.send_data = ut_sensor_ipmi_tests::SendData;

    orcm_sensor_ipmi_module.log(sample);
    OBJ_RELEASE(sample);

    orcm_analytics.send_data = saved;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_inventory_collect_test_vector){
    opal_buffer_t *buff = NULL;

    mca_sensor_ipmi_component.test = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.start(0);
    orcm_sensor_ipmi_module.inventory_collect(buff);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();
    mca_sensor_ipmi_component.test = false;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_inventory_collect_no_test_vector){
    opal_buffer_t *buff = NULL;

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.start(0);
    orcm_sensor_ipmi_module.inventory_collect(buff);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();
}

TEST_F(ut_sensor_ipmi_tests, ipmi_inventory_log_zero_items){
    opal_buffer_t *buff = OBJ_NEW(opal_buffer_t);
    struct timeval current_time;
    unsigned int tot_items = 0;

    opal_dss.pack(buff, &current_time, 1, OPAL_TIMEVAL);
    opal_dss.pack(buff, &tot_items, 1, OPAL_UINT);

    mca_sensor_ipmi_component.test = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.start(0);
    orcm_sensor_ipmi_module.inventory_log((char*)"host", buff);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();
    mca_sensor_ipmi_component.test = false;

    OBJ_RELEASE(buff);
}

// This will provoke compare_ipmi_record to return false.. this might be an issue
// with ipmi_sensor implementation.
TEST_F(ut_sensor_ipmi_tests, ipmi_inventory_log_found_inventory_host_failed){
    opal_buffer_t *buff = OBJ_NEW(opal_buffer_t);
    struct timeval current_time;
    unsigned int tot_items = 4;
    const char *inv[4] = {"dummy", "bmc_ver", "ipmi_ver", "hostname"};
    const char *inv_val[4] = {"dummy_val", "bmc_ver_val", "ipmi_ver_val", "hostname_val"};

    // Pack inventory collection
    opal_dss.pack(buff, &current_time, 1, OPAL_TIMEVAL);
    opal_dss.pack(buff, &tot_items, 1, OPAL_UINT);
    for (int i=0; i<tot_items; ++i){
        opal_dss.pack(buff, &inv[i], 1, OPAL_STRING);
        opal_dss.pack(buff, &inv_val[i], 1, OPAL_STRING);
    }

    // disable the storage
    int saved = orcm_sensor_base.dbhandle;
    orcm_sensor_base.dbhandle = -1;

    mca_sensor_ipmi_component.test = true;

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.start(0);
    orcm_sensor_ipmi_module.inventory_log((char*)"host", buff);

    // Pack inventory collection again
    opal_dss.pack(buff, &current_time, 1, OPAL_TIMEVAL);
    opal_dss.pack(buff, &tot_items, 1, OPAL_UINT);
    for (int i=0; i<tot_items; ++i){
        opal_dss.pack(buff, &inv[i], 1, OPAL_STRING);
        opal_dss.pack(buff, &inv_val[i], 1, OPAL_STRING);
    }
    orcm_sensor_ipmi_module.inventory_log((char*)"host", buff);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();

    mca_sensor_ipmi_component.test = false;
    orcm_sensor_base.dbhandle = saved;

    OBJ_RELEASE(buff);
}

TEST_F(ut_sensor_ipmi_tests, ipmi_sample_using_progres_thread){
    orcm_sensor_sampler_t* sampler = OBJ_NEW(orcm_sensor_sampler_t);

    mca_sensor_ipmi_component.use_progress_thread = true;
    mca_sensor_ipmi_component.test = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.start(0);
    orcm_sensor_ipmi_module.sample(sampler);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();
    mca_sensor_ipmi_component.test = false;
    mca_sensor_ipmi_component.use_progress_thread = false;

    OBJ_RELEASE(sampler);
}

TEST_F(ut_sensor_ipmi_tests, ipmi_sample_no_test_vector_and_metrix_failure){
    orcm_sensor_sampler_t* sampler = OBJ_NEW(orcm_sensor_sampler_t);

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.start(0);
    orcm_sensor_ipmi_module.sample(sampler);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();

    OBJ_RELEASE(sampler);
}

TEST_F(ut_sensor_ipmi_tests, ipmi_sample_no_test_vector_and_zero_hosts_count)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("ipmi", false, false);
    mca_sensor_ipmi_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    mca_sensor_ipmi_component.collect_metrics = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "ipmi");
    collect_ipmi_sample(sampler);

    // Cleanup
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_ipmi_component.runtime_metrics = NULL;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_sample_no_test_vector_and_unable_to_get_bmc_info){
    orcm_sensor_sampler_t* sampler = OBJ_NEW(orcm_sensor_sampler_t);

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    void* object = orcm_sensor_base_runtime_metrics_create("ipmi", false, false);
    mca_sensor_ipmi_component.runtime_metrics = object;
    mca_sensor_ipmi_component.collect_metrics = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "ipmi");
    orcm_sensor_ipmi_module.start(0);

    is_get_bmc_info_expected_to_succeed = false;
    orcm_sensor_ipmi_module.sample(sampler);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();

    OBJ_RELEASE(sampler);
    mca_sensor_ipmi_component.runtime_metrics = NULL;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_sample_no_test_vector_all_success){
    orcm_sensor_sampler_t* sampler = OBJ_NEW(orcm_sensor_sampler_t);

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    void* object = orcm_sensor_base_runtime_metrics_create("ipmi", false, false);
    mca_sensor_ipmi_component.runtime_metrics = object;
    mca_sensor_ipmi_component.collect_metrics = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "ipmi");
    orcm_sensor_ipmi_module.start(0);

    orcm_sensor_ipmi_module.sample(sampler);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();

    OBJ_RELEASE(sampler);
    mca_sensor_ipmi_component.runtime_metrics = NULL;
}

TEST_F(ut_sensor_ipmi_tests, orcm_sensor_ipmi_found_success){
    const char* nodename = "hostname";
    ipmi_collector ic;
    opal_list_t *list = NULL;

    strncpy(ic.bmc_address, "192.168.1.1", MAX_STR_LEN-1);
    ic.bmc_address[MAX_STR_LEN-1] = '\0';
    strncpy(ic.user, "user", MAX_STR_LEN-1);
    ic.user[MAX_STR_LEN-1] = '\0';
    strncpy(ic.pass, "pass", MAX_STR_LEN-1);
    ic.pass[MAX_STR_LEN-1] = '\0';
    strncpy(ic.aggregator, "aggregator", MAX_STR_LEN-1);
    ic.aggregator[MAX_STR_LEN-1] = '\0';
    strncpy(ic.hostname, "hostname", MAX_STR_LEN-1);
    ic.hostname[MAX_STR_LEN-1] = '\0';
    ic.auth_method = 3;
    ic.priv_level  = 3;
    ic.port        = 2000;
    ic.channel     = 3;

    list = OBJ_NEW(opal_list_t);

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_addhost(&ic, list));
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_found((char*)nodename, list));
    OBJ_RELEASE(list);
}

TEST_F(ut_sensor_ipmi_tests, orcm_sensor_ipmi_found_fails){
    opal_list_t *list = OBJ_NEW(opal_list_t);
    EXPECT_EQ(ORCM_ERR_NOT_FOUND, orcm_sensor_ipmi_found((char*)"test", list));
    OBJ_RELEASE(list);
}

TEST_F(ut_sensor_ipmi_tests, orcm_sensor_ipmi_label_found_false){
    opal_list_t *list = OBJ_NEW(opal_list_t);
    EXPECT_EQ(0, orcm_sensor_ipmi_label_found((char*)"test"));
    OBJ_RELEASE(list);
}

TEST_F(ut_sensor_ipmi_tests, orcm_sensor_ipmi_label_found_true){
    opal_list_t *list = OBJ_NEW(opal_list_t);
    const char* sensor = "test";

    mca_sensor_ipmi_component.sensor_list = (char*)sensor;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.start(0);

    EXPECT_EQ(1, orcm_sensor_ipmi_label_found((char*)sensor));

    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();
    OBJ_RELEASE(list);
}

TEST_F(ut_sensor_ipmi_tests, ipmi_sample_using_perthread){
    mca_sensor_ipmi_component.use_progress_thread = true;

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.set_sample_rate(1);
    orcm_sensor_ipmi_module.start(0);
    sleep(5);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();

    mca_sensor_ipmi_component.use_progress_thread = false;
}

TEST_F(ut_sensor_ipmi_tests, ipmi_sample_using_perthread_with_new_sample_rate){
    mca_sensor_ipmi_component.use_progress_thread = true;
    mca_sensor_ipmi_component.test = true;

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_ipmi_module.init());
    orcm_sensor_ipmi_module.set_sample_rate(1);
    orcm_sensor_ipmi_module.start(0);
    sleep(5);
    orcm_sensor_ipmi_module.set_sample_rate(2);
    sleep(5);
    orcm_sensor_ipmi_module.stop(0);
    orcm_sensor_ipmi_module.finalize();

    mca_sensor_ipmi_component.test = false;
    mca_sensor_ipmi_component.use_progress_thread = false;
}

TEST_F(ut_sensor_ipmi_tests, orcm_sensor_ipmi_get_system_power_state_test){
    char str[256];
    int val[15] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x0A, 0x20, 0x21, 0x2A, 0xFF};
    for (int i=0; i<15; i++){
        orcm_sensor_ipmi_get_system_power_state(val[i], str, sizeof(str));
        switch(val[i]) {
            case 0x0:   ASSERT_EQ(0, strcmp(str, "S0/G0")); break;
            case 0x1:   ASSERT_EQ(0, strcmp(str,"S1")); break;
            case 0x2:   ASSERT_EQ(0, strcmp(str,"S2")); break;
            case 0x3:   ASSERT_EQ(0, strcmp(str,"S3")); break;
            case 0x4:   ASSERT_EQ(0, strcmp(str,"S4")); break;
            case 0x5:   ASSERT_EQ(0, strcmp(str,"S5/G2")); break;
            case 0x6:   ASSERT_EQ(0, strcmp(str,"S4/S5")); break;
            case 0x7:   ASSERT_EQ(0, strcmp(str,"G3")); break;
            case 0x8:   ASSERT_EQ(0, strcmp(str,"sleeping")); break;
            case 0x9:   ASSERT_EQ(0, strcmp(str,"G1 sleeping")); break;
            case 0x0A:  ASSERT_EQ(0, strcmp(str,"S5 override")); break;
            case 0x20:  ASSERT_EQ(0, strcmp(str,"Legacy On")); break;
            case 0x21:  ASSERT_EQ(0, strcmp(str,"Legacy Off")); break;
            case 0x2A:  ASSERT_EQ(0, strcmp(str,"Unknown")); break;
            default:    ASSERT_EQ(0, strcmp(str,"Illegal")); break;
        }
    }
}

TEST_F(ut_sensor_ipmi_tests, orcm_sensor_ipmi_get_device_power_state_test){
    char str[256];
    int val[6] = {0x0, 0x1, 0x2, 0x3, 0x4, 0xFF};
    for (int i=0; i<6; i++){
        orcm_sensor_ipmi_get_device_power_state(val[i], str, sizeof(str));
        switch(val[i]) {
            case 0x0:   ASSERT_EQ(0, strcmp(str,"D0")); break;
            case 0x1:   ASSERT_EQ(0, strcmp(str,"D1")); break;
            case 0x2:   ASSERT_EQ(0, strcmp(str,"D2")); break;
            case 0x3:   ASSERT_EQ(0, strcmp(str,"D3")); break;
            case 0x4:   ASSERT_EQ(0, strcmp(str,"Unknown")); break;
            default:    ASSERT_EQ(0, strcmp(str,"Illegal")); break;
        }
    }
}
