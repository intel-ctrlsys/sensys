/*
 * Copyright (c) 2016-2017 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "freq_tests.h"
#include "freq_tests_mocking.h"

// ORTE
#include "orte/runtime/orte_globals.h"

// ORCM
#include "orcm/mca/mca.h"
#include "orcm/mca/sensor/sensor.h"
#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/freq/sensor_freq.h"

// Fixture
#include <queue>
using namespace std;

extern "C" {
    ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
    extern void collect_freq_sample(orcm_sensor_sampler_t *sampler);
    extern int freq_enable_sampling(const char* sensor_specification);
    extern int freq_disable_sampling(const char* sensor_specification);
    extern int freq_reset_sampling(const char* sensor_specification);
    extern void freq_get_units(char* label, char** units);
    extern void freq_ptrk_des(pstate_tracker_t *trk);
    extern void freq_ptrk_con(pstate_tracker_t *trk);
    extern void freq_set_sample_rate(int sample_rate);
    extern void freq_get_sample_rate(int *sample_rate);
};

int counter = 0;
bool dir_name_flag = false;

static queue<bool> opendirQ;
static queue<bool> readdirQ;
static queue<bool> fopenQ;
static queue<bool> fgetsQ;

static const char* fileEntry[] = {
   "1596000",
   (const char*)NULL
};

static const char* entries1[] = {
         ".",
        "..",
      "cpu0",
   "cpufreq",
   (const char*)NULL
};

static const char* entries2[] = {
         ".",
        "..",
      "intel_pstate",
   (const char*)NULL
};

void ut_freq_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();
    freq_mocking.call_real_func = false;
}

void ut_freq_tests::TearDownTestCase()
{
    // Release OPAL level resources
    opal_dss_close();
    // Gtest uses fopen, fclose, etc to save xml log files.
    // need to call real functions after teardown to save results.
    freq_mocking.call_real_func = true;
}

FILE* ut_freq_tests::FOpen(const char * filename, const char * mode)
{
    bool fail = false;
    if(0 < fopenQ.size()) {
        fail = fopenQ.front();
        fopenQ.pop();
     }
    if(true == fail) {
        return NULL;
     }
    else {
        (void)filename;
        int* fi = new int;
        *fi = 0;
        return (FILE*)fi;
    }
}

int ut_freq_tests::FClose(FILE* fd)
{
    if(NULL == fd) {
        return -1;
    } else {
        return 0;
    }
}

char* ut_freq_tests::fgets(char* s, int size, FILE* fd)
{
    const char* lookup = (const char*)fd;
    int* index = (int*)fd;
    bool fail = false;
       if(0 < fgetsQ.size()) {
           fail = fgetsQ.front();
           fgetsQ.pop();
        }
       if(true == fail) {
           return NULL;
        }
       else {
           if(NULL == lookup) {
               return 0;
           }
           if(NULL != fileEntry[*index]) {
               strncpy(s, fileEntry[*index], sizeof(fileEntry[*index]));
               return s;
           }
           else {
            return (char*)-1;
           }
       }
}

DIR* ut_freq_tests::OpenDir(const char* dirname)
{
    bool fail = false;
    if(0 < opendirQ.size()) {
        fail = opendirQ.front();
        opendirQ.pop();
    }
    if(true == fail) {
        return NULL;
    }
    else {
        if (0 == strcmp(dirname, "/sys/devices/system/cpu")) {
            dir_name_flag = true;
        }
        else if(0 == strcmp(dirname, "/sys/devices/system/cpu/intel_pstate")) {
            dir_name_flag = false;
        }
    }
    int* rv = new int;
    *rv = 0;
    return (DIR*)rv;
}

int ut_freq_tests::CloseDir(DIR* dir_fd)
{
    delete (int*)dir_fd;
    return 0;
}

struct dirent* ut_freq_tests::ReadDir(DIR* dir_fd)
{
    static struct dirent dir;
    memset(&dir, 0, sizeof(struct dirent));
    int* index = (int*)dir_fd;
    bool fail = false;
    if(0 < readdirQ.size()) {
        fail = readdirQ.front();
        readdirQ.pop();
    }
    if(true == fail){
        return NULL;
    }
    else {
        if(true == dir_name_flag) {
            if(NULL != entries1[*index]) {
                strcpy(dir.d_name, entries1[*index]);
                ++(*index);
                return &dir;
            }
            else {
                return NULL;
            }
        }
        else {
            if(NULL != entries2[*index]) {
                strcpy(dir.d_name, entries2[*index]);
                ++(*index);
                return &dir;
            }
            else {
                return NULL;
            }
        }
    }
}

// Testing the data collection class
TEST_F(ut_freq_tests, freq_sensor_sample_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("freq", false, false);
    mca_sensor_freq_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    // Tests
    collect_freq_sample(sampler);
    EXPECT_EQ(0, (mca_sensor_freq_component.diagnostics & 0x1));

    orcm_sensor_base_runtime_metrics_set(object, true, "freq");
    collect_freq_sample(sampler);
    EXPECT_EQ(1, (mca_sensor_freq_component.diagnostics & 0x1));

    // Cleanup
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_freq_component.runtime_metrics = NULL;
}

TEST_F(ut_freq_tests, freq_api_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("freq", false, false);
    mca_sensor_freq_component.runtime_metrics = object;

    // Tests
    freq_enable_sampling("freq");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    freq_disable_sampling("freq");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    freq_enable_sampling("freq");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    freq_reset_sampling("freq");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    freq_enable_sampling("not_the_right_datagroup");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    freq_enable_sampling("all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_freq_component.runtime_metrics = NULL;
}

TEST_F(ut_freq_tests, freq_api_tests_2)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("freq", false, false);
    mca_sensor_freq_component.runtime_metrics = object;
    orcm_sensor_base_runtime_metrics_track(object, "core0");
    orcm_sensor_base_runtime_metrics_track(object, "core1");
    orcm_sensor_base_runtime_metrics_track(object, "core2");
    orcm_sensor_base_runtime_metrics_track(object, "core3");

    // Tests
    freq_enable_sampling("freq:core3");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "core1"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "core3"));
    EXPECT_EQ(1, orcm_sensor_base_runtime_metrics_active_label_count(object));
    freq_disable_sampling("freq:core3");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "core3"));
    freq_enable_sampling("freq:core2");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "core2"));
    freq_reset_sampling("freq:core2");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "core2"));
    freq_enable_sampling("freq:core no_core");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, "core2"));
    freq_enable_sampling("freq:all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "core0"));
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, "core3"));
    EXPECT_EQ(4, orcm_sensor_base_runtime_metrics_active_label_count(object));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_freq_component.runtime_metrics = NULL;
}

TEST_F(ut_freq_tests, freq_get_units_tests)
{
    //Setup
    char *units;

    //Tests
    freq_get_units((char*)"num_pstates", &units);
    EXPECT_STREQ("", units);
    freq_get_units((char*)"no_turbo", &units);
    EXPECT_STREQ("", units);
    freq_get_units((char*)"allow_turbo", &units);
    EXPECT_STREQ("", units);
    freq_get_units((char*)"num_cpu_cores", &units);
    EXPECT_STREQ("%", units);
    freq_get_units((char*)"cpu_core", &units);
    EXPECT_STREQ("%", units);
    freq_get_units((char*)"p_cpu_cores", &units);
    EXPECT_STREQ("%", units);
    free(units);
}

TEST_F(ut_freq_tests, freq_set_get_sample_rate_tests)
{
    //Setup
    int sr = 5;
    int *sample_rate = &sr;

    //Tests
    mca_sensor_freq_component.use_progress_thread = true;
    freq_set_sample_rate(10);
    EXPECT_EQ(10, mca_sensor_freq_component.sample_rate);
    freq_get_sample_rate(sample_rate);
    EXPECT_EQ(10, *sample_rate);
    mca_sensor_freq_component.use_progress_thread = false;
    freq_set_sample_rate(7);
    EXPECT_EQ(10, mca_sensor_freq_component.sample_rate);
    freq_get_sample_rate(sample_rate);
    EXPECT_EQ(10, *sample_rate);
    mca_sensor_freq_component.use_progress_thread = true;
    freq_set_sample_rate(15);
    freq_get_sample_rate(NULL);
    EXPECT_EQ(10, *sample_rate);
}

TEST_F(ut_freq_tests, freq_ptrk_con_des_test)
{
    pstate_tracker_t *trk;
    trk = OBJ_NEW(pstate_tracker_t);
    OBJ_RELEASE(trk);
}

TEST_F(ut_freq_tests, freq_init_start_stop_finalize_test)
{
    //Setup
    freq_mocking.fopen_callback = FOpen;
    freq_mocking.fgets_callback = fgets;
    freq_mocking.opendir_callback = OpenDir;
    freq_mocking.closedir_callback = CloseDir;
    freq_mocking.readdir_callback = ReadDir;
    freq_mocking.fclose_callback = FClose;
    mca_sensor_freq_component.use_progress_thread = false;

    //Tests
    int rv = orcm_sensor_freq_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);
    orcm_sensor_freq_module.start(2);
    orcm_sensor_freq_module.stop(2);
    orcm_sensor_freq_module.finalize();

    //Cleanup
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = NULL;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.fclose_callback = NULL;
}

TEST_F(ut_freq_tests, freq_init_start_stop_finalize_test1)
{
    //Setup
    freq_mocking.fopen_callback = FOpen;
    freq_mocking.fgets_callback = fgets;
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = CloseDir;
    freq_mocking.readdir_callback = ReadDir;
    freq_mocking.fclose_callback = FClose;
    mca_sensor_freq_component.use_progress_thread = false;

    //Tests
    int rv = orcm_sensor_freq_module.init();
    EXPECT_EQ(ORCM_ERROR, rv);
    orcm_sensor_freq_module.start(2);
    orcm_sensor_freq_module.stop(2);
    orcm_sensor_freq_module.finalize();

    //Cleanup
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = NULL;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.fclose_callback = NULL;
}

TEST_F(ut_freq_tests, freq_init_start_stop_finalize_test2)
{
    //Setup
    freq_mocking.fopen_callback = FOpen;
    freq_mocking.fgets_callback = fgets;
    freq_mocking.opendir_callback = OpenDir;
    freq_mocking.closedir_callback = CloseDir;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fclose_callback = FClose;
    mca_sensor_freq_component.use_progress_thread = false;

    //Tests
    int rv = orcm_sensor_freq_module.init();
    EXPECT_EQ(ORTE_ERROR, rv);
    orcm_sensor_freq_module.start(2);
    orcm_sensor_freq_module.stop(2);
    orcm_sensor_freq_module.finalize();

    //Cleanup
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = NULL;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.fclose_callback = NULL;
}

TEST_F(ut_freq_tests, freq_init_start_stop_finalize_test3)
{
    //Setup
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = fgets;
    freq_mocking.opendir_callback = OpenDir;
    freq_mocking.closedir_callback = CloseDir;
    freq_mocking.readdir_callback = ReadDir;
    freq_mocking.fclose_callback = FClose;
    mca_sensor_freq_component.use_progress_thread = false;

    //Tests
    int rv = orcm_sensor_freq_module.init();
    EXPECT_EQ(ORCM_ERROR, rv);
    orcm_sensor_freq_module.start(2);
    orcm_sensor_freq_module.stop(2);
    orcm_sensor_freq_module.finalize();

    //Cleanup
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = NULL;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.fclose_callback = NULL;
}

TEST_F(ut_freq_tests, freq_init_start_stop_finalize_test4)
{
    //Setup
    freq_mocking.fopen_callback = FOpen;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.opendir_callback = OpenDir;
    freq_mocking.closedir_callback = CloseDir;
    freq_mocking.readdir_callback = ReadDir;
    freq_mocking.fclose_callback = FClose;
    mca_sensor_freq_component.use_progress_thread = false;

    //Tests
    int rv = orcm_sensor_freq_module.init();
    EXPECT_EQ(ORCM_ERROR, rv);
    orcm_sensor_freq_module.start(2);
    orcm_sensor_freq_module.stop(2);
    orcm_sensor_freq_module.finalize();

    //Cleanup
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = NULL;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.fclose_callback = NULL;
}

TEST_F(ut_freq_tests, freq_init_start_stop_finalize_test5)
{
    //Setup
    fopenQ.push(false);
    fopenQ.push(true);
    freq_mocking.fopen_callback = FOpen;
    freq_mocking.fgets_callback = fgets;
    freq_mocking.opendir_callback = OpenDir;
    freq_mocking.closedir_callback = CloseDir;
    freq_mocking.readdir_callback = ReadDir;
    freq_mocking.fclose_callback = FClose;
    mca_sensor_freq_component.use_progress_thread = false;

    //Tests
    int rv = orcm_sensor_freq_module.init();
    EXPECT_EQ(ORCM_ERROR, rv);
    orcm_sensor_freq_module.start(2);
    orcm_sensor_freq_module.stop(2);
    orcm_sensor_freq_module.finalize();

    //Cleanup
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = NULL;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.fclose_callback = NULL;
}

TEST_F(ut_freq_tests, freq_init_start_stop_finalize_test6)
{
    //Setup
    fgetsQ.push(false);
    fgetsQ.push(true);
    freq_mocking.fopen_callback = FOpen;
    freq_mocking.fgets_callback = fgets;
    freq_mocking.opendir_callback = OpenDir;
    freq_mocking.closedir_callback = CloseDir;
    freq_mocking.readdir_callback = ReadDir;
    freq_mocking.fclose_callback = FClose;
    mca_sensor_freq_component.use_progress_thread = false;

    //Tests
    int rv = orcm_sensor_freq_module.init();
    EXPECT_EQ(ORCM_ERROR, rv);
    orcm_sensor_freq_module.start(2);
    orcm_sensor_freq_module.stop(2);
    orcm_sensor_freq_module.finalize();

    //Cleaup
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = NULL;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.fclose_callback = NULL;
}

TEST_F(ut_freq_tests, freq_init_start_stop_finalize_test7)
{
    //Setup
    mca_sensor_freq_component.pstate = true;
    opendirQ.push(false);
    opendirQ.push(true);
    freq_mocking.opendir_callback = OpenDir;
    freq_mocking.fopen_callback = FOpen;
    freq_mocking.fgets_callback = fgets;
    freq_mocking.closedir_callback = CloseDir;
    freq_mocking.readdir_callback = ReadDir;
    freq_mocking.fclose_callback = FClose;
    mca_sensor_freq_component.use_progress_thread = false;

    //Tests
    int rv = orcm_sensor_freq_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);
    orcm_sensor_freq_module.start(2);
    orcm_sensor_freq_module.stop(2);
    orcm_sensor_freq_module.finalize();

    //Cleanup
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = NULL;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.fclose_callback = NULL;
    mca_sensor_freq_component.pstate = false;
}

TEST_F(ut_freq_tests, freq_init_start_stop_finalize_test8)
{
    //Setup
    mca_sensor_freq_component.pstate = true;
    readdirQ.push(false);
    readdirQ.push(true);
    freq_mocking.opendir_callback = OpenDir;
    freq_mocking.fopen_callback = FOpen;
    freq_mocking.fgets_callback = fgets;
    freq_mocking.closedir_callback = CloseDir;
    freq_mocking.readdir_callback = ReadDir;
    freq_mocking.fclose_callback = FClose;
    mca_sensor_freq_component.use_progress_thread = false;

    //Tests
    int rv = orcm_sensor_freq_module.init();
    EXPECT_EQ(ORCM_ERROR, rv);
    orcm_sensor_freq_module.start(2);
    orcm_sensor_freq_module.stop(2);
    orcm_sensor_freq_module.finalize();

    //Cleaup
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = NULL;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.fclose_callback = NULL;
    mca_sensor_freq_component.pstate = false;
}

TEST_F(ut_freq_tests, freq_init_start_stop_finalize_test9)
{
    //Setup
    mca_sensor_freq_component.pstate = true;
    fopenQ.push(false);
    fopenQ.push(false);
    fopenQ.push(true);
    freq_mocking.opendir_callback = OpenDir;
    freq_mocking.fopen_callback = FOpen;
    freq_mocking.fgets_callback = fgets;
    freq_mocking.closedir_callback = CloseDir;
    freq_mocking.readdir_callback = ReadDir;
    freq_mocking.fclose_callback = FClose;
    mca_sensor_freq_component.use_progress_thread = false;

    //Tests
    int rv = orcm_sensor_freq_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);
    orcm_sensor_freq_module.start(2);
    orcm_sensor_freq_module.stop(2);
    orcm_sensor_freq_module.finalize();

    //Cleanup
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = NULL;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.fclose_callback = NULL;
    mca_sensor_freq_component.pstate = false;
}

TEST_F(ut_freq_tests, freq_init_start_stop_finalize_test10)
{
    //Setup
    mca_sensor_freq_component.pstate = true;
    fgetsQ.push(false);
    fgetsQ.push(false);
    fgetsQ.push(true);
    freq_mocking.opendir_callback = OpenDir;
    freq_mocking.fopen_callback = FOpen;
    freq_mocking.fgets_callback = fgets;
    freq_mocking.closedir_callback = CloseDir;
    freq_mocking.readdir_callback = ReadDir;
    freq_mocking.fclose_callback = FClose;
    mca_sensor_freq_component.use_progress_thread = false;

    //Tests
    int rv = orcm_sensor_freq_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);
    orcm_sensor_freq_module.start(2);
    orcm_sensor_freq_module.stop(2);
    orcm_sensor_freq_module.finalize();

    //Cleanup
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = NULL;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.fclose_callback = NULL;
    mca_sensor_freq_component.pstate = false;
}

TEST_F(ut_freq_tests, freq_init_start_stop_finalize_test11)
{
    //Setup
    freq_mocking.fopen_callback = FOpen;
    freq_mocking.fgets_callback = fgets;
    freq_mocking.opendir_callback = OpenDir;
    freq_mocking.closedir_callback = CloseDir;
    freq_mocking.readdir_callback = ReadDir;
    freq_mocking.fclose_callback = FClose;
    mca_sensor_freq_component.pstate = true;
    mca_sensor_freq_component.use_progress_thread = false;

    //Tests
    int rv = orcm_sensor_freq_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);
    orcm_sensor_freq_module.start(2);
    orcm_sensor_freq_module.stop(2);
    orcm_sensor_freq_module.finalize();

    //Cleanup
    freq_mocking.opendir_callback = NULL;
    freq_mocking.closedir_callback = NULL;
    freq_mocking.readdir_callback = NULL;
    freq_mocking.fopen_callback = NULL;
    freq_mocking.fgets_callback = NULL;
    freq_mocking.fclose_callback = NULL;
}
