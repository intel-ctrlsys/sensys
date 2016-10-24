/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "nodepower_tests.h"

// ORTE
#include "orte/runtime/orte_globals.h"

// ORCM
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/nodepower/sensor_nodepower.h"

// Fixture
using namespace std;

extern "C" {
    ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
    extern orcm_sensor_nodepower_t orcm_sensor_nodepower;
    extern void collect_nodepower_sample(orcm_sensor_sampler_t *sampler);
    extern int nodepower_enable_sampling(const char* sensor_specification);
    extern int nodepower_disable_sampling(const char* sensor_specification);
    extern int nodepower_reset_sampling(const char* sensor_specification);
    extern void perthread_nodepower_sample(int fd, short args, void *cbdata);
};

/* Sample ipmi_cmdraw data for PSU2*/
int ut_nodepower_tests::psu02_index = 0;
unsigned char ut_nodepower_tests::psu02[53][7] = {
    { 0x06,0xa5,0x09,0x92,0xc8,0x37,0xbd },
    { 0x06,0xe5,0x16,0x92,0xe1,0x37,0xbd },
    { 0x06,0xe5,0x16,0x92,0xe1,0x37,0xbd },
    { 0x06,0xe5,0x16,0x92,0xe1,0x37,0xbd },
    { 0x06,0xe5,0x16,0x92,0xe1,0x37,0xbd },
    { 0x06,0xe5,0x16,0x92,0xe1,0x37,0xbd },
    { 0x06,0x68,0x17,0x92,0xe2,0x37,0xbd },
    { 0x06,0xac,0x26,0x92,0xfa,0x37,0xbd },
    { 0x06,0xd5,0x34,0x92,0x13,0x38,0xbd },
    { 0x06,0xd7,0x41,0x92,0x2c,0x38,0xbd },
    { 0x06,0xda,0x4e,0x92,0x45,0x38,0xbd },
    { 0x06,0x13,0x48,0x96,0xe5,0x3b,0xbd },
    { 0x06,0x4e,0x55,0x96,0xfe,0x3b,0xbd },
    { 0x06,0x4e,0x55,0x96,0xfe,0x3b,0xbd },
    { 0x06,0x4e,0x55,0x96,0xfe,0x3b,0xbd },
    { 0x06,0x4e,0x55,0x96,0xfe,0x3b,0xbd },
    { 0x06,0x4e,0x55,0x96,0xfe,0x3b,0xbd },
    { 0x06,0xd1,0x55,0x96,0xff,0x3b,0xbd },
    { 0x06,0x0f,0x63,0x96,0x17,0x3c,0xbd },
    { 0x06,0xde,0x73,0x96,0x30,0x3c,0xbd },
    { 0x06,0x81,0x01,0x97,0x49,0x3c,0xbd },
    { 0x06,0x8d,0x0e,0x97,0x62,0x3c,0xbd },
    { 0x06,0xe2,0x1e,0x97,0x7b,0x3c,0xbd },
    { 0x06,0xf9,0x2d,0x97,0x93,0x3c,0xbd },
    { 0x06,0xce,0x3e,0x97,0xac,0x3c,0xbd },
    { 0x06,0xac,0x4f,0x97,0xc5,0x3c,0xbd },
    { 0x06,0x89,0x60,0x97,0xde,0x3c,0xbd },
    { 0x06,0xdd,0x6d,0x97,0xf7,0x3c,0xbd },
    { 0x06,0xe1,0x7a,0x97,0x10,0x3d,0xbd },
    { 0x06,0xdd,0x07,0x98,0x29,0x3d,0xbd },
    { 0x06,0xe2,0x52,0x9d,0x46,0x42,0xbd },
    { 0x06,0x48,0x60,0x9d,0x5f,0x42,0xbd },
    { 0x06,0x48,0x60,0x9d,0x5f,0x42,0xbd },
    { 0x06,0x48,0x60,0x9d,0x5f,0x42,0xbd },
    { 0x06,0xce,0x60,0x9d,0x60,0x42,0xbd },
    { 0x06,0x4b,0x6d,0x9d,0x78,0x42,0xbd },
    { 0x06,0x4b,0x7a,0x9d,0x91,0x42,0xbd },
    { 0x06,0x47,0x07,0x9e,0xaa,0x42,0xbd },
    { 0x06,0x7e,0x15,0x9e,0xc3,0x42,0xbd },
    { 0x06,0x7b,0x22,0x9e,0xdc,0x42,0xbd },
    { 0x06,0xd1,0x2f,0x9e,0xf5,0x42,0xbd },
    { 0x06,0x8d,0x3d,0x9e,0x0e,0x43,0xbd },
    { 0x06,0x88,0x4a,0x9e,0x27,0x43,0xbd },
    { 0x06,0x73,0x57,0x9e,0x40,0x43,0xbd },
    { 0x06,0x8d,0x64,0x9e,0x59,0x43,0xbd },
    { 0x06,0x77,0x71,0x9e,0x72,0x43,0xbd },
    { 0x06,0x72,0x7e,0x9e,0x8b,0x43,0xbd },
    { 0x06,0xeb,0x0d,0x9f,0xa4,0x43,0xbd },
    { 0x06,0xc0,0x1a,0x9f,0xbc,0x43,0xbd },
    { 0x06,0xac,0x27,0x9f,0xd5,0x43,0xbd },
    { 0x06,0xb0,0x34,0x9f,0xee,0x43,0xbd },
    { 0x06,0xa4,0x41,0x9f,0x07,0x44,0xbd },
    { 0x06,0xfe,0x4e,0x9f,0x20,0x44,0xbd }
};
std::queue<int> ut_nodepower_tests::ipmi_error_queue;

void ut_nodepower_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();

    nodepower_mocking.ipmi_cmdraw_callback = IpmiCmdRaw;
    nodepower_mocking.ipmi_close_callback = IpmiClose;

    orcm_db.store_new = StoreNew;
    nodepower_mocking.geteuid_callback = GetUIDRoot;

}

void ut_nodepower_tests::TearDownTestCase()
{
    nodepower_mocking.ipmi_cmdraw_callback = NULL;
    nodepower_mocking.ipmi_close_callback = NULL;
    // Release OPAL level resources
    opal_dss_close();
}

void ut_nodepower_tests::ResetTest()
{
    while(0 < ipmi_error_queue.size())
        ipmi_error_queue.pop();

    mca_sensor_nodepower_component.test = false;
    mca_sensor_nodepower_component.collect_metrics = true;
    mca_sensor_nodepower_component.use_progress_thread = false;

    nodepower_mocking.geteuid_callback = GetUIDRoot;
    orcm_sensor_base.dbhandle = 20;
    nodepower_mocking.localtime_callback = NULL;
}

int ut_nodepower_tests::IpmiCmdRaw(unsigned char a,unsigned char b,unsigned char c,
                                   unsigned char d,unsigned char e,unsigned char* f,int g,
                                   unsigned char* h,int* i,unsigned char* j,unsigned char k)
{
    memcpy(h, &(psu02[psu02_index++]), 7);
    *i = 7;
    psu02_index = psu02_index % 53;
}

int ut_nodepower_tests::IpmiCmdRawFail(unsigned char a,unsigned char b,unsigned char c,
                                       unsigned char d,unsigned char e,unsigned char* f,int g,
                                       unsigned char* h,int* i,unsigned char* j,unsigned char k)
{
    if(0 == ipmi_error_queue.size())
        return ORCM_ERROR;
    else {
        int err = ipmi_error_queue.front();
        ipmi_error_queue.pop();
        return err;
    }
}

int ut_nodepower_tests::IpmiClose(void)
{
    return 0; // OK
}

void ut_nodepower_tests::StoreNew(int, orcm_db_data_type_t, opal_list_t* records,
                                  opal_list_t*, orcm_db_callback_fn_t cbfunc, void*)
{
    cbfunc(0, 0, records, NULL, NULL);
}

__uid_t ut_nodepower_tests::GetUIDNormal(void)
{
    return 1000;
}

__uid_t ut_nodepower_tests::GetUIDRoot(void)
{
    return 0;
}

struct tm* ut_nodepower_tests::LocalTimeFail(const time_t* tp)
{
    return NULL;
}


// Helpers
int ut_nodepower_tests::NPInit(void)
{
    int rc = ORCM_SUCCESS;
    EXPECT_EQ(ORCM_SUCCESS, (rc = orcm_sensor_nodepower_module.init()));
    return rc;
}


// Testing the data collection class
TEST_F(ut_nodepower_tests, nodepower_sensor_sample_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("nodepower", false, false);
    mca_sensor_nodepower_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    // Tests
    collect_nodepower_sample(sampler);
    EXPECT_EQ(0, (mca_sensor_nodepower_component.diagnostics & 0x1));

    orcm_sensor_base_runtime_metrics_set(object, true, "nodepower");
    collect_nodepower_sample(sampler);
    EXPECT_EQ(1, (mca_sensor_nodepower_component.diagnostics & 0x1));

    // Cleanup
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_nodepower_component.runtime_metrics = NULL;
}

TEST_F(ut_nodepower_tests, nodepower_api_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("nodepower", false, false);
    mca_sensor_nodepower_component.runtime_metrics = object;

    // Tests
    nodepower_enable_sampling("nodepower");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    nodepower_disable_sampling("nodepower");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    nodepower_enable_sampling("nodepower");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    nodepower_reset_sampling("nodepower");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    nodepower_enable_sampling("not_the_right_datagroup");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    nodepower_enable_sampling("all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_nodepower_component.runtime_metrics = NULL;
}

TEST_F(ut_nodepower_tests, nodepower_component_tests)
{
    mca_base_module_t* mod = NULL;
    int priority = -1;

    mca_sensor_nodepower_component.super.base_version.mca_register_component_params();
    mca_sensor_nodepower_component.super.base_version.mca_query_component(&mod, &priority);
    EXPECT_NE((uint64_t)0, (uint64_t)mod);
    EXPECT_EQ(50, priority);

    EXPECT_EQ(ORCM_SUCCESS, mca_sensor_nodepower_component.super.base_version.mca_open_component());
    EXPECT_EQ(ORCM_SUCCESS, mca_sensor_nodepower_component.super.base_version.mca_close_component());
}

TEST_F(ut_nodepower_tests, nodepower_sample_test_vector_tests)
{
    ResetTest();

    orcm_sensor_sampler_t sample;
    OBJ_CONSTRUCT(&sample, orcm_sensor_sampler_t);
    mca_sensor_nodepower_component.test = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_nodepower_module.init());
    orcm_sensor_nodepower_module.start(2);
    orcm_sensor_nodepower_module.sample(&sample);
    orcm_sensor_nodepower_module.stop(2);
    orcm_sensor_nodepower_module.finalize();
    mca_sensor_nodepower_component.test = false;
    OBJ_DESTRUCT(&sample);
}

TEST_F(ut_nodepower_tests, nodepower_sample_log_tests)
{
    char* plugin = NULL;
    int n = 1;

    ResetTest();

    orcm_sensor_sampler_t sample;
    OBJ_CONSTRUCT(&sample, orcm_sensor_sampler_t);

    NPInit();
    orcm_sensor_nodepower_module.start(5);
    orcm_sensor_nodepower_module.sample(&sample);
    opal_buffer_t* buffer = NULL;
    EXPECT_EQ(ORCM_SUCCESS, opal_dss.unpack(&sample.bucket, &buffer, &n, OPAL_BUFFER));
    n = 1;
    EXPECT_EQ(ORCM_SUCCESS, opal_dss.unpack(buffer, &plugin, &n, OPAL_STRING));
    SAFEFREE(plugin);
    orcm_sensor_nodepower_module.log(buffer);
    ORCM_RELEASE(buffer);

    nodepower_mocking.ipmi_cmdraw_callback = IpmiCmdRawFail;
    orcm_sensor_nodepower_module.sample(&sample);
    nodepower_mocking.ipmi_cmdraw_callback = IpmiCmdRaw;

    for(int i = 0; i < 51; ++i)
        orcm_sensor_nodepower_module.sample(&sample);

    orcm_sensor_nodepower_module.stop(5);
    orcm_sensor_nodepower_module.finalize();

    OBJ_DESTRUCT(&sample);
}

TEST_F(ut_nodepower_tests, nodepower_sample_log_tests_2)
{
    char* plugin = NULL;
    int n = 1;

    ResetTest();

    orcm_sensor_sampler_t sample;
    OBJ_CONSTRUCT(&sample, orcm_sensor_sampler_t);

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_nodepower_module.init());
    nodepower_mocking.ipmi_cmdraw_callback = IpmiCmdRawFail;
    ipmi_error_queue.push(ORCM_SUCCESS);
    ipmi_error_queue.push(ORCM_ERROR);
    orcm_sensor_nodepower_module.start(5);
    ipmi_error_queue.push(ORCM_ERROR);
    ipmi_error_queue.push(ORCM_SUCCESS);
    orcm_sensor_nodepower_module.start(5);
    nodepower_mocking.ipmi_cmdraw_callback = IpmiCmdRawFail;
    orcm_sensor_nodepower_module.start(5);
    orcm_sensor_nodepower_module.sample(&sample);
    opal_buffer_t* buffer = NULL;
    EXPECT_EQ(ORCM_SUCCESS, opal_dss.unpack(&sample.bucket, &buffer, &n, OPAL_BUFFER));
    n = 1;
    EXPECT_EQ(ORCM_SUCCESS, opal_dss.unpack(buffer, &plugin, &n, OPAL_STRING));
    SAFEFREE(plugin);
    orcm_sensor_nodepower_module.log(buffer);
    ORCM_RELEASE(buffer);

    nodepower_mocking.ipmi_cmdraw_callback = IpmiCmdRawFail;
    orcm_sensor_nodepower_module.sample(&sample);
    nodepower_mocking.ipmi_cmdraw_callback = IpmiCmdRaw;

    orcm_sensor_nodepower_module.stop(5);
    orcm_sensor_nodepower_module.finalize();

    OBJ_DESTRUCT(&sample);
}

TEST_F(ut_nodepower_tests, nodepower_sample_log_tests_3)
{
    char* plugin = NULL;
    int n = 1;

    ResetTest();

    orcm_sensor_sampler_t sample;
    OBJ_CONSTRUCT(&sample, orcm_sensor_sampler_t);

    NPInit();
    orcm_sensor_nodepower_module.start(5);
    orcm_sensor_nodepower_module.sample(&sample);
    opal_buffer_t* buffer = NULL;
    EXPECT_EQ(ORCM_SUCCESS, opal_dss.unpack(&sample.bucket, &buffer, &n, OPAL_BUFFER));
    n = 1;
    EXPECT_EQ(ORCM_SUCCESS, opal_dss.unpack(buffer, &plugin, &n, OPAL_STRING));
    SAFEFREE(plugin);
    orcm_sensor_base.dbhandle = -1;

    orcm_sensor_nodepower_module.log(buffer);

    ORCM_RELEASE(buffer);

    orcm_sensor_nodepower_module.stop(5);
    orcm_sensor_nodepower_module.finalize();

    OBJ_DESTRUCT(&sample);
}

TEST_F(ut_nodepower_tests, nodepower_sample_log_tests_4)
{
    char* plugin = NULL;
    int n = 1;

    ResetTest();

    orcm_sensor_sampler_t sample;
    OBJ_CONSTRUCT(&sample, orcm_sensor_sampler_t);

    NPInit();
    orcm_sensor_nodepower_module.start(5);
    orcm_sensor_nodepower_module.sample(&sample);
    opal_buffer_t* buffer = NULL;
    EXPECT_EQ(ORCM_SUCCESS, opal_dss.unpack(&sample.bucket, &buffer, &n, OPAL_BUFFER));
    n = 1;
    EXPECT_EQ(ORCM_SUCCESS, opal_dss.unpack(buffer, &plugin, &n, OPAL_STRING));
    SAFEFREE(plugin);
    orcm_sensor_base.dbhandle = -1;

    nodepower_mocking.localtime_callback = LocalTimeFail;
    orcm_sensor_nodepower_module.log(buffer);
    nodepower_mocking.localtime_callback = NULL;

    ORCM_RELEASE(buffer);

    orcm_sensor_nodepower_module.stop(5);
    orcm_sensor_nodepower_module.finalize();

    OBJ_DESTRUCT(&sample);
}

TEST_F(ut_nodepower_tests, nodepower_inventory_sample_log_tests)
{
    char* plugin = NULL;
    int n = 1;

    ResetTest();

    opal_buffer_t buffer;
    OBJ_CONSTRUCT(&buffer, opal_buffer_t);

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_nodepower_module.init());
    orcm_sensor_nodepower_module.start(7);
    orcm_sensor_nodepower_module.inventory_collect(&buffer);
    EXPECT_EQ(ORCM_SUCCESS, opal_dss.unpack(&buffer, &plugin, &n, OPAL_STRING));
    SAFEFREE(plugin);
    orcm_sensor_nodepower_module.inventory_log((char*)"ut_test_host", &buffer);

    orcm_sensor_nodepower_module.stop(7);
    orcm_sensor_nodepower_module.finalize();

    OBJ_DESTRUCT(&buffer);
}

TEST_F(ut_nodepower_tests, nodepower_user_init_test)
{
    ResetTest();
    nodepower_mocking.geteuid_callback = GetUIDNormal;
    EXPECT_EQ(ORCM_ERR_PERM, orcm_sensor_nodepower_module.init());
}

TEST_F(ut_nodepower_tests, nodepower_sample_rate_test)
{
    ResetTest();
    mca_sensor_nodepower_component.use_progress_thread = true;
    orcm_sensor_nodepower_module.set_sample_rate(12);
    mca_sensor_nodepower_component.use_progress_thread = false;
    orcm_sensor_nodepower_module.set_sample_rate(7);

    int sr = 0;
    orcm_sensor_nodepower_module.get_sample_rate(&sr);
    EXPECT_EQ(12, sr);
    orcm_sensor_nodepower_module.get_sample_rate(NULL);
}

TEST_F(ut_nodepower_tests, nodepower_start_tests)
{
    char* plugin = NULL;
    int n = 1;

    ResetTest();

    orcm_sensor_sampler_t sample;
    OBJ_CONSTRUCT(&sample, orcm_sensor_sampler_t);

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_nodepower_module.init());
    mca_sensor_nodepower_component.test = true;
    nodepower_mocking.ipmi_cmdraw_callback = IpmiCmdRawFail;
    ipmi_error_queue.push(ORCM_SUCCESS);
    ipmi_error_queue.push(ORCM_ERROR);
    orcm_sensor_nodepower_module.start(5);
    ipmi_error_queue.push(ORCM_ERROR);
    ipmi_error_queue.push(ORCM_SUCCESS);
    orcm_sensor_nodepower_module.start(5);
    nodepower_mocking.ipmi_cmdraw_callback = IpmiCmdRawFail;
    mca_sensor_nodepower_component.use_progress_thread = true;
    orcm_sensor_nodepower_module.start(5);

    orcm_sensor_nodepower_module.stop(5);
    orcm_sensor_nodepower_module.finalize();

    OBJ_DESTRUCT(&sample);

    mca_sensor_nodepower_component.test = false;
}
