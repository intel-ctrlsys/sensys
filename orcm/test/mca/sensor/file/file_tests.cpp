/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "file_tests.h"

// ORTE
#include "orte/runtime/orte_globals.h"

// ORCM
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/file/sensor_file.h"

#define SAFE_FREE(x) SAFEFREE(x)

// Fixture
using namespace std;

extern "C" {
    ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
    ORTE_DECLSPEC extern orte_proc_info_t orte_process_info;
    extern ActivateJobState_fn_t test_file_activate_job_state;

    extern void collect_file_sample(orcm_sensor_sampler_t *sampler);
    extern orcm_sensor_base_module_t orcm_sensor_file_module;
    extern int file_enable_sampling(const char* sensor_specification);
    extern int file_disable_sampling(const char* sensor_specification);
    extern int file_reset_sampling(const char* sensor_specification);
};

orte_job_t ut_file_tests::job1;
orte_job_t ut_file_tests::job2;
const ActivateJobState_fn_t ut_file_tests::OriginalActivateJobState = test_file_activate_job_state;

#define WILDCARD (0xfffffffe)

void ut_file_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();

    OBJ_CONSTRUCT(&job1, orte_job_t);
    orte_app_context_t* app = OBJ_NEW(orte_app_context_t);
    app->env = (char**)malloc(sizeof(char*) * 6);
    app->env[0] = NULL;
    app->env[1] = NULL;
    app->env[2] = NULL;
    app->env[3] = NULL;
    app->env[4] = NULL;
    app->env[5] = NULL;
    opal_pointer_array_add(job1.apps, (void*)app);

    OBJ_CONSTRUCT(&job2, orte_job_t);
}

void ut_file_tests::TearDownTestCase()
{
    OBJ_DESTRUCT(&job2);
    OBJ_DESTRUCT(&job1);

    // Release OPAL level resources
    opal_dss_close();
}

void ut_file_tests::ResetTestCase()
{
    test_file_activate_job_state = OriginalActivateJobState;

    mca_sensor_file_component.collect_metrics = true;
    mca_sensor_file_component.limit = 0;
    mca_sensor_file_component.check_size = false;
    mca_sensor_file_component.check_mod = false;
    mca_sensor_file_component.check_access = false;
    mca_sensor_file_component.use_progress_thread = false;
    mca_sensor_file_component.sample_rate = 1;
    mca_sensor_file_component.file = NULL;

    file_mocking.orte_get_job_data_object_callback = NULL;
}

orte_job_t* ut_file_tests::GetJobDataObject(orte_jobid_t id)
{
    return &job1;
}

orte_job_t* ut_file_tests::GetJobDataObject2(orte_jobid_t id)
{
    return &job2;
}

void ut_file_tests::MockActivateJobState(orte_job_t* data, int flags)
{
cout << "*** DEBUG: MOCK: MockActivateJobState called!" << endl;
    EXPECT_EQ(ORTE_JOB_STATE_SENSOR_BOUND_EXCEEDED, flags);
}

void ut_file_tests::MakeTestFile(const char* file)
{
    const char* str = "test\n";
    FILE* fd = fopen(file, "w");
    fwrite(str, strlen(str), 1, fd);
    fclose(fd);
}

void ut_file_tests::ModifyTestFile(const char* file, bool size, bool access, bool modify)
{
    if(access) {
        char buffer[256];
        FILE* fd = fopen(file, "r");
        fgets((char*)buffer, 255, fd);
        fclose(fd);
        return;
    } else if(modify) {
        const char* str = "fred\n";
        FILE* fd = fopen(file, "w");
        fwrite(str, strlen(str), 1, fd);
        fclose(fd);
    } else if(size) {
        const char* str = "muchlargerstring\n";
        FILE* fd = fopen(file, "w");
        fwrite(str, strlen(str), 1, fd);
        fclose(fd);

    }
}


// Testing the data collection class
TEST_F(ut_file_tests, file_sensor_sample_tests)
{
    ResetTestCase();
    // Setup
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);
    mca_sensor_file_component.file = strdup("/bin/ls");
    mca_sensor_file_component.check_size = true;
    orcm_sensor_file_module.init();
    void* object = mca_sensor_file_component.runtime_metrics;
    ORTE_PROC_MY_NAME->jobid = 30;
    orcm_sensor_file_module.start(30);

    // Tests
    orcm_sensor_file_module.sample(sampler);
    EXPECT_EQ(1, (mca_sensor_file_component.diagnostics & 0x1));

    orcm_sensor_base_runtime_metrics_set(object, true, "file");
    orcm_sensor_file_module.sample(sampler);
    EXPECT_EQ(1, (mca_sensor_file_component.diagnostics & 0x1));

    // Cleanup
    SAFE_FREE(mca_sensor_file_component.file);
    OBJ_RELEASE(sampler);
    orcm_sensor_file_module.stop(30);
    orcm_sensor_file_module.finalize();
}

TEST_F(ut_file_tests, file_api_tests)
{
    ResetTestCase();
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("file", false, false);
    mca_sensor_file_component.runtime_metrics = object;

    // Tests
    file_enable_sampling("file");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    file_disable_sampling("file");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    file_enable_sampling("file");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    file_reset_sampling("file");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    file_enable_sampling("not_the_right_datagroup");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    file_enable_sampling("all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_file_component.runtime_metrics = NULL;
}

TEST_F(ut_file_tests, file_start_test)
{
    ResetTestCase();
    mca_sensor_file_component.file = (char*)"/bin/ls";
    mca_sensor_file_component.check_size = false;
    mca_sensor_file_component.use_progress_thread = false;

    file_mocking.orte_get_job_data_object_callback = GetJobDataObject;

    orcm_sensor_file_module.init();
    orcm_sensor_file_module.start(25);

    orcm_sensor_file_module.stop(25);
    orcm_sensor_file_module.finalize();
}

TEST_F(ut_file_tests, file_start_test_2)
{
    ResetTestCase();
    mca_sensor_file_component.file = (char*)"/bin/ls";
    mca_sensor_file_component.check_size = true;
    mca_sensor_file_component.use_progress_thread = false;

    file_mocking.orte_get_job_data_object_callback = GetJobDataObject;
    orte_app_context_t* app = (orte_app_context_t*)opal_pointer_array_get_item(job1.apps, 0);
    app->env[0] = (char*)"ORCM_MCA_sensor_file_filename=/bin/ls";
    orcm_sensor_file_module.init();
    ORTE_PROC_MY_NAME->jobid = 30;
    orcm_sensor_file_module.start(25);

    orcm_sensor_file_module.stop(25);
    orcm_sensor_file_module.finalize();

    app->env[0] = NULL;
}

TEST_F(ut_file_tests, file_start_test_4)
{
    ResetTestCase();
    mca_sensor_file_component.file = (char*)"/bin/ls";
    mca_sensor_file_component.check_size = true;
    mca_sensor_file_component.use_progress_thread = false;

    file_mocking.orte_get_job_data_object_callback = GetJobDataObject2;
    orcm_sensor_file_module.init();
    ORTE_PROC_MY_NAME->jobid = WILDCARD;
    orcm_sensor_file_module.start(WILDCARD);

    orcm_sensor_file_module.stop(WILDCARD);
    orcm_sensor_file_module.finalize();
}

TEST_F(ut_file_tests, file_start_test_5)
{
    ResetTestCase();
    mca_sensor_file_component.file = (char*)"/bin/ls";
    mca_sensor_file_component.check_size = true;
    mca_sensor_file_component.use_progress_thread = false;

    file_mocking.orte_get_job_data_object_callback = GetJobDataObject;
    orte_app_context_t* app = (orte_app_context_t*)opal_pointer_array_get_item(job1.apps, 0);
    app->env[0] = (char*)"ORCM_MCA_sensor_file_filename=/bin/ls";
    app->env[1] = (char*)"ORCM_MCA_sensor_file_check_size=false";
    app->env[2] = (char*)"ORCM_MCA_sensor_file_check_access=false";
    app->env[3] = (char*)"ORCM_MCA_sensor_file_check_mod=false";
    app->env[4] = (char*)"ORCM_MCA_sensor_file_limit=5";
    orcm_sensor_file_module.init();
    ORTE_PROC_MY_NAME->jobid = 30;
    orcm_sensor_file_module.start(25);

    orcm_sensor_file_module.stop(25);
    orcm_sensor_file_module.finalize();

    app->env[0] = NULL;
    app->env[1] = NULL;
    app->env[2] = NULL;
    app->env[3] = NULL;
    app->env[4] = NULL;
}

TEST_F(ut_file_tests, file_perthread_sample_test)
{
    ResetTestCase();
    // Setup
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    file_mocking.orte_get_job_data_object_callback = GetJobDataObject;
    mca_sensor_file_component.file = strdup("test_file");
    MakeTestFile("test_file");
    mca_sensor_file_component.collect_metrics = true;
    mca_sensor_file_component.limit = 3;
    mca_sensor_file_component.check_size = false;
    mca_sensor_file_component.check_mod = false;
    mca_sensor_file_component.check_access = false;
    mca_sensor_file_component.sample_rate = 1;
    mca_sensor_file_component.use_progress_thread = true;

    orcm_sensor_file_module.init();
    ORTE_PROC_MY_NAME->jobid = 1;
    orcm_sensor_file_module.start(0);

    orcm_sensor_file_module.sample(sampler);

    sleep(2);

    // Cleanup
    remove("test_file");
    SAFEFREE(mca_sensor_file_component.file);
    OBJ_RELEASE(sampler);
    orcm_sensor_file_module.stop(0);
    orcm_sensor_file_module.finalize();
}

TEST_F(ut_file_tests, file_active_job_state_test)
{
    ResetTestCase();
    test_file_activate_job_state(NULL, ORTE_JOB_STATE_SENSOR_BOUND_EXCEEDED);
    test_file_activate_job_state = MockActivateJobState;
    test_file_activate_job_state(NULL, ORTE_JOB_STATE_SENSOR_BOUND_EXCEEDED);
}

TEST_F(ut_file_tests, file_file_size_test)
{
    ResetTestCase();
    // Setup
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    file_mocking.orte_get_job_data_object_callback = GetJobDataObject;
    test_file_activate_job_state = MockActivateJobState;

    MakeTestFile("test_file");

    mca_sensor_file_component.file = strdup("test_file");
    mca_sensor_file_component.collect_metrics = true;
    mca_sensor_file_component.limit = 3;
    mca_sensor_file_component.check_size = true;
    mca_sensor_file_component.check_mod = false;
    mca_sensor_file_component.check_access = false;

    orcm_sensor_file_module.init();
    ORTE_PROC_MY_NAME->jobid = 1;
    orcm_sensor_file_module.start(0);

    orcm_sensor_file_module.sample(sampler);
    orcm_sensor_file_module.sample(sampler);
    orcm_sensor_file_module.sample(sampler);
    orcm_sensor_file_module.sample(sampler);

    // Cleanup
    remove("test_file");
    SAFEFREE(mca_sensor_file_component.file);
    OBJ_RELEASE(sampler);
    orcm_sensor_file_module.stop(0);
    orcm_sensor_file_module.finalize();
}

TEST_F(ut_file_tests, file_file_mod_test)
{
    ResetTestCase();
    // Setup
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    file_mocking.orte_get_job_data_object_callback = GetJobDataObject;
    test_file_activate_job_state = MockActivateJobState;

    MakeTestFile("test_file");

    mca_sensor_file_component.file = strdup("test_file");
    mca_sensor_file_component.collect_metrics = true;
    mca_sensor_file_component.limit = 3;
    mca_sensor_file_component.check_size = false;
    mca_sensor_file_component.check_mod = true;
    mca_sensor_file_component.check_access = false;

    orcm_sensor_file_module.init();
    ORTE_PROC_MY_NAME->jobid = 1;
    orcm_sensor_file_module.start(0);

    orcm_sensor_file_module.sample(sampler);
    orcm_sensor_file_module.sample(sampler);
    orcm_sensor_file_module.sample(sampler);
    orcm_sensor_file_module.sample(sampler);

    // Cleanup
    remove("test_file");
    SAFEFREE(mca_sensor_file_component.file);
    OBJ_RELEASE(sampler);
    orcm_sensor_file_module.stop(0);
    orcm_sensor_file_module.finalize();
}

TEST_F(ut_file_tests, file_file_access_test)
{
    ResetTestCase();
    // Setup
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    file_mocking.orte_get_job_data_object_callback = GetJobDataObject;
    test_file_activate_job_state = MockActivateJobState;

    MakeTestFile("test_file");

    mca_sensor_file_component.file = strdup("test_file");
    mca_sensor_file_component.collect_metrics = true;
    mca_sensor_file_component.limit = 3;
    mca_sensor_file_component.check_size = false;
    mca_sensor_file_component.check_mod = false;
    mca_sensor_file_component.check_access = true;

    orcm_sensor_file_module.init();
    ORTE_PROC_MY_NAME->jobid = 1;
    orcm_sensor_file_module.start(0);

    orcm_sensor_file_module.sample(sampler);
    orcm_sensor_file_module.sample(sampler);
    orcm_sensor_file_module.sample(sampler);
    orcm_sensor_file_module.sample(sampler);

    // Cleanup
    remove("test_file");
    SAFEFREE(mca_sensor_file_component.file);
    OBJ_RELEASE(sampler);
    orcm_sensor_file_module.stop(0);
    orcm_sensor_file_module.finalize();
}

TEST_F(ut_file_tests, file_component_test)
{
    mca_base_module_t* mod = NULL;
    int priority = -1;

    mca_sensor_file_component.super.base_version.mca_register_component_params();
    mca_sensor_file_component.super.base_version.mca_query_component(&mod, &priority);
    EXPECT_NE((uint64_t)0, (uint64_t)mod);
    EXPECT_EQ(20, priority);

    EXPECT_EQ(ORCM_SUCCESS, mca_sensor_file_component.super.base_version.mca_open_component());
    EXPECT_EQ(ORCM_SUCCESS, mca_sensor_file_component.super.base_version.mca_close_component());
}
