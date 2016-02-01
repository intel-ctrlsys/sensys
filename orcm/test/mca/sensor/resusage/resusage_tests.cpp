/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "resusage_tests.h"

// ORTE
#include "orte/runtime/orte_globals.h"

// ORCM
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/resusage/sensor_resusage.h"

// Fixture
using namespace std;

extern "C" {
    ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
    extern void collect_resusage_sample(orcm_sensor_sampler_t *sampler);
    extern int resusage_enable_sampling(const char* sensor_specification);
    extern int resusage_disable_sampling(const char* sensor_specification);
    extern int resusage_reset_sampling(const char* sensor_specification);
};

void ut_resusage_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();
}

void ut_resusage_tests::TearDownTestCase()
{
    // Release OPAL level resources
    opal_dss_close();
}


// Testing the data collection class
TEST_F(ut_resusage_tests, resusage_sensor_sample_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("resusage", false, false);
    mca_sensor_resusage_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    // Tests
    collect_resusage_sample(sampler);
    EXPECT_EQ(0, (mca_sensor_resusage_component.diagnostics & 0x1));

    orcm_sensor_base_runtime_metrics_set(object, true, "resusage");
    collect_resusage_sample(sampler);
    EXPECT_EQ(1, (mca_sensor_resusage_component.diagnostics & 0x1));

    // Cleanup
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_resusage_component.runtime_metrics = NULL;
}

TEST_F(ut_resusage_tests, resusage_api_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("resusage", false, false);
    mca_sensor_resusage_component.runtime_metrics = object;

    // Tests
    resusage_enable_sampling("resusage");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    resusage_disable_sampling("resusage");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    resusage_enable_sampling("resusage");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    resusage_reset_sampling("resusage");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    resusage_enable_sampling("not_the_right_datagroup");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    resusage_enable_sampling("all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_resusage_component.runtime_metrics = NULL;
}
