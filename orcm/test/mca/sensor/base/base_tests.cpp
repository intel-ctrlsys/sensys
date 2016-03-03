/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "base_tests.h"

// C++
#include <iostream>

extern "C" {
    // C
    #include "memory.h"

    // ORCM
    #include "orcm/runtime/orcm_globals.h"
    #include "orcm/include/orcm/constants.h"

    extern int manage_sensor_sampling(int command, const char* sensor_spec);
};
#define GTEST_MOCK_TESTING
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#undef GTEST_MOCK_TESTING

// Fixture
using namespace std;

bool ut_base_tests::sampling_result = false;
orcm_sensor_base_module_t ut_base_tests::test_module;
orcm_sensor_active_module_t* ut_base_tests::active_module = NULL;

void ut_base_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();

    memset(&test_module, 0, sizeof(orcm_sensor_base_module_t));
    sampling_result = false;

    OBJ_CONSTRUCT(&orcm_sensor_base.modules, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_sensor_base.modules, 0, 1, sizeof(void*));
    active_module = (orcm_sensor_active_module_t*)OBJ_NEW(orcm_sensor_active_module_t);
    active_module->module = &test_module;
    opal_pointer_array_add(&orcm_sensor_base.modules, active_module);
    active_module->component = (orcm_sensor_base_component_t*)malloc(sizeof(orcm_sensor_base_component_t));
}

void ut_base_tests::TearDownTestCase()
{
    test_module.enable_sampling = NULL;
    test_module.disable_sampling = NULL;
    test_module.reset_sampling = NULL;

    free(active_module->component);
    OBJ_RELEASE(active_module);
    OBJ_DESTRUCT(&orcm_sensor_base.modules);
}

int ut_base_tests::sampling_callback(const char* spec)
{
    sampling_result = true;
    return ORCM_SUCCESS;
}

// Testing the data collection class
TEST_F(ut_base_tests, base_sensor_runtime_metrics)
{
    void* obj = orcm_sensor_base_runtime_metrics_create("base", true, true);
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj, NULL));
    orcm_sensor_base_runtime_metrics_destroy(obj);

    obj = orcm_sensor_base_runtime_metrics_create("base", true, false);
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(obj, NULL));
    orcm_sensor_base_runtime_metrics_destroy(obj);

    obj = orcm_sensor_base_runtime_metrics_create("base", false, false);
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(obj, NULL));
    orcm_sensor_base_runtime_metrics_destroy(obj);

    obj = orcm_sensor_base_runtime_metrics_create("base", false, true);
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj, NULL));
    orcm_sensor_base_runtime_metrics_destroy(obj);

    obj = orcm_sensor_base_runtime_metrics_create("base", true, true);
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_base_runtime_metrics_set(obj, false, "base"));
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(obj, NULL));
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_base_runtime_metrics_reset(obj, "base"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj, NULL));
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_base_runtime_metrics_set(obj, false, "not_base"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj, NULL));
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_base_runtime_metrics_set(obj, false, "base"));
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(obj, NULL));
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_base_runtime_metrics_reset(obj, "not_base"));
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(obj, NULL));
    orcm_sensor_base_runtime_metrics_destroy(obj);
}

TEST_F(ut_base_tests, base_manage_sensor_sampling_tests)
{
    test_module.enable_sampling = NULL;
    test_module.disable_sampling = NULL;
    test_module.reset_sampling = NULL;
    sampling_result = false;
    manage_sensor_sampling(ORCM_ENABLE_SENSOR_SAMPLING_COMMAND, "all");
    EXPECT_FALSE(sampling_result);

    sampling_result = false;
    manage_sensor_sampling(ORCM_DISABLE_SENSOR_SAMPLING_COMMAND, "all");
    EXPECT_FALSE(sampling_result);

    sampling_result = false;
    manage_sensor_sampling(ORCM_RESET_SENSOR_SAMPLING_COMMAND, "all");
    EXPECT_FALSE(sampling_result);

    test_module.enable_sampling = sampling_callback;
    test_module.disable_sampling = sampling_callback;
    test_module.reset_sampling = sampling_callback;

    sampling_result = false;
    manage_sensor_sampling(ORCM_ENABLE_SENSOR_SAMPLING_COMMAND, "all");
    EXPECT_TRUE(sampling_result);

    sampling_result = false;
    manage_sensor_sampling(ORCM_DISABLE_SENSOR_SAMPLING_COMMAND, "all");
    EXPECT_TRUE(sampling_result);

    sampling_result = false;
    manage_sensor_sampling(ORCM_RESET_SENSOR_SAMPLING_COMMAND, "all");
    EXPECT_TRUE(sampling_result);

    test_module.enable_sampling = NULL;
    test_module.disable_sampling = NULL;
    test_module.reset_sampling = NULL;
}

TEST_F(ut_base_tests, base_manage_sensor_tracking_tests)
{
    void* obj = orcm_sensor_base_runtime_metrics_create("base", true, true);
    RuntimeMetrics* rt_metrics = (RuntimeMetrics*)obj;

    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj, NULL));
    orcm_sensor_base_runtime_metrics_track(obj, "label_1");
    orcm_sensor_base_runtime_metrics_track(obj, "label_2");
    EXPECT_EQ(2, rt_metrics->sensorLabelMap_.size()) << "map size was not 2";
    EXPECT_FALSE(rt_metrics->sensorLabelMap_.end() == rt_metrics->sensorLabelMap_.find("label_1"));
    EXPECT_FALSE(rt_metrics->sensorLabelMap_.end() == rt_metrics->sensorLabelMap_.find("label_2"));
    EXPECT_TRUE(rt_metrics->sensorLabelMap_.end() == rt_metrics->sensorLabelMap_.find("label_3"));
    orcm_sensor_base_runtime_metrics_track(obj, "label_1");
    EXPECT_EQ(2, rt_metrics->sensorLabelMap_.size()) << "map size was not 2; after inserting same element again.";
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj, "label_1"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj, "label_2"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj, "label_3")) << "unknown labels default to datagroup and this failed.";

    orcm_sensor_base_runtime_metrics_destroy(obj);
}


TEST_F(ut_base_tests, base_manage_sensor_set_labels)
{
    void* obj = orcm_sensor_base_runtime_metrics_create("base", true, true);
    RuntimeMetrics* rt_metrics = (RuntimeMetrics*)obj;
    orcm_sensor_base_runtime_metrics_track(obj, "label_1");
    orcm_sensor_base_runtime_metrics_track(obj, "label_2");

    EXPECT_EQ(2, rt_metrics->sensorLabelMap_.size());
    EXPECT_EQ(2, orcm_sensor_base_runtime_metrics_active_label_count(obj));
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_base_runtime_metrics_set(obj, false, "base:label_2"));
    EXPECT_EQ(1, orcm_sensor_base_runtime_metrics_active_label_count(obj));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj, "label_1"));
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(obj, "label_2"));

    orcm_sensor_base_runtime_metrics_begin(obj);
    orcm_sensor_base_runtime_metrics_begin(obj);
    int result = orcm_sensor_base_runtime_metrics_set(obj, false, "base:label_2");
    EXPECT_EQ(ORCM_ERR_TIMEOUT, result) << "ERROR: " << result;
    orcm_sensor_base_runtime_metrics_end(obj);
    orcm_sensor_base_runtime_metrics_end(obj);
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(obj, "label_2"));
    EXPECT_EQ(ORCM_ERR_NOT_FOUND, orcm_sensor_base_runtime_metrics_set(obj, false, "base:label_3"));

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_base_runtime_metrics_set(obj, false, "all"));
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(obj, "label_1"));

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_base_runtime_metrics_reset(obj, "base:all"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj, "label_2"));
    EXPECT_EQ(2, orcm_sensor_base_runtime_metrics_active_label_count(obj));

    orcm_sensor_base_runtime_metrics_destroy(obj);
}
