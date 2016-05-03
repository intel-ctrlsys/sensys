/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "mcedata_tests.h"
#include "mcedata_tests_mocking.h"

// ORTE
#include "orte/runtime/orte_globals.h"

// ORCM
#include "orcm/mca/mca.h"
#include "orcm/mca/sensor/sensor.h"
#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/mcedata/sensor_mcedata.h"


// Fixture
using namespace std;

extern "C" {
    ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
    extern void collect_mcedata_sample(orcm_sensor_sampler_t *sampler);
    extern int mcedata_enable_sampling(const char* sensor_specification);
    extern int mcedata_disable_sampling(const char* sensor_specification);
    extern int mcedata_reset_sampling(const char* sensor_specification);
    extern void mcedata_unknown_filter(unsigned long *mce_reg, opal_list_t *vals);
    extern void mcedata_gen_cache_filter(unsigned long *mce_reg, opal_list_t *vals);
    extern void mcedata_tlb_filter(unsigned long *mce_reg, opal_list_t *vals);
    extern void mcedata_cache_filter(unsigned long *mce_reg, opal_list_t *vals);
    extern void mcedata_bus_ic_filter(unsigned long *mce_reg, opal_list_t *vals);
};

void ut_mcedata_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();
}

void ut_mcedata_tests::TearDownTestCase()
{
    // Release OPAL level resources
    opal_dss_close();
}


DIR* ut_mcedata_tests::OpenDir(const char* dirname)
{
    (void)dirname;
    int* rv = new int;
    *rv = 0;
    return (DIR*)rv;
}

int ut_mcedata_tests::CloseDir(DIR* dir_fd)
{
    delete (int*)dir_fd;
    return 0;
}

static const char* entries[] = {
    ".",
    "..",
    "mcelog",
    (const char*)NULL
};

struct dirent* ut_mcedata_tests::ReadDir(DIR* dir_fd)
{
    static struct dirent dir;
    memset(&dir, 0, sizeof(struct dirent));
    int* index = (int*)dir_fd;
    if(entries[*index] != NULL) {
        strcpy(dir.d_name, entries[*index]);
        ++(*index);
        return &dir;
    } else {
        return NULL;
    }
}

void ut_mcedata_tests::StoreNew(int dbhandle, orcm_db_data_type_t data_type,
                                opal_list_t *input, opal_list_t *ret,
                                orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    cbfunc(dbhandle, ORCM_SUCCESS, input, ret, cbdata);
}

void ut_mcedata_tests::SendData(orcm_analytics_value_t *analytics_vals)
{
}


// Testing the data collection class
TEST_F(ut_mcedata_tests, mcedata_sensor_sample_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("mcedata", false, false);
    mca_sensor_mcedata_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    // Tests
    collect_mcedata_sample(sampler);
    EXPECT_EQ(0, (mca_sensor_mcedata_component.diagnostics & 0x1));

    orcm_sensor_base_runtime_metrics_set(object, true, "mcedata");
    collect_mcedata_sample(sampler);
    EXPECT_EQ(1, (mca_sensor_mcedata_component.diagnostics & 0x1));

    // Cleanup
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_mcedata_component.runtime_metrics = NULL;
}

TEST_F(ut_mcedata_tests, mcedata_api_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("mcedata", false, false);
    mca_sensor_mcedata_component.runtime_metrics = object;

    // Tests
    mcedata_enable_sampling("mcedata");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    mcedata_disable_sampling("mcedata");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    mcedata_enable_sampling("mcedata");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    mcedata_reset_sampling("mcedata");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    mcedata_enable_sampling("not_the_right_datagroup");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    mcedata_enable_sampling("all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_mcedata_component.runtime_metrics = NULL;
}

TEST_F(ut_mcedata_tests, mcedata_query_test)
{
    mca_base_module_t* module = NULL;
    int priority = 0;

    int rv = (*mca_sensor_mcedata_component.super.base_version.mca_query_component)(&module, &priority);
    ASSERT_EQ(ORCM_SUCCESS, rv);
    ASSERT_EQ(50, priority);
    ASSERT_TRUE(NULL != module);
}

TEST_F(ut_mcedata_tests, mcedata_open_close_test)
{
    int rv = (*mca_sensor_mcedata_component.super.base_version.mca_open_component)();
    ASSERT_EQ(ORCM_SUCCESS, rv);
    rv = (*mca_sensor_mcedata_component.super.base_version.mca_close_component)();
    ASSERT_EQ(ORCM_SUCCESS, rv);
}

TEST_F(ut_mcedata_tests, mcedata_register_test)
{
    int rv = (*mca_sensor_mcedata_component.super.base_version.mca_register_component_params)();
    ASSERT_EQ(ORCM_SUCCESS, rv);
}

TEST_F(ut_mcedata_tests, mcedata_init_start_stop_finalize_test)
{
    mcedata_mocking.opendir_callback = OpenDir;
    mcedata_mocking.closedir_callback = CloseDir;
    mcedata_mocking.readdir_callback = ReadDir;

    mca_sensor_mcedata_component.use_progress_thread = false;

    int rv = orcm_sensor_mcedata_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);

    orcm_sensor_mcedata_module.start(2);
    orcm_sensor_mcedata_module.stop(2);

    orcm_sensor_mcedata_module.finalize();

    mcedata_mocking.opendir_callback = NULL;
    mcedata_mocking.closedir_callback = NULL;
    mcedata_mocking.readdir_callback = NULL;
}

TEST_F(ut_mcedata_tests, mcedata_init_start_stop_finalize_test2)
{
    mca_sensor_mcedata_component.test = true;
    mca_sensor_mcedata_component.use_progress_thread = true;
    mca_sensor_mcedata_component.sample_rate = 1;

    mcedata_mocking.opendir_callback = OpenDir;
    mcedata_mocking.closedir_callback = CloseDir;
    mcedata_mocking.readdir_callback = ReadDir;

    int rv = orcm_sensor_mcedata_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);

    orcm_sensor_mcedata_module.start(5);

    sleep(2);

    orcm_sensor_mcedata_module.stop(5);

    orcm_sensor_mcedata_module.finalize();

    mcedata_mocking.opendir_callback = NULL;
    mcedata_mocking.closedir_callback = NULL;
    mcedata_mocking.readdir_callback = NULL;
    mca_sensor_mcedata_component.use_progress_thread = false;
    mca_sensor_mcedata_component.test = false;
}

TEST_F(ut_mcedata_tests, mcedata_sample_log_test)
{
    mca_sensor_mcedata_component.test = true;
    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);

    orcm_sensor_mcedata_module.sample(&sampler);

    // Unpack whole buffer...
    int n = 1;
    opal_buffer_t* buffer = NULL;
    int rv = opal_dss.unpack(&sampler.bucket, &buffer, &n, OPAL_BUFFER);
    OBJ_DESTRUCT(&sampler);
    EXPECT_EQ(ORCM_SUCCESS, rv);
    EXPECT_FALSE(NULL == buffer);

    // Unpack plugin name...
    char* plugin = NULL;
    n = 1;
    rv = opal_dss.unpack(buffer, &plugin, &n, OPAL_STRING);
    EXPECT_EQ(ORCM_SUCCESS, rv);
    EXPECT_FALSE(NULL == plugin);
    EXPECT_STREQ("mcedata", plugin);
    if(NULL != plugin) {
        free(plugin);
        plugin = NULL;
    }

    orcm_analytics_API_module_send_data_fn_t saved = orcm_analytics.send_data;
    orcm_analytics.send_data = SendData;
    orcm_sensor_mcedata_module.log(buffer);
    orcm_analytics.send_data = saved;

    if(NULL != buffer) {
        OBJ_RELEASE(buffer);
        buffer = NULL;
    }

    mca_sensor_mcedata_component.test = false;
}

TEST_F(ut_mcedata_tests, mcedata_inventory_sample_log_test)
{
    opal_buffer_t buffer;
    OBJ_CONSTRUCT(&buffer, opal_buffer_t);

    orcm_sensor_mcedata_module.inventory_collect(&buffer);

    // Unpack plugin name...
    int n = 1;
    char* plugin = NULL;
    int rv = opal_dss.unpack(&buffer, &plugin, &n, OPAL_STRING);
    EXPECT_EQ(ORCM_SUCCESS, rv);
    EXPECT_FALSE(NULL == plugin);
    EXPECT_STREQ("mcedata", plugin);
    if(NULL != plugin) {
        free(plugin);
        plugin = NULL;
    }
    orcm_db_base_API_store_new_fn_t saved = orcm_db.store_new;
    orcm_db.store_new = StoreNew;
    orcm_sensor_mcedata_module.inventory_log((char*)"test_host", &buffer);
    orcm_db.store_new = saved;

    OBJ_DESTRUCT(&buffer);
}

TEST_F(ut_mcedata_tests, mcedata_sample_rate_test)
{
    mca_sensor_mcedata_component.use_progress_thread = true;
    mca_sensor_mcedata_component.sample_rate = 5;
    int sample_rate = 0;
    orcm_sensor_mcedata_module.get_sample_rate(&sample_rate);
    EXPECT_EQ(5, sample_rate);
    orcm_sensor_mcedata_module.set_sample_rate(7);
    orcm_sensor_mcedata_module.get_sample_rate(&sample_rate);
    EXPECT_EQ(7, sample_rate);
    mca_sensor_mcedata_component.use_progress_thread = false;
    orcm_sensor_mcedata_module.get_sample_rate(&sample_rate);
    EXPECT_EQ(7, sample_rate);
    orcm_sensor_mcedata_module.set_sample_rate(3);
    orcm_sensor_mcedata_module.get_sample_rate(&sample_rate);
    EXPECT_EQ(7, sample_rate);
}

TEST_F(ut_mcedata_tests, mcedata_unknown_filter_test)
{
    unsigned long reg = 0;
    opal_list_t* vals = OBJ_NEW(opal_list_t);
    mcedata_unknown_filter(&reg, vals);
    EXPECT_EQ(1, opal_list_get_size(vals));
    ORCM_RELEASE(vals);
}

TEST_F(ut_mcedata_tests, mcedata_bus_ic_filter_test)
{
    uint64_t mce_reg[MCE_REG_COUNT];
    memset(mce_reg, 0, sizeof(mce_reg));
    opal_list_t* vals = OBJ_NEW(opal_list_t);

    mcedata_bus_ic_filter(mce_reg, vals);
    EXPECT_EQ(6, opal_list_get_size(vals));

    ORCM_RELEASE(vals);
}

TEST_F(ut_mcedata_tests, mcedata_gen_cache_filter_test)
{
    uint64_t mce_reg[MCE_REG_COUNT];
    memset(mce_reg, 0, sizeof(mce_reg));
    opal_list_t* vals = OBJ_NEW(opal_list_t);

    mcedata_gen_cache_filter(mce_reg, vals);
    EXPECT_EQ(2, opal_list_get_size(vals));

    ORCM_RELEASE(vals);
}

TEST_F(ut_mcedata_tests, mcedata_tlb_filter_test)
{
    uint64_t mce_reg[MCE_REG_COUNT];
    memset(mce_reg, 0, sizeof(mce_reg));
    opal_list_t* vals = OBJ_NEW(opal_list_t);

    mcedata_tlb_filter(mce_reg, vals);
    EXPECT_EQ(1, opal_list_get_size(vals));

    ORCM_RELEASE(vals);
}

TEST_F(ut_mcedata_tests, mcedata_cache_filter_test)
{
    uint64_t mce_reg[MCE_REG_COUNT];
    memset(mce_reg, 0, sizeof(mce_reg));
    mce_reg[2] = ((uint64_t)1<<63);
    opal_list_t* vals = OBJ_NEW(opal_list_t);

    mcedata_cache_filter(mce_reg, vals);
    EXPECT_EQ(5, opal_list_get_size(vals));

    ORCM_RELEASE(vals);
}
