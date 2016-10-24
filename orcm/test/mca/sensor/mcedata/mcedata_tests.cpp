/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
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
    extern long orcm_sensor_mcedata_get_log_file_pos(void);
    extern void mcedata_get_sample_rate(int *sample_rate);
    extern mcetype get_mcetype(uint64_t mci_status);
    extern void mcedata_decode(unsigned long *mce_reg, opal_list_t *vals);
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

int ut_mcedata_tests::FSeekError(FILE*,long,int)
{
    errno = EBADF;
    return -1;
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

TEST_F(ut_mcedata_tests, mcedata_init_start_stop_finalize_test3)
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
    orcm_sensor_mcedata_module.start(6);

    sleep(2);

    orcm_sensor_mcedata_module.stop(5);
    orcm_sensor_mcedata_module.stop(6);

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

TEST_F(ut_mcedata_tests, mcedata_start_log_file_test)
{
    mcedata_mocking.opendir_callback = OpenDir;
    mcedata_mocking.closedir_callback = CloseDir;
    mcedata_mocking.readdir_callback = ReadDir;

    mca_sensor_mcedata_component.use_progress_thread = false;
    char* saved_logfile = mca_sensor_mcedata_component.logfile;
    mca_sensor_mcedata_component.logfile = (char*)"./test.logfile";

    int rv = orcm_sensor_mcedata_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);

    orcm_sensor_mcedata_module.start(2);
    EXPECT_EQ(0, orcm_sensor_mcedata_get_log_file_pos());
    orcm_sensor_mcedata_module.stop(2);

    orcm_sensor_mcedata_module.finalize();

    remove("./test.logfile");

    mca_sensor_mcedata_component.logfile = saved_logfile;

    mcedata_mocking.opendir_callback = NULL;
    mcedata_mocking.closedir_callback = NULL;
    mcedata_mocking.readdir_callback = NULL;
}

TEST_F(ut_mcedata_tests, mcedata_start_log_file_test_2)
{
    mcedata_mocking.opendir_callback = OpenDir;
    mcedata_mocking.closedir_callback = CloseDir;
    mcedata_mocking.readdir_callback = ReadDir;

    FILE* tfd = fopen("./test.logfile", "w");
    ASSERT_NE((uint64_t)0, (uint64_t)tfd);
    fclose(tfd);
    mca_sensor_mcedata_component.use_progress_thread = false;
    char* saved_logfile = mca_sensor_mcedata_component.logfile;
    mca_sensor_mcedata_component.logfile = (char*)"./test.logfile";

    int rv = orcm_sensor_mcedata_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);

    orcm_sensor_mcedata_module.start(2);
    EXPECT_EQ(0, orcm_sensor_mcedata_get_log_file_pos());
    remove("./test.logfile");
    orcm_sensor_mcedata_module.stop(2);

    orcm_sensor_mcedata_module.finalize();

    mca_sensor_mcedata_component.logfile = saved_logfile;

    mcedata_mocking.opendir_callback = NULL;
    mcedata_mocking.closedir_callback = NULL;
    mcedata_mocking.readdir_callback = NULL;
}

TEST_F(ut_mcedata_tests, mcedata_update_collect_sample_test_negative)
{
    mcedata_mocking.opendir_callback = OpenDir;
    mcedata_mocking.closedir_callback = CloseDir;
    mcedata_mocking.readdir_callback = ReadDir;
    mca_sensor_mcedata_component.test = false;
    mca_sensor_mcedata_component.use_progress_thread = false;
    char* saved_logfile = mca_sensor_mcedata_component.logfile;
    mca_sensor_mcedata_component.logfile = (char*)"./test.logfile";
    mca_sensor_mcedata_component.collect_metrics = true;

    int rv = orcm_sensor_mcedata_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);

    orcm_sensor_mcedata_module.start(2);

    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    orcm_sensor_mcedata_module.sample(&sampler);
    EXPECT_EQ(0x01, mca_sensor_mcedata_component.diagnostics);
    OBJ_DESTRUCT(&sampler);

    orcm_sensor_mcedata_module.stop(2);

    orcm_sensor_mcedata_module.finalize();

    remove("./test.logfile");

    mca_sensor_mcedata_component.test = true;
    mca_sensor_mcedata_component.logfile = saved_logfile;
    mcedata_mocking.opendir_callback = NULL;
    mcedata_mocking.closedir_callback = NULL;
    mcedata_mocking.readdir_callback = NULL;
}

TEST_F(ut_mcedata_tests, mcedata_update_log_file_size_test)
{
    char* logfile = (char*)"./test.logfile";
    char* lines[] = {
        (char*)"DUMMY COMMENT LINE\n",
        (char*)"DUMMY LOG mcelog CPU \n",
        (char*)"DUMMY LOG mcelog BANK \n",
        (char*)"DUMMY LOG mcelog MISC \n",
        (char*)"DUMMY LOG mcelog ADDR \n",
        (char*)"DUMMY LOG mcelog STATUS \n",
        (char*)"DUMMY LOG mcelog MCGSTATUS \n",
        (char*)"DUMMY LOG mcelog TIME \n",
        (char*)"DUMMY LOG mcelog SOCKETID \n",
        (char*)"DUMMY LOG mcelog MCGCAP \n",
        (char*)"\n",
        NULL
    };
    int full_size = 0;
    for(int i = 0; NULL != lines[i]; ++i) {
        full_size += strlen(lines[i]);
    }
    int first_line_size = strlen(lines[0]);
    mcedata_mocking.opendir_callback = OpenDir;
    mcedata_mocking.closedir_callback = CloseDir;
    mcedata_mocking.readdir_callback = ReadDir;
    mca_sensor_mcedata_component.test = false;
    mca_sensor_mcedata_component.use_progress_thread = false;
    char* saved_logfile = mca_sensor_mcedata_component.logfile;
    mca_sensor_mcedata_component.logfile = logfile;
    mca_sensor_mcedata_component.collect_metrics = true;

    FILE* tfd = fopen(logfile, "w");
    ASSERT_NE((uint64_t)0, (uint64_t)tfd);
    fprintf(tfd, lines[0]);
    fclose(tfd);

    int rv = orcm_sensor_mcedata_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);

    orcm_sensor_mcedata_module.start(2);

    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    orcm_sensor_mcedata_module.sample(&sampler);
    EXPECT_EQ(0x01, mca_sensor_mcedata_component.diagnostics);
    EXPECT_EQ(first_line_size, (size_t)orcm_sensor_mcedata_get_log_file_pos());
    OBJ_DESTRUCT(&sampler);

    tfd = fopen(logfile, "w");
    EXPECT_NE((uint64_t)0, (uint64_t)tfd);
    if(NULL != tfd) {
        int i = 0;
        while(NULL != lines[i])
            fprintf(tfd, lines[i++]);
        fclose(tfd);
    }

    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    orcm_sensor_mcedata_module.sample(&sampler);
    EXPECT_EQ(0x01, mca_sensor_mcedata_component.diagnostics);
    EXPECT_EQ(full_size, (size_t)orcm_sensor_mcedata_get_log_file_pos());
    OBJ_DESTRUCT(&sampler);

    orcm_sensor_mcedata_module.stop(2);

    orcm_sensor_mcedata_module.finalize();

    remove(logfile);

    mca_sensor_mcedata_component.test = true;
    mca_sensor_mcedata_component.logfile = saved_logfile;
    mcedata_mocking.opendir_callback = NULL;
    mcedata_mocking.closedir_callback = NULL;
    mcedata_mocking.readdir_callback = NULL;
}

TEST_F(ut_mcedata_tests, mcedata_update_log_file_size_test_2)
{
    mcedata_mocking.opendir_callback = OpenDir;
    mcedata_mocking.closedir_callback = CloseDir;
    mcedata_mocking.readdir_callback = ReadDir;
    mcedata_mocking.fseek_callback = FSeekError;
    mca_sensor_mcedata_component.test = false;
    mca_sensor_mcedata_component.use_progress_thread = false;
    char* saved_logfile = mca_sensor_mcedata_component.logfile;
    mca_sensor_mcedata_component.logfile = (char*)"./test.logfile";
    mca_sensor_mcedata_component.collect_metrics = true;

    FILE* tfd = fopen("./test.logfile", "w");
    ASSERT_NE((uint64_t)0, (uint64_t)tfd);
    fprintf(tfd, "DUMMY LOG LINE\n");
    fclose(tfd);

    int rv = orcm_sensor_mcedata_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);

    orcm_sensor_mcedata_module.start(2);

    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    orcm_sensor_mcedata_module.sample(&sampler);
    EXPECT_EQ(0x01, mca_sensor_mcedata_component.diagnostics);
    EXPECT_EQ(0, (size_t)orcm_sensor_mcedata_get_log_file_pos());
    OBJ_DESTRUCT(&sampler);
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    orcm_sensor_mcedata_module.sample(&sampler);
    EXPECT_EQ(0x01, mca_sensor_mcedata_component.diagnostics);
    EXPECT_EQ(0, (size_t)orcm_sensor_mcedata_get_log_file_pos());
    OBJ_DESTRUCT(&sampler);

    orcm_sensor_mcedata_module.stop(2);

    orcm_sensor_mcedata_module.finalize();

    remove("./test.logfile");

    mca_sensor_mcedata_component.test = true;
    mca_sensor_mcedata_component.logfile = saved_logfile;
    mcedata_mocking.opendir_callback = NULL;
    mcedata_mocking.closedir_callback = NULL;
    mcedata_mocking.readdir_callback = NULL;
    mcedata_mocking.fseek_callback = NULL;
}

TEST_F(ut_mcedata_tests, mcedata_perthread_test)
{
    mcedata_mocking.opendir_callback = OpenDir;
    mcedata_mocking.closedir_callback = CloseDir;
    mcedata_mocking.readdir_callback = ReadDir;

    mca_sensor_mcedata_component.sample_rate = 1;
    mca_sensor_mcedata_component.use_progress_thread = true;
    mca_sensor_mcedata_component.test = true;

    int rv = orcm_sensor_mcedata_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);

    orcm_sensor_mcedata_module.start(2);
    sleep(2);
    mca_sensor_mcedata_component.sample_rate = 2;
    sleep(3);
    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    orcm_sensor_mcedata_module.sample(&sampler);
    OBJ_DESTRUCT(&sampler);
    orcm_sensor_mcedata_module.stop(2);

    orcm_sensor_mcedata_module.finalize();

    mca_sensor_mcedata_component.use_progress_thread = false;
    mca_sensor_mcedata_component.test = false;
    mca_sensor_mcedata_component.sample_rate = 1;

    mcedata_mocking.opendir_callback = NULL;
    mcedata_mocking.closedir_callback = NULL;
    mcedata_mocking.readdir_callback = NULL;
}

TEST_F(ut_mcedata_tests, mcedata_get_sample_rate_test)
{
    mcedata_mocking.opendir_callback = OpenDir;
    mcedata_mocking.closedir_callback = CloseDir;
    mcedata_mocking.readdir_callback = ReadDir;

    mca_sensor_mcedata_component.sample_rate = 1;

    int rv = orcm_sensor_mcedata_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);

    mcedata_get_sample_rate(NULL);
    int rate = -1;
    mcedata_get_sample_rate(&rate);
    EXPECT_EQ(1, rate);

    orcm_sensor_mcedata_module.finalize();

    mcedata_mocking.opendir_callback = NULL;
    mcedata_mocking.closedir_callback = NULL;
    mcedata_mocking.readdir_callback = NULL;
}

TEST_F(ut_mcedata_tests, mcedata_get_mcetype_test)
{
    ASSERT_EQ(e_unknown_error, get_mcetype(0));
    ASSERT_EQ(e_bus_ic_error, get_mcetype(BUS_IC_ERR_MASK));
    ASSERT_EQ(e_cache_error, get_mcetype(CACHE_ERR_MASK));
    ASSERT_EQ(e_mem_ctrl_error, get_mcetype(MEM_CTRL_ERR_MASK));
    ASSERT_EQ(e_tlb_error, get_mcetype(TLB_ERR_MASK));
    ASSERT_EQ(e_gen_cache_error, get_mcetype(GEN_CACHE_ERR_MASK));
}

TEST_F(ut_mcedata_tests, mcedata_decode_test)
{
    uint64_t combos[] = {
        0,
        MCI_UC_MASK,
        MCI_MISCV_MASK,
        MCI_PCC_MASK,
        MCI_S_MASK,
        MCI_AR_MASK,

        MCI_UC_MASK | MCI_MISCV_MASK,
        MCI_UC_MASK | MCI_PCC_MASK,
        MCI_UC_MASK | MCI_S_MASK,
        MCI_UC_MASK | MCI_AR_MASK,
        MCI_MISCV_MASK | MCI_PCC_MASK,
        MCI_MISCV_MASK | MCI_S_MASK,
        MCI_MISCV_MASK | MCI_AR_MASK,
        MCI_PCC_MASK | MCI_S_MASK,
        MCI_PCC_MASK | MCI_AR_MASK,
        MCI_S_MASK | MCI_AR_MASK,

        MCI_UC_MASK | MCI_MISCV_MASK | MCI_PCC_MASK,
        MCI_UC_MASK | MCI_MISCV_MASK | MCI_S_MASK,
        MCI_UC_MASK | MCI_MISCV_MASK | MCI_AR_MASK,
        MCI_UC_MASK | MCI_PCC_MASK | MCI_S_MASK,
        MCI_UC_MASK | MCI_PCC_MASK | MCI_AR_MASK,
        MCI_UC_MASK | MCI_S_MASK | MCI_AR_MASK,
        MCI_MISCV_MASK | MCI_PCC_MASK | MCI_S_MASK,
        MCI_MISCV_MASK | MCI_PCC_MASK | MCI_AR_MASK,
        MCI_PCC_MASK | MCI_S_MASK | MCI_AR_MASK,

        MCI_UC_MASK | MCI_MISCV_MASK | MCI_PCC_MASK | MCI_S_MASK,
        MCI_UC_MASK | MCI_MISCV_MASK | MCI_PCC_MASK | MCI_AR_MASK,
        MCI_MISCV_MASK | MCI_PCC_MASK | MCI_S_MASK | MCI_AR_MASK,

        MCI_UC_MASK | MCI_MISCV_MASK | MCI_PCC_MASK | MCI_S_MASK | MCI_AR_MASK,

        0xffffffffffffffff
    };
    uint64_t mce_reg[5] = { 0, 0, 0, 0, 0 };
    uint64_t test_vals[6] = {
        0,
        BUS_IC_ERR_MASK,
        CACHE_ERR_MASK,
        MEM_CTRL_ERR_MASK,
        TLB_ERR_MASK,
        GEN_CACHE_ERR_MASK
    };
    opal_list_t* data = OBJ_NEW(opal_list_t);

    for(int i = 0; i < 6; ++i) {
        for(int j = 0; 0xffffffffffffffff != combos[j]; ++j) {
            uint64_t extra = ((j % 3) == 0)?MCI_VALID_MASK:0;
            if(0 == j) {
                mce_reg[MCG_CAP] = MCG_TES_P_MASK;
            } else {
                mce_reg[MCG_CAP] = MCG_TES_P_MASK | MCG_CMCI_P_MASK | MCG_SER_P_MASK;
            }
            mce_reg[MCI_STATUS] = test_vals[i] | combos[j] | 0x30 | extra;
            mcedata_decode(mce_reg, data);
        }
        for(int j = 0; 0xffffffffffffffff != combos[j]; ++j) {
            uint64_t extra = ((j % 3) == 0)?MCI_VALID_MASK:0;
            if(0 == j) {
                mce_reg[MCG_CAP] = MCG_TES_P_MASK;
            } else {
                mce_reg[MCG_CAP] = MCG_TES_P_MASK | MCG_CMCI_P_MASK | MCG_SER_P_MASK;
            }
            mce_reg[MCI_STATUS] = test_vals[i] | combos[j] | 0x90 | extra;
            mcedata_decode(mce_reg, data);
        }
    }
    ORCM_RELEASE(data);
}
