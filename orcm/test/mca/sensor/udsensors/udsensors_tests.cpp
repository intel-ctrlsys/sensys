/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "udsensors_tests.h"

// C
#include <unistd.h>

// C++
#include <iostream>
#include <stdexcept>
#include "orcm/mca/sensor/udsensors/sensor_udsensors.cpp"
#include "orcm/common/dataContainer.hpp"
#include "orcm/common/dataContainerHelper.hpp"

// Fixture
using namespace std;

extern "C" {
    #include "opal/mca/installdirs/installdirs.h"
    #include "orte/runtime/orte_globals.h"
    #include "orcm/util/utils.h"
    #include "orcm/runtime/orcm_globals.h"
    #include "orcm/mca/analytics/analytics.h"
    #include "orcm/mca/sensor/base/base.h"
    #include "orcm/mca/sensor/base/sensor_private.h"
    #include "opal/runtime/opal_progress_threads.h"
    #include "opal/runtime/opal.h"

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

#define N_MOCKED_PLUGINS 1

bool ut_udsensors_tests::use_pt_ = true;

void ut_udsensors_tests::SetUpTestCase()
{

}

void ut_udsensors_tests::TearDownTestCase()
{

}


void ut_udsensors_tests::setFullMock(bool mockStatus, int nPlugins)
{
    mock_readdir = mockStatus;
    n_mocked_plugins = nPlugins;
    mock_dlopen = mockStatus;
    mock_dlclose = mockStatus;
    mock_dlsym = mockStatus;
    mock_plugin = mockStatus;
    mock_do_collect = mockStatus;
    mock_opal_pack = mockStatus;
    mock_opal_unpack = mockStatus;
    do_collect_expected_value = mockStatus;
    opal_pack_expected_value = mockStatus;
    opal_unpack_expected_value = mockStatus;
    mock_serializeMap = mockStatus;
    getPluginNameMock = mockStatus;
    initPluginMock = mockStatus;
    dlopenReturnHandler = mockStatus;
}

bool ut_udsensors_tests::isOutputError(std::string output)
{
    std::string error = ("ERROR");
    std::size_t found = output.find(error);
    return (found!=std::string::npos);
}

void ut_udsensors_sample::SetUp()
{
    setFullMock(true, N_MOCKED_PLUGINS);
    throwOnInit = false;
    emptyContainer = false;
    mock_serializeMap = false;
    opal_pack_expected_value = false;

    orcm_sensor_udsensors_module.init();
    object = orcm_sensor_base_runtime_metrics_create("udsensors", false, false);
    mca_sensor_udsensors_component.runtime_metrics = object;

    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*) OBJ_NEW(orcm_sensor_sampler_t);
    samplerPtr = (void*) sampler;
}

void ut_udsensors_sample::TearDown()
{
    orcm_sensor_udsensors_module.finalize();
    setFullMock(false, 0);
    throwOnInit = false;
    throwOnSample = false;
    do_collect_expected_value = true;

    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*) samplerPtr;

    mca_sensor_udsensors_component.diagnostics = 0;

    OBJ_RELEASE(sampler);
}

TEST_F(ut_udsensors_sample, udsensors_sensor_sample_with_progress_thread)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*) samplerPtr;
    mca_sensor_udsensors_component.use_progress_thread = true;
    orcm_sensor_udsensors_module.sample(sampler);
    EXPECT_EQ(0, (mca_sensor_udsensors_component.diagnostics & 0x1));

}

TEST_F(ut_udsensors_sample, udsensors_sensor_sample_no_progress_thread)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*) samplerPtr;
    mca_sensor_udsensors_component.use_progress_thread = false;
    orcm_sensor_base_runtime_metrics_set(object, true, "udsensors");
    orcm_sensor_udsensors_module.sample(sampler);
    EXPECT_EQ(1, (mca_sensor_udsensors_component.diagnostics & 0x1));
}


TEST_F(ut_udsensors_sample, udsensors_sensor_sample_no_collect_metrics)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*) samplerPtr;
    mca_sensor_udsensors_component.use_progress_thread = false;
    do_collect_expected_value = false;
    orcm_sensor_udsensors_module.sample(sampler);
    EXPECT_EQ(0, (mca_sensor_udsensors_component.diagnostics & 0x1));
}

TEST_F(ut_udsensors_sample, udsensors_sensor_sample_exception_in_factory)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*) samplerPtr;
    mca_sensor_udsensors_component.use_progress_thread = false;
    throwOnSample = true;
    emptyContainer = false;

    testing::internal::CaptureStderr();
    orcm_sensor_udsensors_module.sample(sampler);
    EXPECT_TRUE(isOutputError(testing::internal::GetCapturedStderr()));
}

TEST_F(ut_udsensors_sample, udsensors_sensor_sample_exception_in_serializeMap)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*) samplerPtr;
    mca_sensor_udsensors_component.use_progress_thread = false;
    mock_serializeMap = true;
    emptyContainer = false;

    testing::internal::CaptureStderr();
    orcm_sensor_udsensors_module.sample(sampler);
    EXPECT_TRUE(isOutputError(testing::internal::GetCapturedStderr()));
}


TEST_F(ut_udsensors_sample, udsensors_sensor_perthread)
{
    mca_sensor_udsensors_component.use_progress_thread = true;

    orcm_sensor_udsensors_module.set_sample_rate(1);
    orcm_sensor_udsensors_module.start(0);
    sleep(5);
    orcm_sensor_udsensors_module.set_sample_rate(2);
    sleep(5);

    orcm_sensor_udsensors_module.stop(0);
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

TEST_F(ut_udsensors_tests, udsensors_init_without_plugins)
{
    EXPECT_NE(ORCM_SUCCESS, orcm_sensor_udsensors_module.init());
    EXPECT_NE(0, (uint64_t)(mca_sensor_udsensors_component.runtime_metrics));
}

TEST_F(ut_udsensors_tests, udsensors_init_wrong_udsensorpath)
{
    mca_sensor_udsensors_component.udpath = strdup ("/dummy/path");
    EXPECT_NE(ORCM_SUCCESS, orcm_sensor_udsensors_module.init());

    free( mca_sensor_udsensors_component.udpath );
    mca_sensor_udsensors_component.udpath = opal_install_dirs.opallibdir;
    orcm_sensor_udsensors_module.finalize();
}


TEST_F(ut_udsensors_sample, udsensors_start_stop_progress_thread)
{
    int sample_rate = 8;
    mca_sensor_udsensors_component.use_progress_thread = true;
    mca_sensor_udsensors_component.sample_rate = sample_rate;

    orcm_sensor_udsensors_module.start(5);
    EXPECT_TRUE(orcm_sensor_udsensors.ev_active);
    EXPECT_EQ(sample_rate,mca_sensor_udsensors_component.sample_rate);
    orcm_sensor_udsensors_module.stop(5);
    EXPECT_FALSE(orcm_sensor_udsensors.ev_active);

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
}

TEST_F(ut_udsensors_sample, udsensors_start_stop)
{
    int sample_rate = 8;
    mca_sensor_udsensors_component.use_progress_thread = false;
    mca_sensor_udsensors_component.sample_rate = sample_rate;

    orcm_sensor_udsensors_module.start(5);
    EXPECT_FALSE(orcm_sensor_udsensors.ev_active);
    EXPECT_EQ(orcm_sensor_base.sample_rate, mca_sensor_udsensors_component.sample_rate);
    orcm_sensor_udsensors_module.stop(5);
    EXPECT_FALSE(orcm_sensor_udsensors.ev_active);

    mca_sensor_udsensors_component.sample_rate = 1;
 }

TEST_F(ut_udsensors_tests, udsensors_sample_rate_tests)
{
    int rate = 5;
    mca_sensor_udsensors_component.use_progress_thread = false;
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

void ut_udsensors_init::SetUp()
{
    setFullMock(false, 0);
    throwOnInit = false;
    throwOnSample = false;
    emptyContainer = false;
    obj = sensorFactory::getInstance();
}

void ut_udsensors_init::TearDown()
{
    obj->pluginFilesFound.clear();
    obj->close();
}


TEST_F(ut_udsensors_init, fail_in_factory_init)
{
    setFullMock(true, N_MOCKED_PLUGINS);
    throwOnInit = true;
    EXPECT_EQ(ORCM_ERROR, orcm_sensor_udsensors_module.init());
}

TEST_F(ut_udsensors_init, loaded_plugins)
{
    setFullMock(true, N_MOCKED_PLUGINS);
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_udsensors_module.init());
}

void ut_udsensors_log::SetUp()
{
    setFullMock(false, 0);
    opal_init_test();
    struct timeval current_time;
    const char *name = "udsensors_test";
    const char *nodename = "localhost";

    opal_buffer_t* buffer = (opal_buffer_t*) OBJ_NEW(opal_buffer_t);

    opal_dss.pack(buffer, &nodename, 1, OPAL_STRING);
    gettimeofday(&current_time, NULL);
    opal_dss.pack(buffer, &current_time, 1, OPAL_TIMEVAL);

    bufferPtr = (void*) buffer;
}

void ut_udsensors_log::TearDown()
{
    opal_buffer_t* buffer = (opal_buffer_t*) bufferPtr;
    ORCM_RELEASE(buffer);
}

TEST_F(ut_udsensors_log, log_with_invalid_buffer)
{
    setFullMock(true, 0);
    opal_unpack_expected_value = false;
    testing::internal::CaptureStderr();
    orcm_sensor_udsensors_module.log(NULL);
    EXPECT_TRUE(isOutputError(testing::internal::GetCapturedStderr()));
}

TEST_F(ut_udsensors_log, log_with_valid_buffer)
{
    orcm_sensor_udsensors_module.init();
    opal_buffer_t* buffer = (opal_buffer_t*) bufferPtr;
    dataContainer cnt;
    cnt.put("intValue", (int) 3, "ints");
    cnt.put("floatValue", (float) 3.14, "floats");
    cnt.put("doubleValue", (double) 3.14159265, "doubles");

    dataContainerMap cntMap;
    cntMap["cnt1"] = cnt;
    dataContainerHelper::serializeMap(cntMap, buffer);
    testing::internal::CaptureStderr();
    orcm_sensor_udsensors_module.log(buffer);
    EXPECT_FALSE(isOutputError(testing::internal::GetCapturedStderr()));
}

TEST_F(ut_udsensors_log, log_with_empty_buffer)
{
    orcm_sensor_udsensors_module.init();
    opal_buffer_t* buffer = (opal_buffer_t*) bufferPtr;
    dataContainer cnt;
    dataContainerMap cntMap;
    cntMap["cnt1"] = cnt;
    dataContainerHelper::serializeMap(cntMap, buffer);
    testing::internal::CaptureStderr();
    orcm_sensor_udsensors_module.log(buffer);
    EXPECT_FALSE(isOutputError(testing::internal::GetCapturedStderr()));
}

TEST_F(ut_udsensors_tests, udsensors_fill_compute_data_invalid_buffer)
{
    dataContainer cnt;
    testing::internal::CaptureStderr();
    udsensors_fill_compute_data(NULL, cnt);
    EXPECT_TRUE(isOutputError(testing::internal::GetCapturedStderr()));
}
