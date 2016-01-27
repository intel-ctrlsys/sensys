/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "base_tests.h"

// ORTE
#include "orte/runtime/orte_globals.h"

// ORCM
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"

// Fixture
using namespace std;

void ut_base_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
}

void ut_base_tests::TearDownTestCase()
{
}

// Testing the data collection class
TEST_F(ut_base_tests, base_sensor_runtime_metrics)
{
    void* obj = orcm_sensor_base_runtime_metrics_create(true, true);
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj));
    orcm_sensor_base_runtime_metrics_destroy(obj);

    obj = orcm_sensor_base_runtime_metrics_create(true, false);
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(obj));
    orcm_sensor_base_runtime_metrics_destroy(obj);

    obj = orcm_sensor_base_runtime_metrics_create(false, false);
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(obj));
    orcm_sensor_base_runtime_metrics_destroy(obj);

    obj = orcm_sensor_base_runtime_metrics_create(false, true);
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj));
    orcm_sensor_base_runtime_metrics_destroy(obj);

    obj = orcm_sensor_base_runtime_metrics_create(true, true);
    orcm_sensor_base_runtime_metrics_set(obj, false);
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(obj));
    orcm_sensor_base_runtime_metrics_reset(obj);
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(obj));
    orcm_sensor_base_runtime_metrics_destroy(obj);
}
