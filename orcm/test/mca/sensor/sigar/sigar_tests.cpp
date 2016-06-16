/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sigar_tests.h"
#include "sigar_mocked_functions.h"

// ORTE
#include "orte/runtime/orte_globals.h"

// ORCM
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/sigar/sensor_sigar.h"

// Fixture
using namespace std;

extern "C" {
    ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
    extern orcm_sensor_base_module_t orcm_sensor_sigar_module;
    extern void collect_sigar_sample(orcm_sensor_sampler_t *sampler);
    extern int sigar_enable_sampling(const char* sensor_specification);
    extern int sigar_disable_sampling(const char* sensor_specification);
    extern int sigar_reset_sampling(const char* sensor_specification);
};

void ut_sigar_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();
}

void ut_sigar_tests::TearDownTestCase()
{
    // Release OPAL level resources
    opal_dss_close();
}

void ut_sigar_tests::SetUp()
{
    collect_sample_flag = false;
    uvalue_sample_flag = false;
}

void ut_sigar_tests::TearDown()
{
}

// Testing the data collection class
TEST_F(ut_sigar_tests, sigar_sensor_sample_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    // Tests
    collect_sigar_sample(sampler);
    EXPECT_EQ(0, (mca_sensor_sigar_component.diagnostics & 0x1));

    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");
    collect_sigar_sample(sampler);
    EXPECT_EQ(1, (mca_sensor_sigar_component.diagnostics & 0x1));

    // Cleanup
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_collect_mem_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.mem = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_mem_size; i++){
        collect_mem_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(collect_sample_flag);
        collect_mem_success_flag[i] = false;
        collect_sample_flag = false;
    }

    mca_sensor_sigar_component.mem = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_uvalue_mem_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.mem = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_mem_size; i++){
        uvalue_mem_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(uvalue_sample_flag);
        uvalue_mem_success_flag[i] = false;
        uvalue_sample_flag = false;
    }

    mca_sensor_sigar_component.mem = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_collect_swap_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.swap = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_swap_size; i++){
        collect_swap_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(collect_sample_flag);
        collect_swap_success_flag[i] = false;
        collect_sample_flag = false;
    }

    mca_sensor_sigar_component.swap = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_uvalue_swap_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.swap = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_swap_size; i++){
        uvalue_swap_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(uvalue_sample_flag);
        uvalue_swap_success_flag[i] = false;
        uvalue_sample_flag = false;
    }

    mca_sensor_sigar_component.swap = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_collect_cpu_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.cpu = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_cpu_size; i++){
        collect_cpu_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(collect_sample_flag);
        collect_cpu_success_flag[i] = false;
        collect_sample_flag = false;
    }

    mca_sensor_sigar_component.cpu = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_uvalue_cpu_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.cpu = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_cpu_size; i++){
        uvalue_cpu_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(uvalue_sample_flag);
        uvalue_cpu_success_flag[i] = false;
        uvalue_sample_flag = false;
    }

    mca_sensor_sigar_component.cpu = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_collect_load_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.load = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_load_size; i++){
        collect_load_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(collect_sample_flag);
        collect_load_success_flag[i] = false;
        collect_sample_flag = false;
    }

    mca_sensor_sigar_component.load = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_uvalue_load_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.load = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_load_size; i++){
        uvalue_load_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(uvalue_sample_flag);
        uvalue_load_success_flag[i] = false;
        uvalue_sample_flag = false;
    }

    mca_sensor_sigar_component.load = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_collect_disk_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.disk = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_sigar_module.init());
    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_sigar_component.runtime_metrics);
    mca_sensor_sigar_component.runtime_metrics = object;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_disk_size; i++){
        collect_disk_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(collect_sample_flag);
        collect_disk_success_flag[i] = false;
        collect_sample_flag = false;
    }

    mca_sensor_sigar_component.disk = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_sigar_module.finalize();
}

TEST_F(ut_sigar_tests, sigar_uvalue_disk_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.disk = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_sigar_module.init());
    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_sigar_component.runtime_metrics);
    mca_sensor_sigar_component.runtime_metrics = object;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_disk_size; i++){
        uvalue_disk_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(uvalue_sample_flag);
        uvalue_disk_success_flag[i] = false;
        uvalue_sample_flag = false;
    }

    mca_sensor_sigar_component.disk = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_sigar_module.finalize();
}

TEST_F(ut_sigar_tests, sigar_collect_network_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.network = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_sigar_module.init());
    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_sigar_component.runtime_metrics);
    mca_sensor_sigar_component.runtime_metrics = object;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_network_size; i++){
        collect_network_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(collect_sample_flag);
        collect_network_success_flag[i] = false;
        collect_sample_flag = false;
    }

    mca_sensor_sigar_component.network = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_sigar_module.finalize();
}

TEST_F(ut_sigar_tests, sigar_uvalue_network_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.network = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_sigar_module.init());
    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_sigar_component.runtime_metrics);
    mca_sensor_sigar_component.runtime_metrics = object;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_network_size; i++){
        uvalue_network_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(uvalue_sample_flag);
        uvalue_network_success_flag[i] = false;
        uvalue_sample_flag = false;
    }

    mca_sensor_sigar_component.network = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_sigar_module.finalize();
}

TEST_F(ut_sigar_tests, sigar_collect_system_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.sys = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    collect_sys_success_flag = true;
    collect_sigar_sample(sampler);
    ASSERT_TRUE(collect_sample_flag);
    collect_sys_success_flag = false;

    mca_sensor_sigar_component.sys = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_uvalue_system_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.sys = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    uvalue_sys_success_flag = true;
    collect_sigar_sample(sampler);
    ASSERT_TRUE(uvalue_sample_flag);
    uvalue_sys_success_flag = false;

    mca_sensor_sigar_component.sys = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_collect_global_procstat_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.proc = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_gproc_size; i++){
        collect_gproc_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(collect_sample_flag);
        collect_gproc_success_flag[i] = false;
        collect_sample_flag = false;
    }

    mca_sensor_sigar_component.proc = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_uvalue_global_procstat_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.proc = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_gproc_size; i++){
        uvalue_gproc_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(uvalue_sample_flag);
        uvalue_gproc_success_flag[i] = false;
        uvalue_sample_flag = false;
    }

    mca_sensor_sigar_component.proc = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_collect_procstat_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.proc = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_proc_size; i++){
        collect_proc_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(collect_sample_flag);
        collect_proc_success_flag[i] = false;
        collect_sample_flag = false;
    }

    mca_sensor_sigar_component.proc = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_uvalue_procstat_tests)
{
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    mca_sensor_sigar_component.test = true;
    mca_sensor_sigar_component.proc = true;
    orcm_sensor_base_runtime_metrics_set(object, true, "sigar");

    for(int i = 0; i < sigar_proc_size; i++){
        uvalue_proc_success_flag[i] = true;
        collect_sigar_sample(sampler);
        ASSERT_TRUE(uvalue_sample_flag);
        uvalue_proc_success_flag[i] = false;
        uvalue_sample_flag = false;
    }

    mca_sensor_sigar_component.proc = false;
    mca_sensor_sigar_component.test = false;
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}

TEST_F(ut_sigar_tests, sigar_api_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;

    // Tests
    sigar_enable_sampling("sigar");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    sigar_disable_sampling("sigar");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    sigar_enable_sampling("sigar");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    sigar_reset_sampling("sigar");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    sigar_enable_sampling("not_the_right_datagroup");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    sigar_enable_sampling("all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}


TEST_F(ut_sigar_tests, sigar_api_tests_2)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("sigar", false, false);
    mca_sensor_sigar_component.runtime_metrics = object;
    orcm_sensor_base_runtime_metrics_track(object, "memory");
    orcm_sensor_base_runtime_metrics_track(object, "cpu_load");
    orcm_sensor_base_runtime_metrics_track(object, "io_ops");
    orcm_sensor_base_runtime_metrics_track(object, "procstat");

    // Tests
    sigar_enable_sampling("sigar:procstat");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "cpu_load"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "procstat"));
    EXPECT_EQ(1,orcm_sensor_base_runtime_metrics_active_label_count(object));
    sigar_disable_sampling("sigar:procstat");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "procstat"));
    sigar_enable_sampling("sigar:io_ops");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "io_ops"));
    sigar_reset_sampling("sigar:io_ops");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "io_ops"));
    sigar_enable_sampling("sigar:core no_core");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "io_ops"));
    sigar_enable_sampling("sigar:all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "memory"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "procstat"));
    EXPECT_EQ(4,orcm_sensor_base_runtime_metrics_active_label_count(object));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_sigar_component.runtime_metrics = NULL;
}
