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
    void* object = orcm_sensor_base_runtime_metrics_create(false, false);
    mca_sensor_freq_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    // Tests
    collect_freq_sample(sampler);
    EXPECT_EQ(0, (mca_sensor_freq_component.diagnostics & 0x1));

    orcm_sensor_base_runtime_metrics_set(object, true);
    collect_freq_sample(sampler);
    EXPECT_EQ(1, (mca_sensor_freq_component.diagnostics & 0x1));

    // Cleanup
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_freq_component.runtime_metrics = NULL;
}
