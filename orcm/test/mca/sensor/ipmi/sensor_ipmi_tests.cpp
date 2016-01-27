/*
 * Copyright (c) 2015-2016      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sensor_ipmi_tests.h"
#include "sensor_ipmi_sel_mocked_functions.h"

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
    extern void collect_ipmi_sample(orcm_sensor_sampler_t *sampler);

    extern void test_error_sink_c(int level, const char* msg);
    extern void test_ras_event_c(const char* decoded_msg, const char* hostname, void* user_object);

    #include "opal/class/opal_list.h"
    #include "orcm/mca/sensor/ipmi/sensor_ipmi_decls.h"
    #include "orcm/mca/sensor/ipmi/sensor_ipmi.h"
    extern void orcm_sensor_ipmi_get_sel_events(ipmi_capsule_t* capsule);
} // extern "C"

using namespace std;

// Fixture methods
void ut_sensor_ipmi_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();
}

void ut_sensor_ipmi_tests::TearDownTestCase()
{
    // Release OPAL level resources
    opal_dss_close();
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
    void* object = orcm_sensor_base_runtime_metrics_create(false, false);
    mca_sensor_ipmi_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    // Tests
    collect_ipmi_sample(sampler);
    EXPECT_EQ(0, (mca_sensor_ipmi_component.diagnostics & 0x1));

    orcm_sensor_base_runtime_metrics_set(object, true);
    mca_sensor_ipmi_component.test = true;
    collect_ipmi_sample(sampler);
    mca_sensor_ipmi_component.test = false;
    EXPECT_EQ(1, (mca_sensor_ipmi_component.diagnostics & 0x1));

    // Cleanup
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_ipmi_component.runtime_metrics = NULL;
}
