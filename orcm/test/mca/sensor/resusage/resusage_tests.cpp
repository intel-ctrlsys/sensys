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
#include "orte/util/proc_info.h"
// OPAL
#include "opal/dss/dss.h"

// ORCM
#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/resusage/sensor_resusage.h"

// Fixture
using namespace std;

extern "C" {
    ORCM_DECLSPEC extern mca_base_framework_t orcm_sensor_base_framework;
    ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
    extern orcm_sensor_base_module_t orcm_sensor_resusage_module;
    extern void collect_resusage_sample(orcm_sensor_sampler_t *sampler);
    extern int resusage_enable_sampling(const char* sensor_specification);
    extern int resusage_disable_sampling(const char* sensor_specification);
    extern int resusage_reset_sampling(const char* sensor_specification);
    extern void generate_test_vector(opal_buffer_t *v);
};

orte_job_t ut_resusage_tests::job1;
orte_job_t ut_resusage_tests::job2;
orte_proc_t ut_resusage_tests::mocked_process;
orte_node_t ut_resusage_tests::mocked_node;
opal_process_name_t ut_resusage_tests::mocked_name;
int ut_resusage_tests::err_num;

void ut_resusage_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();

    OBJ_CONSTRUCT(&job1, orte_job_t);
    OBJ_CONSTRUCT(&job2, orte_job_t);
    OBJ_CONSTRUCT(&mocked_process, orte_proc_t);
    OBJ_CONSTRUCT(&mocked_node, orte_node_t);
    mocked_name.jobid = 0;
    mocked_name.vpid = 0;
    mocked_process.name.jobid = 1;
    mocked_process.name.vpid = 1;
    mocked_process.node = &mocked_node;
    err_num = 0;
}

void ut_resusage_tests::TearDownTestCase()
{
    // Release OPAL level resources
    opal_dss_close();

    OBJ_DESTRUCT(&job1);
    OBJ_DESTRUCT(&job2);
    mocked_process.node = NULL;
    OBJ_DESTRUCT(&mocked_node);
    OBJ_DESTRUCT(&mocked_process);
}

orte_job_t* ut_resusage_tests::GetJobDataObject1(orte_jobid_t id)
{
    cout << "job1 was requested" << endl;

    return &job1;
}

orte_job_t* ut_resusage_tests::GetJobDataObject2(orte_jobid_t id)
{
    cout << "job2 was requested" << endl;

    return &job2;
}

void * ut_resusage_tests::ProvideMockedProcess(opal_pointer_array_t *table, int element_num)
{
    cout << "an array item was requested" << endl;

    return &mocked_process;
}

void ut_resusage_tests::MockedOrcmAnalyticsSend(orcm_analytics_value_t *data)
{
    cout << "orcm analytics send was performed" << endl;

    return;
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

TEST_F(ut_resusage_tests, init_jdata_not_null)
{
    resusage_mocking.orte_get_job_data_object_callback = GetJobDataObject1;
    EXPECT_EQ(ORCM_ERR_NOT_FOUND, orcm_sensor_resusage_module.init());
}

TEST_F(ut_resusage_tests, init_my_proc_null)
{
    resusage_mocking.orte_get_job_data_object_callback = GetJobDataObject2;
    EXPECT_EQ(ORCM_ERR_NOT_FOUND, orcm_sensor_resusage_module.init());
}

TEST_F(ut_resusage_tests, generate_test_vector_failed_pstat_query)
{
    opal_buffer_t *test_buffer = NULL;
    int n = 1;
    orcm_sensor_sampler_t* test_sampler = (orcm_sensor_sampler_t*)
                                          OBJ_NEW(orcm_sensor_sampler_t);
    orcm_sensor_base.collect_metrics=true;
    mca_sensor_resusage_component.collect_metrics=true;
    orcm_sensor_resusage_module.init();
    mca_sensor_resusage_component.log_node_stats = true;
    mca_sensor_resusage_component.test = true;
    collect_resusage_sample(test_sampler);

    EXPECT_EQ(ORCM_SUCCESS, opal_dss.unpack(&test_sampler->bucket, &test_buffer,
                                           &n, OPAL_BUFFER));
}

TEST_F(ut_resusage_tests, generate_test_vector_failed_pack)
{
    opal_buffer_t *test_buffer = NULL;
    int n = 1;
    orcm_sensor_sampler_t* test_sampler = (orcm_sensor_sampler_t*)
                                          OBJ_NEW(orcm_sensor_sampler_t);
    orcm_sensor_base.collect_metrics=true;
    EXPECT_TRUE(orcm_sensor_base.collect_metrics);
    mca_sensor_resusage_component.collect_metrics=true;
    EXPECT_TRUE(mca_sensor_resusage_component.collect_metrics);
    orcm_sensor_resusage_module.init();
    EXPECT_EQ(0, orte_process_info.pid);
    orte_process_info.pid = getpid();
    EXPECT_NE(0, orte_process_info.pid);
    mca_sensor_resusage_component.log_node_stats = true;
    EXPECT_TRUE(mca_sensor_resusage_component.log_node_stats);
    mca_sensor_resusage_component.test = true;
    EXPECT_TRUE(mca_sensor_resusage_component.test);
    opal_dss_close();
    collect_resusage_sample(test_sampler);
    opal_dss_register_vars();
    opal_dss_open();
    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER,
              opal_dss.unpack(&test_sampler->bucket, &test_buffer, &n,
              OPAL_BUFFER));
}

TEST_F(ut_resusage_tests, collect_resusage_sample_failed_pstat_query)
{
    opal_buffer_t *test_buffer = NULL;
    int n = 1;
    orcm_sensor_sampler_t* test_sampler = (orcm_sensor_sampler_t*)
                                          OBJ_NEW(orcm_sensor_sampler_t);
    orcm_sensor_base.collect_metrics=true;
    mca_sensor_resusage_component.collect_metrics=true;
    orcm_sensor_resusage_module.init();
    mca_sensor_resusage_component.log_node_stats = true;
    mca_sensor_resusage_component.test = false;

    collect_resusage_sample(test_sampler);
    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER,
              opal_dss.unpack(&test_sampler->bucket, &test_buffer, &n,
              OPAL_BUFFER));
}

TEST_F(ut_resusage_tests, collect_resusage_sample_failed_pack)
{
    opal_buffer_t *test_buffer = NULL;
    int n = 1;
    orcm_sensor_sampler_t* test_sampler = (orcm_sensor_sampler_t*)
                                          OBJ_NEW(orcm_sensor_sampler_t);
    orcm_sensor_base.collect_metrics=true;
    mca_sensor_resusage_component.collect_metrics=true;
    orcm_sensor_resusage_module.init();
    mca_sensor_resusage_component.log_node_stats = true;
    mca_sensor_resusage_component.test = false;
    opal_dss_close();
    collect_resusage_sample(test_sampler);
    opal_dss_register_vars();
    opal_dss_open();
    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER,
              opal_dss.unpack(&test_sampler->bucket, &test_buffer, &n,
                              OPAL_BUFFER));
}

TEST_F(ut_resusage_tests, res_log_first_unpack_fail)
{
    opal_buffer_t *test_buffer = NULL;
    int test_int = 0;
    int n = 1;
    int rc = 0;
    test_buffer = OBJ_NEW(opal_buffer_t);
    opal_dss.pack(test_buffer, &test_int, 1, OPAL_INT);
    orcm_sensor_resusage_module.log(test_buffer);
    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER,
              opal_dss.unpack(test_buffer, &test_int, &n, OPAL_INT));
}

TEST_F(ut_resusage_tests, res_log_second_unpack_fail)
{
    opal_buffer_t *test_buffer = NULL;
    const char *test_string = "some_string";
    char *tmp = NULL;
    int test_int = 0;
    int n = 1;
    int rc = 0;
    test_buffer = OBJ_NEW(opal_buffer_t);
    opal_dss.pack(test_buffer, &test_string, 1, OPAL_STRING);
    opal_dss.pack(test_buffer, &test_string, 1, OPAL_STRING);
    orcm_sensor_resusage_module.log(test_buffer);
    EXPECT_EQ(OPAL_ERR_UNPACK_FAILURE, opal_dss.unpack(test_buffer, &tmp, &n,
                                                       OPAL_STRING));
}

TEST_F(ut_resusage_tests, res_log_third_unpack_fail)
{
    opal_buffer_t *test_buffer = NULL;
    struct timeval test_time;
    const char *test_string = "some_string";
    char *tmp = NULL;
    int test_int = 0;
    int n = 1;
    int rc = 0;
    test_buffer = OBJ_NEW(opal_buffer_t);
    opal_dss.pack(test_buffer, &test_string, 1, OPAL_STRING);
    opal_dss.pack(test_buffer, &test_time, 1, OPAL_TIMEVAL);
    opal_dss.pack(test_buffer, &test_string, 1, OPAL_STRING);
    orcm_sensor_resusage_module.log(test_buffer);
    EXPECT_EQ(OPAL_ERR_UNPACK_FAILURE, opal_dss.unpack(test_buffer, &tmp, &n,
                                                       OPAL_STRING));
}

TEST_F(ut_resusage_tests, res_log_node_stats_valid)
{
    opal_buffer_t *test_buffer = NULL;
    opal_node_stats_t *test_node_stats = NULL;
    struct timeval test_time;
    const char *test_string = "some_string";
    char *tmp = NULL;
    int test_int = 0;
    int n = 1;
    int rc = 0;
    test_buffer = OBJ_NEW(opal_buffer_t);
    test_node_stats = OBJ_NEW(opal_node_stats_t);
    test_node_stats->total_mem=1.0;
    opal_dss.pack(test_buffer, &test_string, 1, OPAL_STRING);
    opal_dss.pack(test_buffer, &test_time, 1, OPAL_TIMEVAL);
    opal_dss.pack(test_buffer, &test_node_stats, 1, OPAL_NODE_STAT);
    mca_sensor_resusage_component.log_node_stats = true;
    resusage_mocking.orcm_analytics_base_send_data_callback =
        MockedOrcmAnalyticsSend;
    orcm_sensor_resusage_module.log(test_buffer);
    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER,
               opal_dss.unpack(test_buffer, &tmp, &n, OPAL_STRING));
}

TEST_F(ut_resusage_tests, resusage_component_open_query_close)
{
    const char* string1 = "ORCM_MCA_sensor=resusage";
    const char* string2 = "ORCM_MCA_sensor=^componentpower,coretemp,dmidata,"
                         "errcounts,evinj,file,freq,ft_tests,heartbeat,ipmi,"
                         "mcedata,nodepower,resusage,sigar,snmp,systlog,test";
    mca_base_open_flag_t test_open_flag = (mca_base_open_flag_t)0;
    putenv((char *)string1);
    putenv((char *)string2);


    EXPECT_EQ(ORCM_SUCCESS, mca_base_framework_open(
                  &orcm_sensor_base_framework, test_open_flag));
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_base_select());
    EXPECT_EQ(ORCM_SUCCESS, mca_base_framework_close(&orcm_sensor_base_framework));
}
