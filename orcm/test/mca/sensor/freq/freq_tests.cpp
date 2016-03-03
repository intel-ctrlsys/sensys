/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "freq_tests.h"

// ORTE
#include "orte/runtime/orte_globals.h"

// ORCM
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/freq/sensor_freq.h"

// Fixture
using namespace std;

extern "C" {
    ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
    extern void collect_freq_sample(orcm_sensor_sampler_t *sampler);
    extern int freq_enable_sampling(const char* sensor_specification);
    extern int freq_disable_sampling(const char* sensor_specification);
    extern int freq_reset_sampling(const char* sensor_specification);
};

void ut_freq_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();
}

void ut_freq_tests::TearDownTestCase()
{
    // Release OPAL level resources
    opal_dss_close();
}


// Testing the data collection class
TEST_F(ut_freq_tests, freq_sensor_sample_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("freq", false, false);
    mca_sensor_freq_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    // Tests
    collect_freq_sample(sampler);
    EXPECT_EQ(0, (mca_sensor_freq_component.diagnostics & 0x1));

    orcm_sensor_base_runtime_metrics_set(object, true, "freq");
    collect_freq_sample(sampler);
    EXPECT_EQ(1, (mca_sensor_freq_component.diagnostics & 0x1));

    // Cleanup
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_freq_component.runtime_metrics = NULL;
}

TEST_F(ut_freq_tests, freq_api_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("freq", false, false);
    mca_sensor_freq_component.runtime_metrics = object;

    // Tests
    freq_enable_sampling("freq");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    freq_disable_sampling("freq");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    freq_enable_sampling("freq");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    freq_reset_sampling("freq");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    freq_enable_sampling("not_the_right_datagroup");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    freq_enable_sampling("all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_freq_component.runtime_metrics = NULL;
}

TEST_F(ut_freq_tests, freq_api_tests_2)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("freq", false, false);
    mca_sensor_freq_component.runtime_metrics = object;
    orcm_sensor_base_runtime_metrics_track(object, "core0");
    orcm_sensor_base_runtime_metrics_track(object, "core1");
    orcm_sensor_base_runtime_metrics_track(object, "core2");
    orcm_sensor_base_runtime_metrics_track(object, "core3");

    // Tests
    freq_enable_sampling("freq:core3");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "core1"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "core3"));
    EXPECT_EQ(1,orcm_sensor_base_runtime_metrics_active_label_count(object));
    freq_disable_sampling("freq:core3");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "core3"));
    freq_enable_sampling("freq:core2");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "core2"));
    freq_reset_sampling("freq:core2");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "core2"));
    freq_enable_sampling("freq:core no_core");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "core2"));
    freq_enable_sampling("freq:all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "core0"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "core3"));
    EXPECT_EQ(4,orcm_sensor_base_runtime_metrics_active_label_count(object));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_freq_component.runtime_metrics = NULL;
}
