/*
 * Copyright (c) 2015-2017  Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "edac_collector_tests.h"

// Code being tested...
#define GTEST_MOCK_TESTING
#include "orcm/mca/sensor/errcounts/sensor_errcounts.h"
#include "orcm/mca/sensor/errcounts/errcounts.h"
#undef GTEST_MOCK_TESTING

// C
#include <memory.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

// C++
#include <iostream>
#include <iomanip>
#include <sstream>

// OPAL
#include "opal/dss/dss.h"

// ORTE
#include "orte/util/proc_info.h"

// ORCM
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"

extern "C" {
    #include "opal/runtime/opal_progress_threads.h"
}

#define SAFE_OBJ_RELEASE(x) if(NULL!=x) { OBJ_RELEASE(x); x=NULL; }

#define NOOP_JOBID -999

// Additional gtest asserts...
#define ASSERT_PTREQ(x,y)  ASSERT_EQ((void*)x,(void*)y)
#define ASSERT_PTRNE(x,y)  ASSERT_NE((void*)x,(void*)y)
#define ASSERT_NULL(x)     ASSERT_PTREQ(NULL,x)
#define ASSERT_NOT_NULL(x) ASSERT_PTRNE(NULL,x)
#define EXPECT_PTREQ(x,y)  EXPECT_EQ((void*)x,(void*)y)
#define EXPECT_PTRNE(x,y)  EXPECT_NE((void*)x,(void*)y)
#define EXPECT_NULL(x)     EXPECT_PTREQ(NULL,x)
#define EXPECT_NOT_NULL(x) EXPECT_PTRNE(NULL,x)

// Fixture
using namespace std;

const char* ut_edac_collector_tests::hostname_ = "test_host";
const char* ut_edac_collector_tests::plugin_name_ = "errcounts";
const char* ut_edac_collector_tests::proc_name_ = "errcounts_tests";
int ut_edac_collector_tests::last_errno_ = 0;
string ut_edac_collector_tests::last_error_filename_;
int ut_edac_collector_tests::last_orte_error_ = ORCM_SUCCESS;
int ut_edac_collector_tests::fail_pack_on_ = -1;
int ut_edac_collector_tests::fail_pack_count_ = 0;
int ut_edac_collector_tests::fail_unpack_on_ = -1;
int ut_edac_collector_tests::fail_unpack_count_ = 0;
bool ut_edac_collector_tests::fail_pack_buffer_ = false;
bool ut_edac_collector_tests::fail_unpack_buffer_ = false;
char* ut_edac_collector_tests::saved_edac_folder_ = NULL;

map<string,int> ut_edac_collector_tests::logged_data_;
map<string,int> ut_edac_collector_tests::golden_data_;
map<string,string> ut_edac_collector_tests::logged_inv_;
map<string,string> ut_edac_collector_tests::golden_inv_;
vector<string> ut_edac_collector_tests::opal_output_verbose_;
queue<int32_t> ut_edac_collector_tests::packed_int32_;
queue<string> ut_edac_collector_tests::packed_str_;
queue<struct timeval> ut_edac_collector_tests::packed_ts_;
std::vector<orcm_analytics_value_t*> ut_edac_collector_tests::current_analytics_values_;
map<string,string> ut_edac_collector_tests::database_data_;

extern "C" {
    extern errcounts_impl* implementation;
    extern orcm_sensor_errcounts_component_t mca_sensor_errcounts_component;
    extern orte_proc_info_t orte_process_info;

    extern int orcm_sensor_errcounts_open(void);
    extern int orcm_sensor_errcounts_close(void);
    extern int orcm_sensor_errcounts_query(mca_base_module_t **module, int *priority);
    extern int errcounts_component_register(void);
}

static void mkfile(const char* filename, const char* contents)
{
    FILE* fd = fopen(filename, "w");
    fwrite(contents, 1, strlen(contents), fd);
    fclose(fd);
}

class edac_collector_mock_getline: public edac_collector
{
    public:
    edac_collector_mock_getline(edac_error_callback_fn_t error_cb = NULL,
                          const char* edac_path = NULL)
            : edac_collector(error_cb, edac_path) {};
        virtual ~edac_collector_mock_getline() {};

    protected:
        ssize_t GetLine(char** lineptr, size_t* n, FILE* stream) const
        {
            (void)lineptr;
            (void)n;
            (void)stream;
            errno = EBADF;
            return -1;
        };
};

class edac_collector_mock_stat: public edac_collector
{
public:
    edac_collector_mock_stat(edac_error_callback_fn_t error_cb = NULL,
                          const char* edac_path = NULL)
            : edac_collector(error_cb, edac_path) {};
    virtual ~edac_collector_mock_stat() {};

protected:
    int Stat(const char* path, struct stat* info) const
    {
        (void)path;
        (void)info;
        return -1;
    }
};

class edac_collector_mock_fopen: public edac_collector
{
public:
    edac_collector_mock_fopen(edac_error_callback_fn_t error_cb = NULL,
                          const char* edac_path = NULL)
            : edac_collector(error_cb, edac_path) {};
    virtual ~edac_collector_mock_fopen() {};

protected:
    FILE* FOpen(const char* path, const char* mode) const
    {
        (void)path;
        (void)mode;
        errno = ENOENT;
        return NULL;
    };
};


void ut_edac_collector_tests::BuildMcFileSys()
{
    mkfile("./mcfile", "");
    mkdir("./mc", 0755);

    mkdir("./mc/mc0", 0755);
    mkdir("./mc/mc0/csrow0", 0755);
    mkfile("./mc/mc0/csrow0/ce_count", "2");
    mkfile("./mc/mc0/csrow0/ue_count", "1");
    mkfile("./mc/mc0/csrow0/ch0_dimm_label", "TEST_MC0_CSROW0_CH0\n");
    mkfile("./mc/mc0/csrow0/ch0_ce_count", "0");
    mkfile("./mc/mc0/csrow0/ch1_dimm_label", "TEST_MC0_CSROW0_CH1\n");
    mkfile("./mc/mc0/csrow0/ch1_ce_count", "1");
    mkfile("./mc/mc0/csrow0/ch2_dimm_label", "TEST_MC0_CSROW0_CH2\n");
    mkfile("./mc/mc0/csrow0/ch2_ce_count", "1");
    mkfile("./mc/mc0/csrow0/ch3_dimm_label", "TEST_MC0_CSROW0_CH3\n");
    mkfile("./mc/mc0/csrow0/ch3_ce_count", "0");

    mkdir("./mc/mc1", 0755);
    mkdir("./mc/mc1/csrow0", 0755);
    mkfile("./mc/mc1/csrow0/ce_count", "6");
    mkfile("./mc/mc1/csrow0/ue_count", "2");
    mkfile("./mc/mc1/csrow0/ch0_dimm_label", "TEST_MC1_CSROW0_CH0\n");
    mkfile("./mc/mc1/csrow0/ch0_ce_count", "1");
    mkfile("./mc/mc1/csrow0/ch1_dimm_label", "TEST_MC1_CSROW0_CH1\n");
    mkfile("./mc/mc1/csrow0/ch1_ce_count", "2");
    mkfile("./mc/mc1/csrow0/ch2_dimm_label", "TEST_MC1_CSROW0_CH2\n");
    mkfile("./mc/mc1/csrow0/ch2_ce_count", "1");
    mkfile("./mc/mc1/csrow0/ch3_dimm_label", "TEST_MC1_CSROW0_CH3\n");
    mkfile("./mc/mc1/csrow0/ch3_ce_count", "2");
}

void ut_edac_collector_tests::DestroyMcFileSys()
{
    remove("./mc/mc1/csrow0/ch3_ce_count");
    remove("./mc/mc1/csrow0/ch3_dimm_label");
    remove("./mc/mc1/csrow0/ch2_ce_count");
    remove("./mc/mc1/csrow0/ch2_dimm_label");
    remove("./mc/mc1/csrow0/ch1_ce_count");
    remove("./mc/mc1/csrow0/ch1_dimm_label");
    remove("./mc/mc1/csrow0/ch0_ce_count");
    remove("./mc/mc1/csrow0/ch0_dimm_label");
    remove("./mc/mc1/csrow0/ue_count");
    remove("./mc/mc1/csrow0/ce_count");
    remove("./mc/mc1/csrow0");
    remove("./mc/mc1");

    remove("./mc/mc0/csrow0/ch3_ce_count");
    remove("./mc/mc0/csrow0/ch3_dimm_label");
    remove("./mc/mc0/csrow0/ch2_ce_count");
    remove("./mc/mc0/csrow0/ch2_dimm_label");
    remove("./mc/mc0/csrow0/ch1_ce_count");
    remove("./mc/mc0/csrow0/ch1_dimm_label");
    remove("./mc/mc0/csrow0/ch0_ce_count");
    remove("./mc/mc0/csrow0/ch0_dimm_label");
    remove("./mc/mc0/csrow0/ue_count");
    remove("./mc/mc0/csrow0/ce_count");
    remove("./mc/mc0/csrow0");
    remove("./mc/mc0");

    remove("./mc");
    remove("./mcfile");
}

void ut_edac_collector_tests::SetUpTestCase()
{
    // Apparently never configured in OPAL to a real value only -1...
    opal_dss_register_vars();
    saved_edac_folder_ = mca_sensor_errcounts_component.edac_mc_folder;
    mca_sensor_errcounts_component.edac_mc_folder = (char*)"./mc";
    
    // Base of sysfs (mocked)
    BuildMcFileSys();

    // Golden Data Comparer
    golden_data_["CPU_SrcID#0_Sum_DIMM#0_CE"] = 2;
    golden_data_["CPU_SrcID#0_Sum_DIMM#0_UE"] = 1;
    golden_data_["TEST_MC0_CSROW0_CH0_CE"] = 0;
    golden_data_["TEST_MC0_CSROW0_CH1_CE"] = 1;
    golden_data_["TEST_MC0_CSROW0_CH2_CE"] = 1;
    golden_data_["TEST_MC0_CSROW0_CH3_CE"] = 0;

    golden_data_["CPU_SrcID#1_Sum_DIMM#0_CE"] = 6;
    golden_data_["CPU_SrcID#1_Sum_DIMM#0_UE"] = 2;
    golden_data_["TEST_MC1_CSROW0_CH0_CE"] = 1;
    golden_data_["TEST_MC1_CSROW0_CH1_CE"] = 2;
    golden_data_["TEST_MC1_CSROW0_CH2_CE"] = 1;
    golden_data_["TEST_MC1_CSROW0_CH3_CE"] = 2;

    // Golden Invetory Comparer
    golden_inv_["sensor_errcounts_1"] = "CPU_SrcID#0_Sum_DIMM#0_CE";
    golden_inv_["sensor_errcounts_2"] = "CPU_SrcID#0_Sum_DIMM#0_UE";
    golden_inv_["sensor_errcounts_3"] = "TEST_MC0_CSROW0_CH0_CE";
    golden_inv_["sensor_errcounts_4"] = "TEST_MC0_CSROW0_CH1_CE";
    golden_inv_["sensor_errcounts_5"] = "TEST_MC0_CSROW0_CH2_CE";
    golden_inv_["sensor_errcounts_6"] = "TEST_MC0_CSROW0_CH3_CE";

    golden_inv_["sensor_errcounts_7"] = "CPU_SrcID#1_Sum_DIMM#0_CE";
    golden_inv_["sensor_errcounts_8"] = "CPU_SrcID#1_Sum_DIMM#0_UE";
    golden_inv_["sensor_errcounts_9"] = "TEST_MC1_CSROW0_CH0_CE";
    golden_inv_["sensor_errcounts_10"] = "TEST_MC1_CSROW0_CH1_CE";
    golden_inv_["sensor_errcounts_11"] = "TEST_MC1_CSROW0_CH2_CE";
    golden_inv_["sensor_errcounts_12"] = "TEST_MC1_CSROW0_CH3_CE";
}

void ut_edac_collector_tests::TearDownTestCase()
{
    golden_data_.clear();
    golden_inv_.clear();
    
    mca_sensor_errcounts_component.edac_mc_folder = saved_edac_folder_;

    DestroyMcFileSys();
}

void ut_edac_collector_tests::SetUp()
{
    ResetTestEnvironment();
}

void ut_edac_collector_tests::TearDown()
{
    edac_mocking.orte_errmgr_base_log_callback = NULL;
    edac_mocking.opal_output_verbose_callback = NULL;
    edac_mocking.orte_util_print_name_args_callback = NULL;
    edac_mocking.opal_dss_pack_callback = NULL;
    edac_mocking.opal_dss_unpack_callback = NULL;
    edac_mocking.orcm_analytics_base_send_data_callback = NULL;
    edac_mocking.opal_progress_thread_init_callback = NULL;
    edac_mocking.opal_progress_thread_finalize_callback = NULL;
}
void ut_edac_collector_tests::ClearBuffers()
{
    while(0 < packed_int32_.size()) {
        packed_int32_.pop();
    }
    while(0 < packed_ts_.size()) {
        packed_ts_.pop();
    }
    while(0 < packed_str_.size()) {
        packed_str_.pop();
    }
}

void ut_edac_collector_tests::ResetTestEnvironment()
{
    logged_data_.clear();
    logged_inv_.clear();
    opal_output_verbose_.clear();
    database_data_.clear();
    ClearBuffers();

    edac_mocking.orte_errmgr_base_log_callback = OrteErrmgrBaseLog;
    edac_mocking.opal_output_verbose_callback = OpalOutputVerbose;
    edac_mocking.orte_util_print_name_args_callback = OrteUtilPrintNameArgs;
    edac_mocking.opal_dss_pack_callback = OpalDssPack;
    edac_mocking.opal_dss_unpack_callback = OpalDssUnpack;
    edac_mocking.orcm_analytics_base_send_data_callback = OrcmAnalyticsBaseSendData;
    edac_mocking.opal_progress_thread_init_callback = NULL;
    edac_mocking.opal_progress_thread_finalize_callback = NULL;

    orte_process_info.nodename = (char*)hostname_;
    orcm_sensor_base.host_tag_value = (char*)hostname_;

    errcounts_finalize_relay();

    last_orte_error_ = ORCM_SUCCESS;
    fail_pack_on_ = -1;
    fail_pack_count_ = 0;
    fail_unpack_on_ = -1;
    fail_unpack_count_ = 0;
    fail_pack_buffer_ = false;
    fail_unpack_buffer_ = false;

    mca_sensor_errcounts_component.use_progress_thread = false;
    mca_sensor_errcounts_component.sample_rate = 0;
    orcm_sensor_base.collect_metrics = true;
    mca_sensor_errcounts_component.collect_metrics = true;

    for(int i = 0; i < current_analytics_values_.size(); ++i) {
        SAFE_OBJ_RELEASE(current_analytics_values_[i]);
    }
    current_analytics_values_.clear();
}

// Mocking methods
void ut_edac_collector_tests::OrteErrmgrBaseLog(int err, char* file, int lineno)
{
    (void)file;
    (void)lineno;
    last_orte_error_ = err;
}

void ut_edac_collector_tests::OpalOutputVerbose(int level, int output_id, const char* line)
{
    opal_output_verbose_.push_back(string(line));
}

char* ut_edac_collector_tests::OrteUtilPrintNameArgs(const orte_process_name_t *name)
{
    (void)name;
    return (char*)proc_name_;
}

int ut_edac_collector_tests::OpalDssPack(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type)
{
    (void)buffer;
    if(NULL == src) {
        return ORCM_ERR_PACK_FAILURE;
    }
    if(fail_pack_on_ == (++fail_pack_count_)) {
        return ORCM_ERR_PACK_FAILURE;
    }
    if(1 != num_vals) {
        return ORCM_ERR_PACK_FAILURE;
    }
    if(OPAL_BUFFER != type) {
        packed_int32_.push(num_vals);
    }
    switch(type) {
    case OPAL_INT32:
        {
            const int32_t* array = (const int32_t*)src;
            packed_int32_.push(*array);
        }
        break;
    case OPAL_STRING:
        {
            const char* str = (const char*)*((void**)src);
            packed_str_.push(str);
        }
        break;
    case OPAL_TIMEVAL:
        {
            const struct timeval* tv = (const struct timeval*)src;
            packed_ts_.push(*tv);
        }
        break;
    case OPAL_BUFFER:
        if(true == fail_pack_buffer_) {
            return ORCM_ERR_PACK_FAILURE;
        }
        break;
    }
    return OPAL_SUCCESS;
}

int ut_edac_collector_tests::OpalDssUnpack(opal_buffer_t* buffer, void* dst, int32_t* num_vals, opal_data_type_t type)
{
    (void)buffer;
    if(NULL == dst || NULL == num_vals) {
        return ORCM_ERR_UNPACK_FAILURE;
    }
    if(fail_unpack_on_ == (++fail_unpack_count_)) {
        return ORCM_ERR_UNPACK_FAILURE;
    }
    if(OPAL_BUFFER != type) {
        *num_vals = packed_int32_.front();
        packed_int32_.pop();
    }
    if(1 != *num_vals) {
        return ORCM_ERR_UNPACK_FAILURE;
    }
    switch(type) {
    case OPAL_INT32:
        {
            int32_t* array = (int32_t*)dst;
            *array = packed_int32_.front();
            packed_int32_.pop();
        }
        break;
    case OPAL_STRING:
        {
            void** ddst = (void**)dst;
            string val = packed_str_.front();
            packed_str_.pop();
            char* str = strdup(val.c_str());
            *ddst = (void*)str;
        }
        break;
    case OPAL_TIMEVAL:
        {
            struct timeval* tv = (struct timeval*)dst;
            *tv = packed_ts_.front();
            packed_ts_.pop();
        }
        break;
    case OPAL_BUFFER:
        if(true == fail_unpack_buffer_) {
            return ORCM_ERR_UNPACK_FAILURE;
        }
        break;
    }

    return OPAL_SUCCESS;
}

void ut_edac_collector_tests::OrcmAnalyticsBaseSendData(orcm_analytics_value_t* data)
{
    if(NULL != data) {
        OBJ_RETAIN(data);
        current_analytics_values_.push_back(data);
    }
}

void ut_edac_collector_tests::MyDbStoreNew(int dbhandle, orcm_db_data_type_t data_type, opal_list_t *input,
                                           opal_list_t *ret, orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    orcm_value_t* item;
    OPAL_LIST_FOREACH(item, input, orcm_value_t) {
        if(OPAL_STRING == item->value.type) {
            string name = item->value.key;
            string value = item->value.data.string;
            database_data_[name] = value;
        }
    }
    if(NULL != cbfunc) {
        cbfunc(dbhandle, ORCM_SUCCESS, input, NULL, NULL);
    }
}


// Callbacks...
void ut_edac_collector_tests::ErrorSink(const char* pathname, int error_number, void* user_data)
{
    last_error_filename_ = pathname;
    last_errno_ = error_number;
}

void ut_edac_collector_tests::DataSink(const char* label, int count, void* user_data)
{
    logged_data_[label] = count;
}

void ut_edac_collector_tests::InventorySink(const char* label, const char* name, void* user_data)
{
    logged_inv_[label] = name;
}

// Testing the data collection class
TEST_F(ut_edac_collector_tests, test_constructor_error_callback)
{
    edac_collector* collector = new edac_collector(NULL, "./mc");
    last_error_filename_ = string();
    last_errno_ = 0;
    collector->report_error("/somepath", 1);
    ASSERT_TRUE(last_error_filename_.empty());
    ASSERT_EQ(0, last_errno_);

    collector->error_callback = ErrorSink;
    collector->report_error("/somepath", 1);

    ASSERT_STREQ("/somepath", last_error_filename_.c_str());
    ASSERT_EQ(1, last_errno_);
}

TEST_F(ut_edac_collector_tests, test_have_edac)
{
    edac_collector* collector = new edac_collector(NULL, "./not_mc");
    ASSERT_FALSE(collector->have_edac());
    delete collector;

    collector = new edac_collector(NULL, "./mc");
    ASSERT_TRUE(collector->have_edac());
    delete collector;

    collector = new edac_collector(NULL, "./mcfile");
    ASSERT_FALSE(collector->have_edac());
    delete collector;
}

TEST_F(ut_edac_collector_tests, test_get_mc_folder_count)
{
    edac_collector* collector = new edac_collector(NULL, "./mc");
    ASSERT_EQ(2, collector->get_mc_folder_count());
    delete collector;
}

TEST_F(ut_edac_collector_tests, test_get_csrow_folder_count)
{
    edac_collector* collector = new edac_collector(ErrorSink, "./mc");

    ASSERT_EQ(1, collector->get_csrow_folder_count(0));
    ASSERT_EQ(1, collector->get_csrow_folder_count(1));
    delete collector;
}

TEST_F(ut_edac_collector_tests, test_get_channel_folder_count)
{
    edac_collector* collector = new edac_collector(NULL, "./mc");
    ASSERT_EQ(4, collector->get_channel_folder_count(0, 0));
    ASSERT_EQ(4, collector->get_channel_folder_count(1, 0));
    delete collector;
}

TEST_F(ut_edac_collector_tests, test_get_ce_count)
{
    edac_collector* collector = new edac_collector(ErrorSink, "./mc");

    ASSERT_EQ(2, collector->get_ce_count(0, 0));
    ASSERT_EQ(6, collector->get_ce_count(1, 0));
    delete collector;
    collector = new edac_collector_mock_fopen(ErrorSink, "./mc");

    ASSERT_NE(2, collector->get_ce_count(0, 0));
    ASSERT_NE(6, collector->get_ce_count(1, 0));
    delete collector;
}

TEST_F(ut_edac_collector_tests, test_get_ue_count)
{
    edac_collector* collector = new edac_collector(DataSink, "./mc");

    ASSERT_EQ(1, collector->get_ue_count(0, 0));
    ASSERT_EQ(2, collector->get_ue_count(1, 0));

    delete collector;
    collector = new edac_collector_mock_fopen(DataSink, "./mc");

    ASSERT_NE(1, collector->get_ue_count(0, 0));
    ASSERT_NE(2, collector->get_ue_count(1, 0));
    delete collector;
}

TEST_F(ut_edac_collector_tests, test_get_channel_ce_count)
{
    edac_collector* collector = new edac_collector(ErrorSink, "./mc");

    ASSERT_EQ(0, collector->get_channel_ce_count(0, 0, 0));
    ASSERT_EQ(1, collector->get_channel_ce_count(0, 0, 1));
    ASSERT_EQ(1, collector->get_channel_ce_count(0, 0, 2));
    ASSERT_EQ(0, collector->get_channel_ce_count(0, 0, 3));

    ASSERT_EQ(1, collector->get_channel_ce_count(1, 0, 0));
    ASSERT_EQ(2, collector->get_channel_ce_count(1, 0, 1));
    ASSERT_EQ(1, collector->get_channel_ce_count(1, 0, 2));
    ASSERT_EQ(2, collector->get_channel_ce_count(1, 0, 3));
    delete collector;
    collector = new edac_collector_mock_fopen(ErrorSink, "./mc");

    ASSERT_NE(0, collector->get_channel_ce_count(0, 0, 0));
    ASSERT_NE(1, collector->get_channel_ce_count(0, 0, 1));
    ASSERT_NE(1, collector->get_channel_ce_count(0, 0, 2));
    ASSERT_NE(0, collector->get_channel_ce_count(0, 0, 3));

    ASSERT_NE(1, collector->get_channel_ce_count(1, 0, 0));
    ASSERT_NE(2, collector->get_channel_ce_count(1, 0, 1));
    ASSERT_NE(1, collector->get_channel_ce_count(1, 0, 2));
    ASSERT_NE(2, collector->get_channel_ce_count(1, 0, 3));
    delete collector;
}

TEST_F(ut_edac_collector_tests, test_get_channel_label)
{
    edac_collector* collector = new edac_collector(ErrorSink, "./mc");

    ASSERT_STREQ("TEST_MC0_CSROW0_CH0", collector->get_channel_label(0, 0, 0).c_str());
    ASSERT_STREQ("TEST_MC0_CSROW0_CH1", collector->get_channel_label(0, 0, 1).c_str());
    ASSERT_STREQ("TEST_MC0_CSROW0_CH2", collector->get_channel_label(0, 0, 2).c_str());
    ASSERT_STREQ("TEST_MC0_CSROW0_CH3", collector->get_channel_label(0, 0, 3).c_str());

    ASSERT_STREQ("TEST_MC1_CSROW0_CH0", collector->get_channel_label(1, 0, 0).c_str());
    ASSERT_STREQ("TEST_MC1_CSROW0_CH1", collector->get_channel_label(1, 0, 1).c_str());
    ASSERT_STREQ("TEST_MC1_CSROW0_CH2", collector->get_channel_label(1, 0, 2).c_str());
    ASSERT_STREQ("TEST_MC1_CSROW0_CH3", collector->get_channel_label(1, 0, 3).c_str());

    delete collector;
    collector = new edac_collector_mock_fopen(ErrorSink, "./mc");

    ASSERT_STRNE("TEST_MC0_CSROW0_CH0", collector->get_channel_label(0, 0, 0).c_str());
    ASSERT_STRNE("TEST_MC0_CSROW0_CH1", collector->get_channel_label(0, 0, 1).c_str());
    ASSERT_STRNE("TEST_MC0_CSROW0_CH2", collector->get_channel_label(0, 0, 2).c_str());
    ASSERT_STRNE("TEST_MC0_CSROW0_CH3", collector->get_channel_label(0, 0, 3).c_str());

    ASSERT_STRNE("TEST_MC1_CSROW0_CH0", collector->get_channel_label(1, 0, 0).c_str());
    ASSERT_STRNE("TEST_MC1_CSROW0_CH1", collector->get_channel_label(1, 0, 1).c_str());
    ASSERT_STRNE("TEST_MC1_CSROW0_CH2", collector->get_channel_label(1, 0, 2).c_str());
    ASSERT_STRNE("TEST_MC1_CSROW0_CH3", collector->get_channel_label(1, 0, 3).c_str());
    delete collector;
}

TEST_F(ut_edac_collector_tests, test_get_xx_negative)
{
    edac_collector* collector = new edac_collector_mock_fopen(ErrorSink, "./mc");

    ASSERT_STREQ("", collector->get_channel_label(0, 0, 0).c_str());
    ASSERT_EQ(2, last_errno_);
    ASSERT_STREQ("./mc/mc0/csrow0/ch0_dimm_label", last_error_filename_.c_str());
    ASSERT_EQ(-1, collector->get_channel_ce_count(0, 0, 1));
    ASSERT_EQ(2, last_errno_);
    ASSERT_STREQ("./mc/mc0/csrow0/ch1_ce_count", last_error_filename_.c_str());

    delete collector;
    collector = new edac_collector_mock_getline(ErrorSink, "./mc");

    ASSERT_STREQ("", collector->get_channel_label(0, 0, 0).c_str());
    ASSERT_EQ(9, last_errno_);
    ASSERT_STREQ("./mc/mc0/csrow0/ch0_dimm_label", last_error_filename_.c_str());
    ASSERT_EQ(-1, collector->get_channel_ce_count(0, 0, 1));
    ASSERT_EQ(9, last_errno_);
    ASSERT_STREQ("./mc/mc0/csrow0/ch1_ce_count", last_error_filename_.c_str());
    delete collector;
}

TEST_F(ut_edac_collector_tests, test_collect_data)
{
    edac_collector* collector = new edac_collector(ErrorSink, "./mc");

    ASSERT_TRUE(collector->collect_data(DataSink, NULL));
    ASSERT_EQ(12, logged_data_.size());

    // Compare to golden...
    map<string,int>::iterator log_it = logged_data_.begin();
    for(map<string,int>::iterator it = golden_data_.begin(); it != golden_data_.end(); ++it) {
        ASSERT_STREQ(it->first.c_str(), log_it->first.c_str()) << it->first << "==" << log_it->first;
        ASSERT_EQ(it->second, log_it->second) << it->second << "==" << log_it->second;
        ++log_it;
    }
    delete collector;
    collector = new edac_collector_mock_getline(ErrorSink, "./mc");

    logged_data_.clear();
    ASSERT_TRUE(collector->collect_data(DataSink, NULL));
    ASSERT_EQ(0, logged_data_.size());

    delete collector;
    collector = new edac_collector_mock_fopen(ErrorSink, "./mc");

    ASSERT_TRUE(collector->collect_data(DataSink, NULL));
    ASSERT_EQ(0, logged_data_.size());

    ASSERT_FALSE(collector->collect_data(NULL, NULL));
    delete collector;
}

TEST_F(ut_edac_collector_tests, test_collect_inventory)
{
    edac_collector* collector = new edac_collector(ErrorSink, "./mc");
    ASSERT_TRUE(collector->collect_inventory(InventorySink, NULL));
    ASSERT_EQ(12, logged_inv_.size());

    map<string,string>::iterator log_it = logged_inv_.begin();
    for(map<string,string>::iterator it = golden_inv_.begin(); it != golden_inv_.end(); ++it) {
        ASSERT_STREQ(it->first.c_str(), log_it->first.c_str()) << it->first << "==" << log_it->first;
        ASSERT_STREQ(it->second.c_str(), log_it->second.c_str()) << it->second << "==" << log_it->second;
        ++log_it;
    }
    delete collector;
    collector = new edac_collector_mock_stat(ErrorSink, "./mc");

    logged_inv_.clear();
    ASSERT_TRUE(collector->collect_inventory(InventorySink, NULL));
    ASSERT_EQ(0, logged_inv_.size());

    ASSERT_FALSE(collector->collect_inventory(NULL, NULL));
    delete collector;
}

// Testing plugin relay methods...
TEST_F(ut_edac_collector_tests, test_init_finalize_relays)
{
    // Open plugin: Positive
    ASSERT_EQ(ORCM_SUCCESS, errcounts_init_relay());

    // Open plugin: Negative (can't init more than once)
    ASSERT_EQ(ORCM_ERROR, errcounts_init_relay());

    // Close plugin...
    errcounts_finalize_relay();

    // For testing... it should ignore this call and return.
    errcounts_finalize_relay();
}

TEST_F(ut_edac_collector_tests, test_start_stop_relays)
{
    // Open plugin
    ASSERT_EQ(ORCM_SUCCESS, errcounts_init_relay());

    // Test start: Positive
    errcounts_start_relay((orte_jobid_t)NOOP_JOBID); // NOOP ID
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    // Test stop: Positive
    errcounts_stop_relay((orte_jobid_t)NOOP_JOBID); // NOOP ID
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    // Close plugin...
    errcounts_finalize_relay();

    // Test start: Negtive (no instance)
    errcounts_start_relay((orte_jobid_t)0);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);

    errcounts_stop_relay((orte_jobid_t)0);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_edac_collector_tests, test_sample_relay)
{
    // Open plugin
    ASSERT_EQ(ORCM_SUCCESS, errcounts_init_relay());

    errcounts_sample_relay(NULL);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    // Close plugin...
    errcounts_finalize_relay();

    errcounts_sample_relay(NULL);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_edac_collector_tests, test_log_relay)
{
    // Open plugin
    ASSERT_EQ(ORCM_SUCCESS, errcounts_init_relay());

    errcounts_log_relay(NULL);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    // Close plugin...
    errcounts_finalize_relay();

    errcounts_log_relay(NULL);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_edac_collector_tests, test_set_sample_rate_relay)
{
    // Open plugin
    ASSERT_EQ(ORCM_SUCCESS, errcounts_init_relay());

    errcounts_set_sample_rate_relay(1);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    // Close plugin...
    errcounts_finalize_relay();

    errcounts_set_sample_rate_relay(1);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_edac_collector_tests, test_enable_relay)
{
    // Open plugin
    ASSERT_EQ(ORCM_SUCCESS, errcounts_init_relay());

    errcounts_enable_sampling_relay("test");
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    // Close plugin...
    errcounts_finalize_relay();

    errcounts_enable_sampling_relay("test");
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_edac_collector_tests, test_disable_relay)
{
    // Open plugin
    ASSERT_EQ(ORCM_SUCCESS, errcounts_init_relay());

    errcounts_disable_sampling_relay("test");
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    // Close plugin...
    errcounts_finalize_relay();

    errcounts_disable_sampling_relay("test");
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_edac_collector_tests, test_reset_relay)
{
    // Open plugin
    ASSERT_EQ(ORCM_SUCCESS, errcounts_init_relay());

    errcounts_reset_sampling_relay("test");
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    // Close plugin...
    errcounts_finalize_relay();

    errcounts_reset_sampling_relay("test");
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_edac_collector_tests, test_get_sample_rate_relay)
{
    // Open plugin
    ASSERT_EQ(ORCM_SUCCESS, errcounts_init_relay());

    errcounts_get_sample_rate_relay(NULL);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    // Close plugin...
    errcounts_finalize_relay();

    errcounts_get_sample_rate_relay(NULL);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_edac_collector_tests, test_inventory_collect_relay)
{
    // Open plugin
    ASSERT_EQ(ORCM_SUCCESS, errcounts_init_relay());

    errcounts_inventory_collect_relay(NULL);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    // Close plugin...
    errcounts_finalize_relay();

    errcounts_inventory_collect_relay(NULL);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_edac_collector_tests, test_inventory_log_relay)
{
    // Open plugin
    ASSERT_EQ(ORCM_SUCCESS, errcounts_init_relay());

    errcounts_inventory_log_relay((char*)"testhostname", NULL);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    errcounts_inventory_log_relay(NULL, NULL);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    // Close plugin...
    errcounts_finalize_relay();

    errcounts_inventory_log_relay(NULL, NULL);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

// Testing errcounts_impl class
TEST_F(ut_edac_collector_tests, test_errcounts_callback_relays)
{
    errcounts_impl dummy;

    // Positive
    errcounts_impl::error_callback_relay(strdup("somepath"), 0, NULL);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    // Negative
    last_orte_error_ = ORCM_SUCCESS;
    errcounts_impl::error_callback_relay(NULL, 0, &dummy);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    // Positive
    last_orte_error_ = ORCM_SUCCESS;
    errcounts_impl::data_callback_relay(strdup("somelabel"), 0, &dummy);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    // Negative
    last_orte_error_ = ORCM_SUCCESS;
    errcounts_impl::data_callback_relay(NULL, 0, &dummy);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    // Positive
    last_orte_error_ = ORCM_SUCCESS;
    errcounts_impl::inventory_callback_relay(strdup("somelabel"), strdup("somename"), NULL);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    // Negative
    last_orte_error_ = ORCM_SUCCESS;
    errcounts_impl::inventory_callback_relay(strdup("somelabel"), NULL, &dummy);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    // Negative
    last_orte_error_ = ORCM_SUCCESS;
    errcounts_impl::inventory_callback_relay(NULL, strdup("somename"), &dummy);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    // Positive
    last_orte_error_ = ORCM_SUCCESS;
    errcounts_impl::perthread_errcounts_sample_relay(0, 0, NULL);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);
}

TEST_F(ut_edac_collector_tests, test_errcounts_construction_and_init_finalize)
{
    errcounts_impl dummy;

    ASSERT_NOT_NULL(dummy.collector_);
    ASSERT_NULL(dummy.ev_base_);
    ASSERT_NULL(dummy.errcounts_sampler_);

    ASSERT_EQ(ORCM_SUCCESS, dummy.init());
    ASSERT_NOT_NULL(dummy.collector_);
    ASSERT_STREQ("test_host", dummy.hostname_.c_str());
    dummy.finalize();
    ASSERT_NOT_NULL(dummy.collector_);
}

TEST_F(ut_edac_collector_tests, test_start_and_stop)
{
    errcounts_impl dummy;

    dummy.start(0);
    ASSERT_NULL(dummy.ev_base_);
    ASSERT_NULL(dummy.errcounts_sampler_);

    dummy.stop(0);
    ASSERT_NULL(dummy.ev_base_);
    ASSERT_NULL(dummy.errcounts_sampler_);
    dummy.finalize();

    mca_sensor_errcounts_component.use_progress_thread = true;
    dummy.init();

    dummy.start(0);
    EXPECT_NOT_NULL(dummy.ev_base_);
    EXPECT_NOT_NULL(dummy.errcounts_sampler_);

    dummy.stop(0);
    ASSERT_NOT_NULL(dummy.ev_base_);
    ASSERT_NULL(dummy.errcounts_sampler_);
}

TEST_F(ut_edac_collector_tests, test_get_and_set_sample_rate)
{
    int rate = -999;
    errcounts_impl dummy;

    dummy.get_sample_rate(&rate);
    ASSERT_EQ(0, rate);

    rate = -999;
    dummy.set_sample_rate(5);
    dummy.get_sample_rate(&rate);
    ASSERT_EQ(0, rate);

    mca_sensor_errcounts_component.use_progress_thread = true;

    rate = -999;
    dummy.get_sample_rate(&rate);
    ASSERT_EQ(0, rate);

    rate = -999;
    dummy.set_sample_rate(5);
    dummy.get_sample_rate(&rate);
    ASSERT_EQ(5, rate);
}

TEST_F(ut_edac_collector_tests, test_ev_create_thread_and_destroy_thread_negative)
{
    errcounts_impl dummy;

    dummy.ev_create_thread();
    ASSERT_NULL(dummy.ev_base_);

    dummy.errcounts_sampler_ = OBJ_NEW(orcm_sensor_sampler_t);

    dummy.ev_create_thread();
    ASSERT_NOT_NULL(dummy.ev_base_);
    dummy.ev_destroy_thread();
    ASSERT_NULL(dummy.ev_base_);

    SAFE_OBJ_RELEASE(dummy.errcounts_sampler_);
}

TEST_F(ut_edac_collector_tests, test_ev_pause_and_resume)
{
    errcounts_impl dummy;

    // No per-thread sampling...
    dummy.init();
    dummy.start(0);
    ASSERT_NULL(dummy.ev_base_);
    ASSERT_FALSE(dummy.ev_paused_);

    dummy.ev_pause();
    ASSERT_FALSE(dummy.ev_paused_);
    dummy.ev_resume();
    ASSERT_FALSE(dummy.ev_paused_);

    dummy.stop(0);
    dummy.finalize();

    // Per-thread sampling...
    mca_sensor_errcounts_component.use_progress_thread = true;

    dummy.init();
    dummy.start(0);
    ASSERT_NOT_NULL(dummy.ev_base_);
    ASSERT_FALSE(dummy.ev_paused_);

    dummy.ev_pause();
    ASSERT_TRUE(dummy.ev_paused_);
    dummy.ev_resume();
    ASSERT_FALSE(dummy.ev_paused_);

    dummy.stop(0);
    dummy.finalize();
}

TEST_F(ut_edac_collector_tests, test_perthread_errcounts_sample)
{
    mca_sensor_errcounts_component.test = false;
    mca_sensor_errcounts_component.use_progress_thread = true;
    mca_sensor_errcounts_component.sample_rate = 1;

    errcounts_impl dummy;
    edac_collector* saved = dummy.collector_;

    dummy.collector_ = new edac_collector(ErrorSink, "./mc");
    dummy.init();
    dummy.start(0);
    ASSERT_FALSE(dummy.edac_missing_);

    dummy.perthread_errcounts_sample();

    ASSERT_EQ(12, dummy.data_samples_labels_.size());

    dummy.stop(0);
    dummy.finalize();
    delete dummy.collector_;
    dummy.collector_ = saved;
}

TEST_F(ut_edac_collector_tests, test_sample)
{
    mca_sensor_errcounts_component.use_progress_thread = false;

    orcm_sensor_sampler_t sampler;

    errcounts_impl dummy;
    dummy.init();
    edac_collector* saved = dummy.collector_;
    dummy.collector_ = new edac_collector(ErrorSink, "./mc");

    opal_output_verbose_.clear();
    last_orte_error_ = 0;
    dummy.sample(&sampler);
    ASSERT_EQ(OPAL_SUCCESS, last_orte_error_);

    ASSERT_EQ(1, opal_output_verbose_.size());
    ASSERT_STREQ("errcounts_tests sensor errcounts : errcounts_sample: called", opal_output_verbose_[0].c_str());

    opal_buffer_t* buffer = &(sampler.bucket);

    string str;
    dummy.unpack_string(buffer, str);
    ASSERT_STREQ(plugin_name_, str.c_str());

    dummy.unpack_string(buffer, str);
    ASSERT_STREQ(hostname_, str.c_str());

    struct timeval tv;
    dummy.unpack_timestamp(buffer, tv);

    dummy.unpack_data_sample(buffer);
    dummy.data_samples_labels_.clear();
    dummy.data_samples_values_.clear();

    for(size_t i = 0; i < dummy.log_samples_labels_.size(); ++i) {
        EXPECT_EQ(golden_data_[dummy.log_samples_labels_[i]], dummy.log_samples_values_[i]);
    }
    dummy.log_samples_labels_.clear();
    dummy.log_samples_values_.clear();

    for(int i = 0; i < 4; ++i) {
        dummy.data_samples_labels_.clear();
        dummy.data_samples_values_.clear();
        dummy.collector_->previous_sample_->clear();
        last_orte_error_ = ORCM_SUCCESS;
        fail_pack_on_ = i + 1;
        fail_pack_count_ = 0;
        dummy.sample(&sampler);
        ASSERT_NE(OPAL_SUCCESS, last_orte_error_);
    }
    dummy.data_samples_labels_.clear();
    dummy.data_samples_values_.clear();

    last_orte_error_ = ORCM_SUCCESS;
    fail_pack_on_ = -1;
    fail_pack_buffer_ = true;
    dummy.collector_->previous_sample_->clear();
    dummy.sample(&sampler);
    ASSERT_NE(OPAL_SUCCESS, last_orte_error_);

    delete dummy.collector_;
    dummy.collector_ = saved;
}

TEST_F(ut_edac_collector_tests, test_inventory_collect)
{
    opal_buffer_t buffer;

    errcounts_impl dummy;
    dummy.init();
    edac_collector* saved = dummy.collector_;
    dummy.collector_ = new edac_collector(ErrorSink, "./mc");

    last_orte_error_ = 0;
    dummy.inventory_collect(&buffer);
    ASSERT_EQ(OPAL_SUCCESS, last_orte_error_);
    ASSERT_EQ(12, dummy.inv_samples_.size());

    string str;
    dummy.unpack_string(&buffer, str);
    ASSERT_STREQ(plugin_name_, str.c_str());

    dummy.unpack_string(&buffer, str);
    ASSERT_STREQ(hostname_, str.c_str());

    dummy.unpack_inv_sample(&buffer);
    for(map<string,string>::iterator it = dummy.inv_samples_.begin(); it != dummy.inv_samples_.end(); ++it) {
        EXPECT_EQ(golden_inv_[it->first], it->second);
    }

    for(int i = 0; i < 3; ++i) {
        dummy.inv_samples_.clear();
        last_orte_error_ = ORCM_SUCCESS;
        fail_pack_on_ = i + 1;
        fail_pack_count_ = 0;
        dummy.inventory_collect(&buffer);
        ASSERT_NE(OPAL_SUCCESS, last_orte_error_);
    }
    delete dummy.collector_;
    dummy.collector_ = saved;
}

TEST_F(ut_edac_collector_tests, test_log)
{
    orcm_sensor_sampler_t sampler;
    opal_buffer_t buffer;

    errcounts_impl dummy;
    dummy.init();
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);
    last_orte_error_ = ORCM_SUCCESS;

    dummy.sample(&sampler);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);
    last_orte_error_ = ORCM_SUCCESS;

    // Remove plugin name
    string plugin;
    ASSERT_TRUE(dummy.unpack_string(&buffer, plugin));
    ASSERT_STREQ(plugin_name_, plugin.c_str());

    dummy.log(&buffer);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);
    ASSERT_EQ(0, packed_str_.size());
    ASSERT_EQ(0, packed_int32_.size());
    ASSERT_EQ(0, packed_ts_.size());
    last_orte_error_ = ORCM_SUCCESS;
    dummy.data_samples_labels_.clear();
    dummy.data_samples_values_.clear();

    ASSERT_EQ(1, current_analytics_values_.size());

    for(int i = 0; i < current_analytics_values_.size(); ++i) {
        SAFE_OBJ_RELEASE(current_analytics_values_[i]);
    }
    current_analytics_values_.clear();
    dummy.data_samples_labels_.clear();
    dummy.data_samples_values_.clear();
    dummy.log_samples_labels_.clear();
    dummy.log_samples_values_.clear();
    ClearBuffers();

    for(int i = 0; i < 4; ++i) {
        last_orte_error_ = ORCM_SUCCESS;

        // we need to get fresh samples, not filtered samples
        dummy.collector_->previous_sample_->clear();

        dummy.sample(&sampler);
        ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

        // Remove pluging name like orcm would...
        fail_unpack_on_ = -1;
        fail_unpack_count_ = 0;
        last_orte_error_ = ORCM_SUCCESS;
        ASSERT_TRUE(dummy.unpack_string(&buffer, plugin));
        ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);
        ASSERT_STREQ(plugin_name_, plugin.c_str());

        fail_unpack_on_ = i + 1;
        fail_unpack_count_ = 0;
        last_orte_error_ = ORCM_SUCCESS;
        dummy.log(&buffer);
        ASSERT_EQ(ORCM_ERR_UNPACK_FAILURE, last_orte_error_);
        ASSERT_NE(0, packed_str_.size());
        ASSERT_NE(0, packed_int32_.size());

        for(int i = 0; i < current_analytics_values_.size(); ++i) {
            SAFE_OBJ_RELEASE(current_analytics_values_[i]);
        }
        current_analytics_values_.clear();
        dummy.data_samples_labels_.clear();
        dummy.data_samples_values_.clear();
        dummy.log_samples_labels_.clear();
        dummy.log_samples_values_.clear();

        ClearBuffers();
    }
    dummy.finalize();
}

TEST_F(ut_edac_collector_tests, test_log_negative)
{
    orcm_sensor_sampler_t sampler;
    opal_buffer_t buffer;

    errcounts_impl dummy;
    dummy.init();
    edac_collector* saved = dummy.collector_;
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);
    last_orte_error_ = ORCM_SUCCESS;

    dummy.sample(&sampler);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);
    last_orte_error_ = ORCM_SUCCESS;

    string plugin;
    ASSERT_TRUE(dummy.unpack_string(&buffer, plugin));
    ASSERT_STREQ(plugin_name_, plugin.c_str());

    dummy.collector_ = new edac_collector_mock_stat(ErrorSink, "./mc");
    last_orte_error_ = -1024;
    dummy.log(&buffer);
    ASSERT_EQ(-1024, last_orte_error_);
    delete dummy.collector_;
    dummy.collector_ = saved;
    dummy.finalize();
}

TEST_F(ut_edac_collector_tests, test_edac_missing)
{
    orcm_sensor_sampler_t sampler;

    errcounts_impl dummy;
    edac_collector* saved = dummy.collector_;
    dummy.collector_ = new edac_collector_mock_stat(ErrorSink, "./mc");
    dummy.init();
    last_orte_error_ = -2048;
    dummy.start(1);
    ASSERT_EQ(-2048, last_orte_error_);
    dummy.stop(1);
    ASSERT_EQ(-2048, last_orte_error_);
    dummy.log(NULL);
    ASSERT_EQ(-2048, last_orte_error_);
    dummy.inventory_collect(NULL);
    ASSERT_EQ(-2048, last_orte_error_);
    dummy.inventory_log(NULL, NULL);
    ASSERT_EQ(-2048, last_orte_error_);
    dummy.set_sample_rate(5);
    ASSERT_EQ(-2048, last_orte_error_);
    dummy.get_sample_rate(NULL);
    ASSERT_EQ(-2048, last_orte_error_);

    dummy.finalize();
    delete dummy.collector_;
    dummy.collector_ = saved;
}

TEST_F(ut_edac_collector_tests, test_test_vector)
{
    mca_sensor_errcounts_component.test = true;
    orcm_sensor_sampler_t sampler;
    opal_buffer_t buffer;
    errcounts_impl dummy;
    dummy.collector_->error_callback = ErrorSink;
    dummy.init();

    last_orte_error_ = ORCM_SUCCESS;
    dummy.inventory_collect(&buffer);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);
    dummy.sample(&sampler);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    dummy.finalize();
}

TEST_F(ut_edac_collector_tests, test_error_callback)
{
    string path="/sysfs/madeuppath/entry";
    int err = 54;
    string golden="WARNING: errcounts_tests sensor errcounts : sample: trouble accessing sysfs entry (errno=54): '/sysfs/madeuppath/entry': skipping data entry";
    errcounts_impl dummy;

    dummy.error_callback(path.c_str(), err);

    ASSERT_EQ(1, opal_output_verbose_.size());
    ASSERT_STREQ(golden.c_str(), opal_output_verbose_[0].c_str());
}

TEST_F(ut_edac_collector_tests, test_inventory_log)
{
    opal_buffer_t buffer;
    OBJ_CONSTRUCT(&buffer, opal_buffer_t);
    mca_sensor_errcounts_component.test = false;
    errcounts_impl dummy;
    edac_collector* edac_saved = dummy.collector_;
    dummy.collector_ = new edac_collector(ErrorSink, "./mc");
    dummy.init();

    dummy.inventory_collect(&buffer);

    // Unpack plugin name like ORCM...
    string plugin;
    ASSERT_TRUE(dummy.unpack_string(&buffer, plugin));
    ASSERT_STREQ(plugin_name_, plugin.c_str());

    orcm_db_base_API_store_new_fn_t saved = orcm_db.store_new;
    orcm_db.store_new = MyDbStoreNew;

    dummy.inventory_log((char*)hostname_, &buffer);

    orcm_db.store_new = saved;

    ASSERT_EQ(13, database_data_.size());
    ASSERT_STREQ(hostname_, database_data_["hostname"].c_str());
    ASSERT_EQ(1, database_data_.erase("hostname"));

    for(map<string,string>::iterator it = database_data_.begin(); it != database_data_.end(); ++it) {
        ASSERT_STREQ(golden_inv_[it->first].c_str(), it->second.c_str());
    }

    for(int i = 0; i < 4; ++i) {
        ResetTestEnvironment();

        dummy.inventory_collect(&buffer);

        // Unpack plugin name like ORCM...
        fail_unpack_on_ = -1;
        fail_unpack_count_ = 0;
        last_orte_error_ = ORCM_SUCCESS;
        ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);
        ASSERT_TRUE(dummy.unpack_string(&buffer, plugin));
        ASSERT_STREQ(plugin_name_, plugin.c_str());
        ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

        saved = orcm_db.store_new;
        orcm_db.store_new = MyDbStoreNew;

        fail_unpack_on_ = i + 1;
        fail_unpack_count_ = 0;
        last_orte_error_ = ORCM_SUCCESS;
        dummy.inventory_log((char*)hostname_, &buffer);

        orcm_db.store_new = saved;

        ASSERT_EQ(ORCM_ERR_UNPACK_FAILURE, last_orte_error_);
    }

    ResetTestEnvironment();
    dummy.inventory_collect(&buffer);

    // Unpack plugin name like ORCM...
    ASSERT_TRUE(dummy.unpack_string(&buffer, plugin));
    ASSERT_STREQ(plugin_name_, plugin.c_str());

    saved = orcm_db.store_new;
    orcm_db.store_new = MyDbStoreNew;
    int saved2 = orcm_sensor_base.dbhandle;
    orcm_sensor_base.dbhandle = -1;

    dummy.inventory_log((char*)hostname_, &buffer);

    orcm_sensor_base.dbhandle = saved2;
    orcm_db.store_new = saved;

    ASSERT_EQ(0, database_data_.size());

    dummy.finalize();
    delete dummy.collector_;
    dummy.collector_ = edac_saved;

    OBJ_DESTRUCT(&buffer);
}

TEST_F(ut_edac_collector_tests, test_component_functions)
{
    mca_base_module_t* module = NULL;
    int priority = -1;
    int rv = orcm_sensor_errcounts_query(&module, &priority);
    ASSERT_EQ(ORCM_SUCCESS, rv);
    ASSERT_NE(-1, priority);
    ASSERT_NOT_NULL(module);

    rv = orcm_sensor_errcounts_open();
    ASSERT_EQ(ORCM_SUCCESS, rv);
    rv = errcounts_component_register();
    ASSERT_EQ(ORCM_SUCCESS, rv);
    rv = orcm_sensor_errcounts_close();
    ASSERT_EQ(ORCM_SUCCESS, rv);
}

TEST_F(ut_edac_collector_tests, test_init_negative)
{
    errcounts_impl dummy;
    edac_collector* saved = dummy.collector_;
    dummy.collector_ = new edac_collector_mock_stat(ErrorSink, "./mc");
    ASSERT_EQ(ORCM_ERROR, dummy.init());
    delete dummy.collector_;
    dummy.collector_ = saved;
}

// Testing the data collection class
TEST_F(ut_edac_collector_tests, errcounts_sensor_sample_tests)
{
    {
        errcounts_impl errcounts;
        orcm_sensor_base.collect_metrics = false;
        mca_sensor_errcounts_component.collect_metrics = false;
        errcounts.init();
        errcounts.collect_sample();
        EXPECT_EQ(0, (errcounts.diagnostics_ & 0x1));
        errcounts.finalize();
    }
    {
        errcounts_impl errcounts;
        orcm_sensor_base.collect_metrics = true;
        mca_sensor_errcounts_component.collect_metrics = false;
        errcounts.init();
        errcounts.collect_sample();
        EXPECT_EQ(0, (errcounts.diagnostics_ & 0x1));
        errcounts.finalize();
    }
    {
        errcounts_impl errcounts;
        orcm_sensor_base.collect_metrics = true;
        mca_sensor_errcounts_component.collect_metrics = true;
        errcounts.init();
        errcounts.collect_sample();
        EXPECT_EQ(1, (errcounts.diagnostics_ & 0x1));
        errcounts.finalize();
    }
}

TEST_F(ut_edac_collector_tests, errcounts_api_tests)
{
    // Setup
    errcounts_impl errcounts;
    orcm_sensor_base.collect_metrics = false;
    mca_sensor_errcounts_component.collect_metrics = false;
    errcounts.init();

    // Tests
    errcounts.enable_sampling("errcounts");
    EXPECT_TRUE(errcounts.collect_metrics_->DoCollectMetrics(NULL));
    errcounts.disable_sampling("errcounts");
    EXPECT_FALSE(errcounts.collect_metrics_->DoCollectMetrics(NULL));
    errcounts.enable_sampling("all");
    EXPECT_TRUE(errcounts.collect_metrics_->DoCollectMetrics(NULL));
    errcounts.enable_sampling("not_the_right_datagroup");
    EXPECT_TRUE(errcounts.collect_metrics_->DoCollectMetrics(NULL));
    errcounts.reset_sampling("errcounts");
    EXPECT_FALSE(errcounts.collect_metrics_->DoCollectMetrics(NULL));

    // Cleanup
    errcounts.finalize();
}

TEST_F(ut_edac_collector_tests, errcounts_api_tests_2)
{
    // Setup
    orcm_sensor_base.collect_metrics = false;
    mca_sensor_errcounts_component.collect_metrics = false;
    errcounts_impl errcounts;
    errcounts.init();
    errcounts.collect_metrics_->TrackSensorLabel("DIMM0");
    errcounts.collect_metrics_->TrackSensorLabel("DIMM1");
    errcounts.collect_metrics_->TrackSensorLabel("DIMM2");
    errcounts.collect_metrics_->TrackSensorLabel("DIMM3");

    // Tests
    errcounts.enable_sampling("errcounts:DIMM3");
    EXPECT_FALSE(errcounts.collect_metrics_->DoCollectMetrics("DIMM1"));
    EXPECT_TRUE(errcounts.collect_metrics_->DoCollectMetrics("DIMM3"));
    EXPECT_EQ(1,errcounts.collect_metrics_->CountOfCollectedLabels());
    errcounts.disable_sampling("errcounts:DIMM3");
    EXPECT_FALSE(errcounts.collect_metrics_->DoCollectMetrics("DIMM3"));
    errcounts.enable_sampling("errcounts:DIMM2");
    EXPECT_TRUE(errcounts.collect_metrics_->DoCollectMetrics("DIMM2"));
    errcounts.reset_sampling("errcounts:DIMM2");
    EXPECT_FALSE(errcounts.collect_metrics_->DoCollectMetrics("DIMM2"));
    errcounts.enable_sampling("errcounts:no_DIMM");
    EXPECT_FALSE(errcounts.collect_metrics_->DoCollectMetrics("DIMM2"));
    errcounts.enable_sampling("errcounts:all");
    EXPECT_TRUE(errcounts.collect_metrics_->DoCollectMetrics("DIMM0"));
    EXPECT_TRUE(errcounts.collect_metrics_->DoCollectMetrics("DIMM3"));
    EXPECT_EQ(4,errcounts.collect_metrics_->CountOfCollectedLabels());

    // Cleanup
    errcounts.finalize();
}

TEST_F(ut_edac_collector_tests, test_sample_and_inv_test)
{
    opal_buffer_t buffer;
    OBJ_CONSTRUCT(&buffer, opal_buffer_t);
    errcounts_impl* impl = new errcounts_impl();
    impl->init();
    impl->generate_test_samples(false);
    impl->generate_test_samples(true);
    impl->generate_inventory_test_vector(&buffer);

    OBJ_DESTRUCT(&buffer);
    delete impl;
}
