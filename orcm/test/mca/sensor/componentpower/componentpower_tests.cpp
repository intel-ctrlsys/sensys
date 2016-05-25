/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "componentpower_tests.h"
#include "componentpower_tests_mocking.h"

// ORTE
#include "orte/runtime/orte_globals.h"

// ORCM
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/componentpower/sensor_componentpower.h"

// Fixture
using namespace std;

extern "C" {
    ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
    extern void collect_componentpower_sample(orcm_sensor_sampler_t *sampler);
    extern int componentpower_enable_sampling(const char* sensor_specification);
    extern int componentpower_disable_sampling(const char* sensor_specification);
    extern int componentpower_reset_sampling(const char* sensor_specification);
};

int ut_componentpower_tests::last_orte_error_ = ORCM_SUCCESS;

// Mocking Functions

void ut_componentpower_tests::OrteErrmgrBaseLog(int err, char* file, int lineno)
{
    (void)file;
    (void)lineno;
    last_orte_error_ = err;
}

FILE* ut_componentpower_tests::FOpen(const char * filename, const char * mode)
{
    return NULL;
}

int ut_componentpower_tests::OpenReturnError(char *filename, int access, int permission)
{
    return -1;
}

int ut_componentpower_tests::Open(char *filename, int access, int permission)
{
    return ORCM_SUCCESS;
}

int  ut_componentpower_tests::Read(int handle, void *buffer, int nbyte)
{
    if(NULL != buffer){
        *(unsigned long long*)buffer = -1;
    }
    return nbyte;
}

int  ut_componentpower_tests::ReadReturnError(int handle, void *buffer, int nbyte)
{
    if(NULL != buffer){
        *(unsigned long long*)buffer = -1;
    }
    return -1;
}

opal_event_base_t* ut_componentpower_tests::OpalProgressThreadInitReturnNULL(const char *name)
{
    return NULL;
}

void ut_componentpower_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();

}

void ut_componentpower_tests::SetUp()
{
    resetTestFunctions();
    componentpower_mocking.orte_errmgr_base_log_callback = OrteErrmgrBaseLog;
    last_orte_error_=ORCM_SUCCESS;
}

void ut_componentpower_tests::TearDownTestCase()
{
    // Release OPAL level resources
    opal_dss_close();
    resetTestFunctions();
}

void ut_componentpower_tests::resetTestFunctions()
{
    //Mocking
    componentpower_mocking.orte_errmgr_base_log_callback = NULL;
    componentpower_mocking.open_callback = NULL;
    componentpower_mocking.read_callback = NULL;
    componentpower_mocking.read_return_error_callback = NULL;
    componentpower_mocking.opal_progress_thread_init_callback = NULL;
    componentpower_mocking.fopen_callback = NULL;
    componentpower_mocking.ntimes = 0;
    componentpower_mocking.keep_mocking = false;
}

void ut_componentpower_tests::setInitTestFunctions()
{
    componentpower_mocking.fopen_callback = FOpen;
    componentpower_mocking.open_callback = Open;
    componentpower_mocking.read_callback = Read;
}

// Testing the data collection class
TEST_F(ut_componentpower_tests, componentpower_sensor_sample_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("componentpower", false, false);
    mca_sensor_componentpower_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    // Tests
    collect_componentpower_sample(sampler);
    EXPECT_EQ(0, (mca_sensor_componentpower_component.diagnostics & 0x1));

    orcm_sensor_base_runtime_metrics_set(object, true, "componentpower");
    collect_componentpower_sample(sampler);
    EXPECT_EQ(1, (mca_sensor_componentpower_component.diagnostics & 0x1));

    // Cleanup
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_componentpower_component.runtime_metrics = NULL;
}

TEST_F(ut_componentpower_tests, componentpower_api_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("componentpower", false, false);
    mca_sensor_componentpower_component.runtime_metrics = object;

    // Tests
    componentpower_enable_sampling("componentpower");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    componentpower_disable_sampling("componentpower");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    componentpower_enable_sampling("componentpower");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    componentpower_reset_sampling("componentpower");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    componentpower_enable_sampling("not_the_right_datagroup");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    componentpower_enable_sampling("all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_componentpower_component.runtime_metrics = NULL;
}

TEST_F(ut_componentpower_tests, componentpower_api_tests_2)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("componentpower", false, false);
    mca_sensor_componentpower_component.runtime_metrics = object;
    orcm_sensor_base_runtime_metrics_track(object, "CPU 0");
    orcm_sensor_base_runtime_metrics_track(object, "DIMM 0");
    orcm_sensor_base_runtime_metrics_track(object, "CPU 1");
    orcm_sensor_base_runtime_metrics_track(object, "DIMM 1");

    // Tests
    componentpower_enable_sampling("componentpower:DIMM 1");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "CPU 0"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "DIMM 1"));
    EXPECT_EQ(1,orcm_sensor_base_runtime_metrics_active_label_count(object));
    componentpower_disable_sampling("componentpower:DIMM 1");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "DIMM 1"));
    componentpower_enable_sampling("componentpower:CPU 0");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "CPU 0"));
    componentpower_reset_sampling("componentpower:CPU 0");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "core 2"));
    componentpower_enable_sampling("componentpower:DIMM 99");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "DIMM 99"));
    componentpower_enable_sampling("componentpower:all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "CPU 1"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "DIMM 0"));
    EXPECT_EQ(4,orcm_sensor_base_runtime_metrics_active_label_count(object));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_componentpower_component.runtime_metrics = NULL;
}

TEST_F(ut_componentpower_tests, componentpower_set_sample_rate)
{
    int new_sample_rate = 5;
    int current_sample_rate = mca_sensor_componentpower_component.sample_rate;
    bool pt_old_value = mca_sensor_componentpower_component.use_progress_thread;
    mca_sensor_componentpower_component.use_progress_thread = true;

    orcm_sensor_componentpower_module.set_sample_rate(new_sample_rate);
    EXPECT_EQ(new_sample_rate, mca_sensor_componentpower_component.sample_rate);

    mca_sensor_componentpower_component.sample_rate = current_sample_rate;
    mca_sensor_componentpower_component.use_progress_thread = pt_old_value;
}

TEST_F(ut_componentpower_tests, componentpower_set_sample_rate_negative)
{
    int new_sample_rate = 5;
    int current_sample_rate = mca_sensor_componentpower_component.sample_rate;
    bool pt_old_value = mca_sensor_componentpower_component.use_progress_thread;
    mca_sensor_componentpower_component.use_progress_thread = false;

    orcm_sensor_componentpower_module.set_sample_rate(new_sample_rate);
    EXPECT_EQ(current_sample_rate, mca_sensor_componentpower_component.sample_rate);

    mca_sensor_componentpower_component.use_progress_thread = pt_old_value;
}

TEST_F(ut_componentpower_tests, componentpower_get_sample_rate_negative)
{
    int *ptr = NULL;
    orcm_sensor_componentpower_module.get_sample_rate(ptr);
    EXPECT_EQ(NULL, ptr);
}

TEST_F(ut_componentpower_tests, componentpower_get_sample_rate)
{
    int sample_rate = -1;
    orcm_sensor_componentpower_module.get_sample_rate(&sample_rate);
    EXPECT_NE(-1, sample_rate);
}

TEST_F(ut_componentpower_tests, componentpower_inventory_nodbhandle)
{
    int prev_value = orcm_sensor_base.dbhandle;
    orcm_sensor_base.dbhandle = -1;
    opal_buffer_t *buffer = OBJ_NEW(opal_buffer_t);
    char *value = NULL;
    int num = 1;

    orcm_sensor_componentpower_module.inventory_collect(buffer);
    EXPECT_EQ(ORCM_SUCCESS, opal_dss.unpack(buffer,&value, &num, OPAL_STRING));
    orcm_sensor_componentpower_module.inventory_log(value, buffer);
    EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);

    orcm_sensor_base.dbhandle = prev_value;
    OBJ_RELEASE(buffer);
}

TEST_F(ut_componentpower_tests, componentpower_start)
{
    orte_jobid_t mock_job = 44;
    int old_sample_rate = mca_sensor_componentpower_component.sample_rate;
    bool old_value = mca_sensor_componentpower_component.use_progress_thread;
    mca_sensor_componentpower_component.sample_rate = 0;
    mca_sensor_componentpower_component.use_progress_thread = true;

    orcm_sensor_componentpower_module.start(mock_job);
    EXPECT_EQ(orcm_sensor_base.sample_rate, mca_sensor_componentpower_component.sample_rate);
    orcm_sensor_componentpower_module.stop(mock_job);

    mca_sensor_componentpower_component.use_progress_thread = old_value;
    mca_sensor_componentpower_component.sample_rate = old_sample_rate;
}

TEST_F(ut_componentpower_tests, componentpower_start_negative)
{
    orte_jobid_t mock_job = 44;
    bool old_value = mca_sensor_componentpower_component.use_progress_thread;
    mca_sensor_componentpower_component.use_progress_thread = true;
    componentpower_mocking.opal_progress_thread_init_callback = OpalProgressThreadInitReturnNULL;

    orcm_sensor_componentpower_module.start(mock_job);

    mca_sensor_componentpower_component.use_progress_thread = old_value;
}


TEST_F(ut_componentpower_tests, componentpower_init)
{
    setInitTestFunctions();
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_componentpower_module.init());
    orcm_sensor_componentpower_module.finalize();
}

TEST_F(ut_componentpower_tests, componentpower_init_negative_load_msr_file)
{
    setInitTestFunctions();
    componentpower_mocking.open_callback = OpenReturnError;

    EXPECT_EQ(ORCM_ERR_FILE_OPEN_FAILURE, orcm_sensor_componentpower_module.init());
    orcm_sensor_componentpower_module.finalize();
}

TEST_F(ut_componentpower_tests, componentpower_init_negative_rapl_register)
{
    setInitTestFunctions();
    componentpower_mocking.read_callback = ReadReturnError;

    EXPECT_EQ(ORCM_ERR_RESOURCE_BUSY, orcm_sensor_componentpower_module.init());
    orcm_sensor_componentpower_module.finalize();

}

TEST_F(ut_componentpower_tests, componentpower_init_negative_energy_unit_check)
{
    setInitTestFunctions();

    for (int i=2; i < 7 ; i++){
        componentpower_mocking.read_return_error_callback = ReadReturnError;
        componentpower_mocking.ntimes = i;
        EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_componentpower_module.init());
        orcm_sensor_componentpower_module.finalize();
    }

}
