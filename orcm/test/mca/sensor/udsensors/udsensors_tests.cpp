/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "udsensors_tests.h"

// ORTE
#include "orte/runtime/orte_globals.h"

//OPAL
#include "opal/mca/installdirs/installdirs.h"

// ORCM
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"

// C
#include <unistd.h>

// C++
#include <iostream>
#include <stdexcept>

// Fixture
using namespace std;

extern "C" {
    #include "orcm/mca/sensor/udsensors/sensor_udsensors.cpp"
    extern int orcm_sensor_udsensors_open(void);
    extern int orcm_sensor_udsensors_close(void);
    extern int udsensors_component_register(void);
    extern int orcm_sensor_udsensors_query(mca_base_module_t **module, int *priority);

    extern opal_event_base_t* __real_opal_progress_thread_init(const char* name);
    opal_event_base_t* __wrap_opal_progress_thread_init(const char* name)
    {
        if(ut_udsensors_tests::use_pt_) {
            return __real_opal_progress_thread_init(name);
        } else {
            return NULL;
        }
    }
};

bool ut_udsensors_tests::use_pt_ = true;

void ut_udsensors_tests::SetUpTestCase()
{

}

void ut_udsensors_tests::TearDownTestCase()
{

}

TEST_F(ut_udsensors_tests, udsensors_sensor_sample_with_progress_thread)
{
    void* object = orcm_sensor_base_runtime_metrics_create("udsensors", false, false);
    mca_sensor_udsensors_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    mca_sensor_udsensors_component.use_progress_thread = true;
    orcm_sensor_udsensors_module.sample(sampler);
    EXPECT_EQ(0, (mca_sensor_udsensors_component.diagnostics & 0x1));

    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_udsensors_component.runtime_metrics = NULL;
    mca_sensor_udsensors_component.diagnostics = 0;
}

TEST_F(ut_udsensors_tests, udsensors_sensor_sample_no_progress_thread)
{
    void* object = orcm_sensor_base_runtime_metrics_create("udsensors", false, false);
    mca_sensor_udsensors_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    mca_sensor_udsensors_component.use_progress_thread = false;
    orcm_sensor_base_runtime_metrics_set(object, true, "udsensors");
    orcm_sensor_udsensors_module.sample(sampler);
    EXPECT_EQ(1, (mca_sensor_udsensors_component.diagnostics & 0x1));

    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_udsensors_component.runtime_metrics = NULL;
    mca_sensor_udsensors_component.diagnostics = 0;
}

TEST_F(ut_udsensors_tests, udsensors_sensor_perthread)
{
        mca_sensor_udsensors_component.use_progress_thread = true;
        EXPECT_EQ(ORCM_SUCCESS,orcm_sensor_udsensors_module.init());

        orcm_sensor_udsensors_module.set_sample_rate(1);
        orcm_sensor_udsensors_module.start(0);
        sleep(5);
        orcm_sensor_udsensors_module.set_sample_rate(2);
        sleep(5);

        orcm_sensor_udsensors_module.stop(0);
        orcm_sensor_udsensors_module.finalize();
        mca_sensor_udsensors_component.use_progress_thread = false;
}

TEST_F(ut_udsensors_tests, udsensors_api_tests)
{
    void* object = orcm_sensor_base_runtime_metrics_create("udsensors", false, false);
    mca_sensor_udsensors_component.runtime_metrics = object;

    udsensors_enable_sampling("udsensors");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    udsensors_disable_sampling("udsensors");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    udsensors_enable_sampling("udsensors");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    udsensors_reset_sampling("udsensors");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    udsensors_enable_sampling("not_the_right_datagroup");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    udsensors_enable_sampling("all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));

    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_udsensors_component.runtime_metrics = NULL;
}

TEST_F(ut_udsensors_tests, udsensors_init)
{
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_udsensors_module.init());
    EXPECT_NE(0, (uint64_t)(mca_sensor_udsensors_component.runtime_metrics));

    orcm_sensor_udsensors_module.finalize();
}

TEST_F(ut_udsensors_tests, udsensors_start_stop_progress_thread)
{
    int sample_rate = 8;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_udsensors_module.init());
    mca_sensor_udsensors_component.use_progress_thread = true;
    mca_sensor_udsensors_component.sample_rate = sample_rate;

    orcm_sensor_udsensors_module.start(5);
    EXPECT_TRUE(orcm_sensor_udsensors.ev_active);
    EXPECT_EQ(sample_rate,mca_sensor_udsensors_component.sample_rate);
    orcm_sensor_udsensors_module.stop(5);
    EXPECT_FALSE(orcm_sensor_udsensors.ev_active);

    orcm_sensor_udsensors_module.finalize();
    mca_sensor_udsensors_component.sample_rate = 1;
}

TEST_F(ut_udsensors_tests, udsensors_start_stop_progress_thread_negative)
{
    use_pt_ = false;
    mca_sensor_udsensors_component.use_progress_thread = true;

    orcm_sensor_udsensors_module.start(5);
    EXPECT_FALSE(orcm_sensor_udsensors.ev_active);

    use_pt_ = true;
}

TEST_F(ut_udsensors_tests, udsensors_double_start)
{
    mca_sensor_udsensors_component.use_progress_thread = true;
    orcm_sensor_udsensors_module.start(5);
    EXPECT_TRUE(orcm_sensor_udsensors.ev_active);
    orcm_sensor_udsensors_module.start(5);
    EXPECT_TRUE(orcm_sensor_udsensors.ev_active);

    orcm_sensor_udsensors_module.stop(5);
    EXPECT_FALSE(orcm_sensor_udsensors.ev_active);

    orcm_sensor_udsensors_module.finalize();
}

TEST_F(ut_udsensors_tests, udsensors_start_stop)
{
    int sample_rate = 8;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_udsensors_module.init());
    mca_sensor_udsensors_component.use_progress_thread = false;
    mca_sensor_udsensors_component.sample_rate = sample_rate;

    orcm_sensor_udsensors_module.start(5);
    EXPECT_FALSE(orcm_sensor_udsensors.ev_active);
    EXPECT_EQ(orcm_sensor_base.sample_rate, mca_sensor_udsensors_component.sample_rate);
    orcm_sensor_udsensors_module.stop(5);
    EXPECT_FALSE(orcm_sensor_udsensors.ev_active);

    orcm_sensor_udsensors_module.finalize();
    mca_sensor_udsensors_component.sample_rate = 1;
 }

TEST_F(ut_udsensors_tests, udsensors_sample_rate_tests)
{
    int rate = 5;
    orcm_sensor_udsensors_module.set_sample_rate(rate);
    EXPECT_NE(rate, mca_sensor_udsensors_component.sample_rate);

    mca_sensor_udsensors_component.use_progress_thread = true;
    orcm_sensor_udsensors_module.set_sample_rate(rate);
    EXPECT_EQ(rate, mca_sensor_udsensors_component.sample_rate);

    int *get_rate = NULL;
    orcm_sensor_udsensors_module.get_sample_rate(get_rate);
    EXPECT_EQ(NULL, get_rate);

    get_rate = (int *)malloc(sizeof(int));
    orcm_sensor_udsensors_module.get_sample_rate(get_rate);
    EXPECT_EQ(rate, *get_rate);

    mca_sensor_udsensors_component.sample_rate = 1;
}

TEST_F(ut_udsensors_tests, udsensors_inventory_collect)
{
     opal_buffer_t buffer;
     orcm_sensor_udsensors_module.inventory_collect(&buffer);
}

TEST_F(ut_udsensors_tests, udsensors_inventory_log)
{
     opal_buffer_t buffer;
     orcm_sensor_udsensors_module.inventory_log((char*)"testhost1", &buffer);
}

TEST_F(ut_udsensors_tests, udsensors_log)
{
     opal_buffer_t buffer;
     orcm_sensor_udsensors_module.log(&buffer);
}

TEST_F(ut_udsensors_tests, udsensors_component)
{
    int priority;
    mca_base_module_t* module = NULL;

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_udsensors_open());
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_udsensors_close());
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_udsensors_query(&module, &priority));
}

TEST_F(ut_udsensors_tests, udsensors_component_register_defaults)
{
    EXPECT_EQ(ORCM_SUCCESS, udsensors_component_register());
    EXPECT_FALSE(mca_sensor_udsensors_component.use_progress_thread);
    EXPECT_EQ(0, mca_sensor_udsensors_component.sample_rate);
    EXPECT_EQ(orcm_sensor_base.collect_metrics,mca_sensor_udsensors_component.collect_metrics);
    EXPECT_EQ(opal_install_dirs.opallibdir, mca_sensor_udsensors_component.udpath);
}

