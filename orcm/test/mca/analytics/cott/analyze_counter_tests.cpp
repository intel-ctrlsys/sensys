/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "analyze_counter_tests.h"

// ORTE
#include "orte/mca/notifier/notifier.h"

// ORCM
extern "C" {
#include "orcm/util/utils.h"
}

// C
#include <unistd.h>

// C++
#include <sstream>
#include <iomanip>

#define GTEST_MOCK_TESTING
#include "orcm/mca/analytics/cott/analytics_cott.h"
#include "orcm/mca/analytics/cott/analyze_counter.h"
#include "orcm/mca/analytics/cott/host_analyze_counters.h"
#undef GTEST_MOCK_TESTING
#include "cott_mocking.h"


// Additional gtest asserts...
#define ASSERT_PTREQ(x,y)  ASSERT_EQ((void*)x,(void*)y)
#define ASSERT_PTRNE(x,y)  ASSERT_NE((void*)x,(void*)y)
#define ASSERT_NULL(x)     ASSERT_PTREQ(NULL,x)
#define ASSERT_NOT_NULL(x) ASSERT_PTRNE(NULL,x)
#define EXPECT_PTREQ(x,y)  EXPECT_EQ((void*)x,(void*)y)
#define EXPECT_PTRNE(x,y)  EXPECT_NE((void*)x,(void*)y)
#define EXPECT_NULL(x)     EXPECT_PTREQ(NULL,x)
#define EXPECT_NOT_NULL(x) EXPECT_PTRNE(NULL,x)

#define SAFE_FREE(x) if(NULL != x) free((void*)x); x=NULL
#define SAFE_OBJ_RELEASE(x) if(NULL!=x) OBJ_RELEASE(x); x=NULL
#define SAFE_DELETE(x) if(NULL != x) delete x; x=NULL

#define CM_PART1                "CPU_SrcID"
#define CM_PART2                "_Channel"
#define CM_PART3                "_DIMM"
#define CM_PART4                "_CE"
#define COMPLEX_MASK            "CPU_SrcID*_Channel*_DIMM*_CE"
#define COMPLEX_LABEL(x,y,z)    ((char*)"CPU_SrcID##x##_Channel##y##_DIMM##z##_CE")

uint32_t analyze_counter_tests::last_event_count_ = 0;
time_t analyze_counter_tests::last_event_elapsed_ = 0;
std::vector<uint32_t> analyze_counter_tests::last_event_raw_data_;
std::string analyze_counter_tests::last_event_host_;
std::string analyze_counter_tests::last_event_label_;
uint32_t analyze_counter_tests::last_callback_count_ = 0;
std::queue<bool> analyze_counter_tests::fail_malloc_call_;
assert_caddy_data_fn_t analyze_counter_tests::saved_assert_caddy_data_hook_ = NULL;

using namespace std;

extern int cott_init(orcm_analytics_base_module_t *imod);
extern void cott_finalize(orcm_analytics_base_module_t *imod);
extern int cott_analyze(int sd, short args, void *cbdata);
extern void cott_event_fired_callback(const string& host, const string& label, uint32_t count,
                                      time_t elapsed, const vector<uint32_t>& data, void* cb_data);
extern "C" {
    extern orcm_analytics_base_module_t* cott_component_create(void);
    extern bool cott_component_avail(void);
}
extern string get_hostname_from_caddy(orcm_workflow_caddy_t* caddy);
extern time_t get_timeval_from_caddy(orcm_workflow_caddy_t* caddy);
extern void add_data_values(const string& hostname, time_t tv, orcm_workflow_caddy_t* caddy,
                            opal_list_t* threshold_list);
extern int assert_caddy_data(void *cbdata);


extern host_analyze_counters* counter_analyzer;
extern map<string,int> lookup_fault;
extern map<string,bool> lookup_store;

struct test_step_data
{
    int fault_type;
    bool store_event;
    int severity;
    bool use_notifier;
    string notifier_action;
};

// gtest setup
void analyze_counter_tests::SetUpTestCase()
{
    ResetTestCase();

    saved_assert_caddy_data_hook_ = assert_caddy_data_hook;
    assert_caddy_data_hook = AssertCaddyData;
}

void analyze_counter_tests::TearDownTestCase()
{
    assert_caddy_data_hook = saved_assert_caddy_data_hook_;
    saved_assert_caddy_data_hook_ = NULL;

    ResetTestCase();
}

void analyze_counter_tests::ResetTestCase()
{
    cott_mocking.malloc_callback = Malloc;
    cott_mocking.opal_hash_table_init_callback = NULL;

    last_event_count_ = 0;
    last_event_elapsed_ = 0;
    last_event_raw_data_.clear();
    last_event_host_ = "";
    last_event_label_ = "";
    last_callback_count_ = 0;

    while(0 != fail_malloc_call_.size()) {
        fail_malloc_call_.pop();
    }
}

// Callbacks
void analyze_counter_tests::RasEventFired(uint32_t count, time_t elapsed, const std::vector<uint32_t>& data, void* cb_data)
{
    (void)cb_data;
    last_event_count_ = count;
    last_event_elapsed_ = elapsed;
    last_event_raw_data_ = data;
}

void analyze_counter_tests::RasHostEventFired(const std::string& host, const std::string& label, uint32_t count,
                                              time_t elapsed, const std::vector<uint32_t>& data, void* cb_data)
{
    (void)cb_data;
    last_event_count_ = count;
    last_event_elapsed_ = elapsed;
    last_event_raw_data_ = data;
    last_event_host_ = host;
    last_event_label_ = label;
    ++last_callback_count_;
}


// Mocking
void* analyze_counter_tests::Malloc(size_t size)
{
    if(0 != fail_malloc_call_.size() && true == fail_malloc_call_.front()) {
        fail_malloc_call_.pop();
        return NULL;
    } else {
        if(0 != fail_malloc_call_.size()) {
            fail_malloc_call_.pop();
        }
        return __real_malloc(size);
    }
}

int analyze_counter_tests::OpalHashTableInit(opal_hash_table_t* ht, size_t table_size)
{
    return ORCM_ERROR;
}

void analyze_counter_tests::ActivateNextWF(orcm_workflow_caddy_t* caddy)
{
}

int analyze_counter_tests::AssertCaddyData(void* cbdata)
{
    if (NULL == cbdata) {
        return ORCM_ERR_BAD_PARAM;
    } else {
        orcm_workflow_caddy_t* caddy = (orcm_workflow_caddy_t *)cbdata;
        if (NULL == caddy->imod || NULL == caddy->analytics_value) {
            return ORCM_ERR_BAD_PARAM;
        }
    }
    return ORCM_SUCCESS;
}

// Helper Methods
orcm_workflow_caddy_t* analyze_counter_tests::MakeCaddy(bool include_hostname, bool include_ts,
                                                        bool include_large_count,
                                                        const char* str_threshold,
                                                        const char* store, const char* action,
                                                        const char* severity, const char* fault)
{
    static time_t base_time = time(NULL);
    static time_t fast_time = 0;

    orcm_workflow_caddy_t* caddy = OBJ_NEW(orcm_workflow_caddy_t);
    opal_list_t* key = OBJ_NEW(opal_list_t);
    opal_list_t* non_compute = OBJ_NEW(opal_list_t);
    opal_list_t* compute = OBJ_NEW(opal_list_t);
    caddy->analytics_value = orcm_util_load_orcm_analytics_value(key, non_compute, compute);
    caddy->wf = OBJ_NEW(orcm_workflow_t);
    caddy->wf_step = OBJ_NEW(orcm_workflow_step_t);

    opal_value_t* attr = orcm_util_load_opal_value((char*)"label_mask", (void*)"CPU_SrcID#*_Channel#*_DIMM#*_CE", OPAL_STRING);
    opal_list_append(&caddy->wf_step->attributes, &attr->super);

    attr = orcm_util_load_opal_value((char*)"time_window", (void*)"2m", OPAL_STRING);
    opal_list_append(&caddy->wf_step->attributes, &attr->super);

    attr = orcm_util_load_opal_value((char*)"count_threshold", (void*)str_threshold, OPAL_STRING);
    opal_list_append(&caddy->wf_step->attributes, &attr->super);

    attr = orcm_util_load_opal_value((char*)"store_event", (void*)store, OPAL_STRING);
    opal_list_append(&caddy->wf_step->attributes, &attr->super);

    attr = orcm_util_load_opal_value((char*)"notifier_action", (void*)action, OPAL_STRING);
    opal_list_append(&caddy->wf_step->attributes, &attr->super);

    attr = orcm_util_load_opal_value((char*)"severity", (void*)severity, OPAL_STRING);
    opal_list_append(&caddy->wf_step->attributes, &attr->super);

    attr = orcm_util_load_opal_value((char*)"fault_type", (void*)fault, OPAL_STRING);
    opal_list_append(&caddy->wf_step->attributes, &attr->super);

    attr = orcm_util_load_opal_value((char*)"other_name", (void*)"other_value", OPAL_STRING);
    opal_list_append(&caddy->wf_step->attributes, &attr->super);

    orcm_value_t* val = NULL;
    if(true == include_hostname) {
        val = orcm_util_load_orcm_value((char*)"hostname", (void*)"test_host", OPAL_STRING, NULL);
        opal_list_append(caddy->analytics_value->key, &val->value.super);
    } else {
        val = orcm_util_load_orcm_value((char*)"somekey", (void*)"value", OPAL_STRING, NULL);
        opal_list_append(caddy->analytics_value->key, &val->value.super);
    }

    if(true == include_ts) {
        val = OBJ_NEW(orcm_value_t);
        val->value.key = strdup("ctime");
        val->value.type = OPAL_TIMEVAL;
        memset((void*)&val->value.data.tv, 0, sizeof(struct timeval));
        val->value.data.tv.tv_sec = base_time + (fast_time);
        fast_time += 60; // every call adds 60 seconds to time stamp
        opal_list_append(caddy->analytics_value->non_compute_data, &val->value.super);
    } else {
        val = orcm_util_load_orcm_value((char*)"some_non_compute", (void*)"non_value", OPAL_STRING, NULL);
        opal_list_append(caddy->analytics_value->non_compute_data, &val->value.super);
    }

    // Matched values....
    for(int i = 0; i < 2; ++i) {
        val = orcm_util_load_orcm_value(COMPLEX_LABEL(0,0,0), (void*)&i, OPAL_INT32, NULL);
        opal_list_append(caddy->analytics_value->compute_data, &val->value.super);
    }

    // Non-matched values...
    for(int i = 2; i < 4; ++i) {
        val = orcm_util_load_orcm_value((char*)"label1", (void*)&i, OPAL_INT32, NULL);
        opal_list_append(caddy->analytics_value->compute_data, &val->value.super);
    }

    // Large jump to trigger event...
    if(true == include_large_count) {
        int32_t n = 4 + 100;
        val = orcm_util_load_orcm_value(COMPLEX_LABEL(0,0,0), (void*)&n, OPAL_INT32, NULL);
        opal_list_append(caddy->analytics_value->compute_data, &val->value.super);
    }

    return caddy;
}

void analyze_counter_tests::DestroyCaddy(orcm_workflow_caddy_t*& caddy)
{
    if(NULL != caddy->analytics_value) {
        SAFE_OBJ_RELEASE(caddy->analytics_value->key);
        SAFE_OBJ_RELEASE(caddy->analytics_value->non_compute_data);
        SAFE_OBJ_RELEASE(caddy->analytics_value->compute_data);
        SAFE_OBJ_RELEASE(caddy->analytics_value);
    }
    SAFE_OBJ_RELEASE(caddy->wf);
    SAFE_OBJ_RELEASE(caddy->wf_step);
    SAFE_OBJ_RELEASE(caddy);
}


// Tests
TEST_F(analyze_counter_tests, test_set_window_size)
{
    analyze_counter counter;

    ASSERT_EQ(1, counter.window_size_);

    counter.set_window_size(string(""));
    ASSERT_EQ(1, counter.window_size_);

    counter.set_window_size(string("5s"));
    ASSERT_EQ(5, counter.window_size_);

    counter.set_window_size(string("5m"));
    ASSERT_EQ(300, counter.window_size_);

    counter.set_window_size(string("5h"));
    ASSERT_EQ(3600 * 5, counter.window_size_);

    counter.set_window_size(string("10d"));
    ASSERT_EQ(24 * 3600 * 10, counter.window_size_);

    counter.set_window_size(string("   garbage   "));
    ASSERT_EQ(24 * 3600 * 10, counter.window_size_); // Bad input = no change

    counter.set_window_size(string(" \t  2s  \n"));
    ASSERT_EQ(2, counter.window_size_); // Bad input = no change

    counter.set_window_size(string("0s"));
    ASSERT_EQ(2, counter.window_size_); // Must be 1 or more!

    counter.set_window_size(string(" \t \r   \n"));
    ASSERT_EQ(2, counter.window_size_);

    counter.set_window_size(string(" \t \r s  \n"));
    ASSERT_EQ(2, counter.window_size_);
}

TEST_F(analyze_counter_tests, test_set_threshold)
{
    analyze_counter counter;

    counter.set_threshold(10);
    ASSERT_EQ(10, counter.threshold_);

    counter.set_threshold(0);
    ASSERT_EQ(10, counter.threshold_);
}

TEST_F(analyze_counter_tests, test_add_value_1)
{
    ResetTestCase();
    analyze_counter counter;

    counter.set_window_size("5s");
    counter.set_threshold(10);

    counter.add_value(3, 1, RasEventFired, NULL);
    ASSERT_EQ(1, counter.past_data_.size());
    counter.add_value(6, 2, RasEventFired, NULL);
    ASSERT_EQ(2, counter.past_data_.size());
    counter.add_value(9, 3, RasEventFired, NULL);
    ASSERT_EQ(3, counter.past_data_.size());
    counter.add_value(12, 4, RasEventFired, NULL);
    ASSERT_EQ(4, counter.past_data_.size());
    counter.add_value(16, 4, RasEventFired, NULL);
    ASSERT_EQ(1, counter.past_data_.size());
    ASSERT_EQ(13, last_event_count_);
    ASSERT_EQ(3, last_event_elapsed_);
    ASSERT_EQ(5, last_event_raw_data_.size());
}

TEST_F(analyze_counter_tests, test_add_value_2)
{
    ResetTestCase();
    analyze_counter counter;

    counter.set_window_size("5s");
    counter.set_threshold(10);

    counter.add_value(3, 1, RasEventFired, NULL);
    ASSERT_EQ(1, counter.past_data_.size());
    counter.add_value(6, 2, RasEventFired, NULL);
    ASSERT_EQ(2, counter.past_data_.size());
    counter.add_value(9, 3, RasEventFired, NULL);
    ASSERT_EQ(3, counter.past_data_.size());
    counter.add_value(12, 4, RasEventFired, NULL);
    ASSERT_EQ(4, counter.past_data_.size());
    counter.add_value(15, 7, RasEventFired, NULL);
    ASSERT_EQ(4, counter.past_data_.size());
    ASSERT_EQ(0, last_event_count_);
    ASSERT_EQ(0, last_event_elapsed_);
    ASSERT_EQ(0, last_event_raw_data_.size());
}

TEST_F(analyze_counter_tests, test_host_add_value)
{
    ResetTestCase();
    host_analyze_counters counter;

    counter.set_window_size("5s");
    counter.set_threshold(10);
    ASSERT_EQ(10, counter.get_threshold());

    counter.add_value("test_host_1", "label0", 3, 1, RasHostEventFired, NULL);
    counter.add_value("test_host_1", "label0", 6, 2, RasHostEventFired, NULL);
    counter.add_value("test_host_1", "label0", 9, 3, RasHostEventFired, NULL);
    counter.add_value("test_host_1", "label0", 12, 4, RasHostEventFired, NULL);
    counter.add_value("test_host_1", "label0", 16, 4, RasHostEventFired, NULL);
    for(int i = 0; i < 100; ++i) {
        for(int j = 0; j < 8; ++j) {
            stringstream ss;
            ss << "node_" << setw(5) << setfill('0') << i;
            string host = ss.str();
            ss.str("");
            ss << "label_" << setw(1) << setfill(' ') << j;
            string label = ss.str();
            counter.add_value(host, label, 0, i, RasHostEventFired, NULL);
        }
    }
    ASSERT_EQ(13, last_event_count_);
    ASSERT_EQ(3, last_event_elapsed_);
    ASSERT_EQ(5, last_event_raw_data_.size());
    ASSERT_STREQ("test_host_1", last_event_host_.c_str());
    ASSERT_STREQ("label0", last_event_label_.c_str());
    ASSERT_EQ(1, last_callback_count_);
    ASSERT_EQ(101, counter.data_db_.size());

    counter.set_window_size("4h");
    counter.set_threshold(100);
    ASSERT_STREQ("4h", counter.window_size_.c_str());
    ASSERT_EQ(100, counter.threshold_);
    for(int i = 0; i < 100; ++i) {
        for(int j = 0; j < 8; ++j) {
            stringstream ss;
            ss << "node_" << setw(5) << setfill('0') << i;
            string host = ss.str();
            ss.str("");
            ss << "label_" << setw(1) << setfill(' ') << j;
            string label = ss.str();
            counter.add_value(host, label, 20, j * 2, RasHostEventFired, NULL);
        }
    }
    ASSERT_EQ(101, counter.data_db_.size());
    ASSERT_EQ(1, last_callback_count_);
}

TEST_F(analyze_counter_tests, test_host_add_value_negative)
{
    ResetTestCase();
    host_analyze_counters counter;

    counter.set_window_size("5s");
    counter.set_threshold(10);
    counter.set_value_label_mask("label**");
    ASSERT_STREQ("label**", counter.label_mask_.c_str());

    counter.add_value("test_host_1", "label0", 2, 2, NULL, NULL);
    ASSERT_EQ(0, last_callback_count_);
    counter.add_value("test_host_1", "label0", 20, 3, NULL, NULL);
    ASSERT_EQ(0, last_callback_count_);

    counter.add_value("test_host_1", "", 2, 2, RasHostEventFired, NULL);
    ASSERT_EQ(0, last_callback_count_);
    counter.add_value("", "label0", 4, 4, RasHostEventFired, NULL);
    ASSERT_EQ(0, last_callback_count_);
    counter.add_value("", "", 6, 6, RasHostEventFired, NULL);
    ASSERT_EQ(0, last_callback_count_);
    counter.add_value("test_host_1", "label2", 8, 20, RasHostEventFired, NULL);
    ASSERT_EQ(0, last_callback_count_);
    counter.add_value("test_host_1", "label2", 10, 100, RasHostEventFired, NULL);
    ASSERT_EQ(0, last_callback_count_);
}

TEST_F(analyze_counter_tests, test_host_is_wanted_label)
{
    ResetTestCase();
    host_analyze_counters counter;

    counter.set_value_label_mask(COMPLEX_MASK);
    ASSERT_STREQ(COMPLEX_MASK, counter.label_mask_.c_str());

    ASSERT_EQ(4, counter.mask_parts_.size());
    ASSERT_STREQ(CM_PART1, counter.mask_parts_[0].c_str());
    ASSERT_STREQ(CM_PART2, counter.mask_parts_[1].c_str());
    ASSERT_STREQ(CM_PART3, counter.mask_parts_[2].c_str());
    ASSERT_STREQ(CM_PART4, counter.mask_parts_[3].c_str());

    ASSERT_TRUE(counter.is_wanted_label(COMPLEX_LABEL(1,3,0)));
    ASSERT_TRUE(counter.is_wanted_label(COMPLEX_LABEL(0,0,1)));

    // Negative tests
    ASSERT_FALSE(counter.is_wanted_label(""));
    ASSERT_FALSE(counter.is_wanted_label("CPU_SrcID0_Sum_DIMM0_UE"));
    ASSERT_FALSE(counter.is_wanted_label("CPU_SrcID0_Sum_DIMM0_CE"));
    ASSERT_FALSE(counter.is_wanted_label("CPU_SrcID1_Chanel3_DIMM0_CE"));
    ASSERT_FALSE(counter.is_wanted_label("CPU_ScID1_Channel3_DIMM0_CE"));
    ASSERT_FALSE(counter.is_wanted_label("CPU_SrcID1_Channel3_DIM0_CE"));
    ASSERT_FALSE(counter.is_wanted_label("CPU_SrcID1_Channel3_DIMM0_UE"));

    counter.set_value_label_mask(COMPLEX_LABEL(0,0,0));
    ASSERT_TRUE(counter.is_wanted_label(COMPLEX_LABEL(0,0,0)));

    counter.set_value_label_mask("*");
    ASSERT_TRUE(counter.is_wanted_label(COMPLEX_LABEL(0,0,0)));
}

TEST_F(analyze_counter_tests, test_host_add_value_stress)
{
    ResetTestCase();
    host_analyze_counters counter;

    counter.set_window_size("5s");
    counter.set_threshold(10);

    counter.add_value("test_host_1", "label0", 3, 1, RasHostEventFired, NULL);
    counter.add_value("test_host_1", "label0", 6, 2, RasHostEventFired, NULL);
    counter.add_value("test_host_1", "label0", 9, 3, RasHostEventFired, NULL);
    counter.add_value("test_host_1", "label0", 12, 4, RasHostEventFired, NULL);
    counter.add_value("test_host_1", "label0", 16, 4, RasHostEventFired, NULL);
    for(int i = 0; i < 10000; ++i) {
        for(int j = 0; j < 8; ++j) {
            stringstream ss;
            ss << "node_" << setw(6) << setfill('0') << i;
            string host = ss.str();
            ss.str("");
            ss << "label_" << setw(1) << setfill(' ') << j;
            string label = ss.str();
            counter.add_value(host, label, 0, i, RasHostEventFired, NULL);
        }
    }
    ASSERT_EQ(13, last_event_count_);
    ASSERT_EQ(3, last_event_elapsed_);
    ASSERT_EQ(5, last_event_raw_data_.size());
    ASSERT_STREQ("test_host_1", last_event_host_.c_str());
    ASSERT_STREQ("label0", last_event_label_.c_str());
    ASSERT_EQ(1, last_callback_count_);
    ASSERT_EQ(10001, counter.data_db_.size());

    counter.set_window_size("4h");
    counter.set_threshold(100);
    ASSERT_STREQ("4h", counter.window_size_.c_str());
    ASSERT_EQ(100, counter.threshold_);
    for(int i = 0; i < 10000; ++i) {
        for(int j = 0; j < 8; ++j) {
            stringstream ss;
            ss << "node_" << setw(6) << setfill('0') << i;
            string host = ss.str();
            ss.str("");
            ss << "label_" << setw(1) << setfill(' ') << j;
            string label = ss.str();
            counter.add_value(host, label, 20, i + 10001, RasHostEventFired, NULL);
        }
    }
    ASSERT_EQ(10001, counter.data_db_.size());
    ASSERT_EQ(1, last_callback_count_);
}

TEST_F(analyze_counter_tests, test_component_init_finalize)
{
    ResetTestCase();

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));

    ASSERT_NOT_NULL(mod);
    ASSERT_NE(ORCM_SUCCESS, cott_init(NULL));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    ASSERT_EQ(2, lookup_fault.size());
    ASSERT_EQ(6, lookup_store.size());

    cott_finalize(NULL);
    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_component_obj_new_out_of_mem)
{
    ResetTestCase();

    mca_analytics_cott_module_t* mod = (mca_analytics_cott_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_NOT_NULL(mod);

    fail_malloc_call_.push(true);
    ASSERT_NE(ORCM_SUCCESS, cott_init((orcm_analytics_base_module_t*)mod));
    ASSERT_NULL(mod->api.orcm_mca_analytics_data_store);
    SAFE_FREE(mod);
}

TEST_F(analyze_counter_tests, test_component_hash_init_failure)
{
    ResetTestCase();

    mca_analytics_cott_module_t* mod = (mca_analytics_cott_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_NOT_NULL(mod);

    cott_mocking.opal_hash_table_init_callback = OpalHashTableInit;
    ASSERT_NE(ORCM_SUCCESS, cott_init((orcm_analytics_base_module_t*)mod));
    cott_mocking.opal_hash_table_init_callback = NULL;

    ASSERT_NULL(mod->api.orcm_mca_analytics_data_store);
    SAFE_FREE(mod);
}

TEST_F(analyze_counter_tests, test_cott_component_create)
{
    ResetTestCase();

    fail_malloc_call_.push(true);
    ASSERT_NULL(cott_component_create());

    cott_mocking.opal_hash_table_init_callback = OpalHashTableInit;
    ASSERT_NULL(cott_component_create());
    cott_mocking.opal_hash_table_init_callback = NULL;

    orcm_analytics_base_module_t* mod = cott_component_create();
    ASSERT_NOT_NULL(mod);
    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_get_hostname_from_caddy)
{
    ResetTestCase();

    orcm_workflow_caddy_t* caddy = MakeCaddy(true, false, false);
    ASSERT_STREQ("test_host", get_hostname_from_caddy(caddy).c_str());
    DestroyCaddy(caddy);

    caddy = MakeCaddy(false, false, false);
    ASSERT_STREQ("", get_hostname_from_caddy(caddy).c_str());
    DestroyCaddy(caddy);
}

TEST_F(analyze_counter_tests, test_get_timeval_from_caddy)
{
    ResetTestCase();

    orcm_workflow_caddy_t* caddy = MakeCaddy(false, true, false);
    ASSERT_NE(0, get_timeval_from_caddy(caddy));
    DestroyCaddy(caddy);

    caddy = MakeCaddy(false, false, false);
    ASSERT_EQ(0, get_timeval_from_caddy(caddy));
    DestroyCaddy(caddy);
}

TEST_F(analyze_counter_tests, test_add_data_values)
{
    ResetTestCase();

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(200);
    counter_analyzer->set_window_size("4m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy();

    string hostname = get_hostname_from_caddy(caddy);
    ASSERT_STREQ("test_host", hostname.c_str());

    time_t tv = get_timeval_from_caddy(caddy);
    ASSERT_NE(0, tv);

    opal_list_t* threshold_list = OBJ_NEW(opal_list_t);
    add_data_values(hostname, tv, caddy, threshold_list);
    string label = COMPLEX_LABEL(0,0,0);
    ASSERT_EQ(3, counter_analyzer->data_db_[hostname][label]->past_data_.size());
    ASSERT_EQ(0, counter_analyzer->data_db_[hostname][label]->past_data_[0].first);
    ASSERT_EQ(1, counter_analyzer->data_db_[hostname][label]->past_data_[1].first);
    ASSERT_EQ(104, counter_analyzer->data_db_[hostname][label]->past_data_[2].first);
    ASSERT_EQ(0, opal_list_get_size(threshold_list));
}

TEST_F(analyze_counter_tests, test_cott_component_avail)
{
    ResetTestCase();

    ASSERT_TRUE(cott_component_avail());
}

TEST_F(analyze_counter_tests, test_assert_caddy_data)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, assert_caddy_data(NULL));
    orcm_workflow_caddy_t* caddy = MakeCaddy();
    ASSERT_NE(ORCM_SUCCESS, assert_caddy_data(caddy));
}

TEST_F(analyze_counter_tests, test_analyze_1)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, NULL));

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(50);
    counter_analyzer->set_window_size("1m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy();
    caddy->imod = (orcm_analytics_base_module_t*)mod;

    ASSERT_EQ(ORCM_SUCCESS, cott_analyze(0, 0, (void*)caddy));

    // Check that wf arguments get to the class
    ASSERT_STREQ("CPU_SrcID#*_Channel#*_DIMM#*_CE", counter_analyzer->label_mask_.c_str());
    ASSERT_STREQ("2m", counter_analyzer->window_size_.c_str());
    ASSERT_EQ(100, counter_analyzer->threshold_);

    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_analyze_2)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, NULL));

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(50);
    counter_analyzer->set_window_size("1m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy(false);
    caddy->imod = (orcm_analytics_base_module_t*)mod;

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, (void*)caddy));

    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_analyze_3)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, NULL));

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(50);
    counter_analyzer->set_window_size("1m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy(true,false);
    caddy->imod = (orcm_analytics_base_module_t*)mod;

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, (void*)caddy));

    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_analyze_4)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, NULL));

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(50);
    counter_analyzer->set_window_size("1m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy(true, true, true, "10000000000");
    caddy->imod = (orcm_analytics_base_module_t*)mod;

    ASSERT_EQ(ORCM_SUCCESS, cott_analyze(0, 0, (void*)caddy));

    // Check that wf arguments get to the class
    ASSERT_STREQ("CPU_SrcID#*_Channel#*_DIMM#*_CE", counter_analyzer->label_mask_.c_str());
    ASSERT_STREQ("2m", counter_analyzer->window_size_.c_str());
    ASSERT_EQ(1, counter_analyzer->threshold_);

    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_analyze_5)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, NULL));

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(50);
    counter_analyzer->set_window_size("1m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy(true, true, true, "4294967296");
    caddy->imod = (orcm_analytics_base_module_t*)mod;

    ASSERT_EQ(ORCM_SUCCESS, cott_analyze(0, 0, (void*)caddy));

    // Check that wf arguments get to the class
    ASSERT_STREQ("CPU_SrcID#*_Channel#*_DIMM#*_CE", counter_analyzer->label_mask_.c_str());
    ASSERT_STREQ("2m", counter_analyzer->window_size_.c_str());
    ASSERT_EQ(1, counter_analyzer->threshold_);

    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_analyze_6)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, NULL));

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(50);
    counter_analyzer->set_window_size("1m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy(true, true, true, "-1");
    caddy->imod = (orcm_analytics_base_module_t*)mod;

    ASSERT_EQ(ORCM_SUCCESS, cott_analyze(0, 0, (void*)caddy));

    // Check that wf arguments get to the class
    ASSERT_STREQ("CPU_SrcID#*_Channel#*_DIMM#*_CE", counter_analyzer->label_mask_.c_str());
    ASSERT_STREQ("2m", counter_analyzer->window_size_.c_str());
    ASSERT_EQ(1, counter_analyzer->threshold_);

    test_step_data* data = (test_step_data*)counter_analyzer->get_user_data();
    ASSERT_EQ(ORCM_EVENT_HARD_FAULT, data->fault_type);
    ASSERT_TRUE(data->store_event);
    ASSERT_STREQ("none", data->notifier_action.c_str());
    ASSERT_FALSE(data->use_notifier);
    ASSERT_EQ(ORCM_RAS_SEVERITY_ERROR, data->severity);

    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_analyze_7)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, NULL));

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(50);
    counter_analyzer->set_window_size("1m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy(true, true, true, "-1", "false", "email");
    caddy->imod = (orcm_analytics_base_module_t*)mod;

    ASSERT_EQ(ORCM_SUCCESS, cott_analyze(0, 0, (void*)caddy));

    // Check that wf arguments get to the class
    ASSERT_STREQ("CPU_SrcID#*_Channel#*_DIMM#*_CE", counter_analyzer->label_mask_.c_str());
    ASSERT_STREQ("2m", counter_analyzer->window_size_.c_str());
    ASSERT_EQ(1, counter_analyzer->threshold_);

    test_step_data* data = (test_step_data*)counter_analyzer->get_user_data();
    ASSERT_EQ(ORCM_EVENT_HARD_FAULT, data->fault_type);
    ASSERT_FALSE(data->store_event);
    ASSERT_STREQ("email", data->notifier_action.c_str());
    ASSERT_TRUE(data->use_notifier);
    ASSERT_EQ(ORCM_RAS_SEVERITY_ERROR, data->severity);

    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_analyze_8)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, NULL));

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(50);
    counter_analyzer->set_window_size("1m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy(true, true, true, "-1", "1", "syslog");
    caddy->imod = (orcm_analytics_base_module_t*)mod;

    ASSERT_EQ(ORCM_SUCCESS, cott_analyze(0, 0, (void*)caddy));

    // Check that wf arguments get to the class
    ASSERT_STREQ("CPU_SrcID#*_Channel#*_DIMM#*_CE", counter_analyzer->label_mask_.c_str());
    ASSERT_STREQ("2m", counter_analyzer->window_size_.c_str());
    ASSERT_EQ(1, counter_analyzer->threshold_);

    test_step_data* data = (test_step_data*)counter_analyzer->get_user_data();
    ASSERT_EQ(ORCM_EVENT_HARD_FAULT, data->fault_type);
    ASSERT_TRUE(data->store_event);
    ASSERT_STREQ("syslog", data->notifier_action.c_str());
    ASSERT_TRUE(data->use_notifier);
    ASSERT_EQ(ORCM_RAS_SEVERITY_ERROR, data->severity);

    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_analyze_9)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, NULL));

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(50);
    counter_analyzer->set_window_size("1m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy(true, true, true, "-1", "dummy", "dummy");
    caddy->imod = (orcm_analytics_base_module_t*)mod;

    ASSERT_EQ(ORCM_SUCCESS, cott_analyze(0, 0, (void*)caddy));

    // Check that wf arguments get to the class
    ASSERT_STREQ("CPU_SrcID#*_Channel#*_DIMM#*_CE", counter_analyzer->label_mask_.c_str());
    ASSERT_STREQ("2m", counter_analyzer->window_size_.c_str());
    ASSERT_EQ(1, counter_analyzer->threshold_);

    test_step_data* data = (test_step_data*)counter_analyzer->get_user_data();
    ASSERT_EQ(ORCM_EVENT_HARD_FAULT, data->fault_type);
    ASSERT_TRUE(data->store_event);
    ASSERT_STREQ("dummy", data->notifier_action.c_str());
    ASSERT_TRUE(data->use_notifier);
    ASSERT_EQ(ORCM_RAS_SEVERITY_ERROR, data->severity);

    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_analyze_10)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, NULL));

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(50);
    counter_analyzer->set_window_size("1m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy(true, true, true, "-1", "yes", "email", "emerg");
    caddy->imod = (orcm_analytics_base_module_t*)mod;

    ASSERT_EQ(ORCM_SUCCESS, cott_analyze(0, 0, (void*)caddy));

    // Check that wf arguments get to the class
    ASSERT_STREQ("CPU_SrcID#*_Channel#*_DIMM#*_CE", counter_analyzer->label_mask_.c_str());
    ASSERT_STREQ("2m", counter_analyzer->window_size_.c_str());
    ASSERT_EQ(1, counter_analyzer->threshold_);

    test_step_data* data = (test_step_data*)counter_analyzer->get_user_data();
    ASSERT_EQ(ORCM_EVENT_HARD_FAULT, data->fault_type);
    ASSERT_TRUE(data->store_event);
    ASSERT_STREQ("email", data->notifier_action.c_str());
    ASSERT_TRUE(data->use_notifier);
    ASSERT_EQ(ORCM_RAS_SEVERITY_EMERG, data->severity);

    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_analyze_11)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, NULL));

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(50);
    counter_analyzer->set_window_size("1m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy(true, true, true, "-1", "yes", "email", "dummy", "soft");
    caddy->imod = (orcm_analytics_base_module_t*)mod;

    ASSERT_EQ(ORCM_SUCCESS, cott_analyze(0, 0, (void*)caddy));

    // Check that wf arguments get to the class
    ASSERT_STREQ("CPU_SrcID#*_Channel#*_DIMM#*_CE", counter_analyzer->label_mask_.c_str());
    ASSERT_STREQ("2m", counter_analyzer->window_size_.c_str());
    ASSERT_EQ(1, counter_analyzer->threshold_);

    test_step_data* data = (test_step_data*)counter_analyzer->get_user_data();
    ASSERT_EQ(ORCM_EVENT_SOFT_FAULT, data->fault_type);
    ASSERT_TRUE(data->store_event);
    ASSERT_STREQ("email", data->notifier_action.c_str());
    ASSERT_TRUE(data->use_notifier);
    ASSERT_EQ(ORCM_RAS_SEVERITY_ERROR, data->severity);

    cott_finalize(mod);
}

TEST_F(analyze_counter_tests, test_analyze_12)
{
    ResetTestCase();

    ASSERT_NE(ORCM_SUCCESS, cott_analyze(0, 0, NULL));

    orcm_analytics_base_module_t* mod = (orcm_analytics_base_module_t*)malloc(sizeof(mca_analytics_cott_module_t));
    ASSERT_EQ(ORCM_SUCCESS, cott_init(mod));
    counter_analyzer->set_threshold(50);
    counter_analyzer->set_window_size("1m");
    counter_analyzer->set_value_label_mask(COMPLEX_MASK);

    orcm_workflow_caddy_t* caddy = MakeCaddy(true, true, true, "-1", "yes", "email", "dummy", "dummy");
    caddy->imod = (orcm_analytics_base_module_t*)mod;

    ASSERT_EQ(ORCM_SUCCESS, cott_analyze(0, 0, (void*)caddy));

    // Check that wf arguments get to the class
    ASSERT_STREQ("CPU_SrcID#*_Channel#*_DIMM#*_CE", counter_analyzer->label_mask_.c_str());
    ASSERT_STREQ("2m", counter_analyzer->window_size_.c_str());
    ASSERT_EQ(1, counter_analyzer->threshold_);

    test_step_data* data = (test_step_data*)counter_analyzer->get_user_data();
    ASSERT_EQ(ORCM_EVENT_HARD_FAULT, data->fault_type);
    ASSERT_TRUE(data->store_event);
    ASSERT_STREQ("email", data->notifier_action.c_str());
    ASSERT_TRUE(data->use_notifier);
    ASSERT_EQ(ORCM_RAS_SEVERITY_ERROR, data->severity);

    cott_finalize(mod);
}
