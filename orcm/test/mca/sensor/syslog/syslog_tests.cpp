/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "syslog_tests.h"

// ORTE
#include "orte/runtime/orte_globals.h"

// ORCM
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/syslog/sensor_syslog.h"

// C
#include <unistd.h>

// C++
#include <iostream>

// Fixture
using namespace std;

#define DEF_MS_PERIOD (75)

#define SOCKET_FD (30042)

#define SAFE_OBJ_RELEASE(x) if(NULL!=x) { OBJ_RELEASE(x); x=NULL; }

extern "C" {
    ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
    extern orcm_sensor_base_module_t orcm_sensor_syslog_module;
    extern void collect_syslog_sample(orcm_sensor_sampler_t *sampler);
    extern int syslog_enable_sampling(const char* sensor_specification);
    extern int syslog_disable_sampling(const char* sensor_specification);
    extern int syslog_reset_sampling(const char* sensor_specification);
    extern int orcm_sensor_syslog_open(void);
    extern int orcm_sensor_syslog_close(void);
    extern int orcm_sensor_syslog_query(mca_base_module_t **module, int *priority);
    extern int syslog_component_register(void);
    extern void perthread_syslog_sample(int fd, short args, void *cbdata);
    extern const char *syslog_get_severity(int  prival);
    extern const char *syslog_get_facility(int prival);
    extern bool orcm_sensor_syslog_generate_test_vector(opal_buffer_t* buffer);

    extern __uid_t __real_geteuid();
    __uid_t __wrap_geteuid()
    {
        return ut_syslog_tests::euid_;
    }

    extern void __real_close(int fd);
    void __wrap_close(int fd)
    {
        if(SOCKET_FD != fd) {
            __real_close(fd);
        } else {
            ut_syslog_tests::flags_ |= 0x0008;
        }
    }

    extern int __real_socket(int a, int b, int c);
    int __wrap_socket(int a, int b, int c)
    {
        if(SOCKET_FD == ut_syslog_tests::syslog_socket_) {
            ut_syslog_tests::flags_ |= 0x0001;
        }
        return ut_syslog_tests::syslog_socket_;
    }

    extern ssize_t __real_recv(int a, void* b, size_t c, int d);
    ssize_t __wrap_recv(int a, void* b, size_t c, int d)
    {
        if(SOCKET_FD != a) {
            return -1;
        } else {
            ut_syslog_tests::flags_ |= 0x0002;
            return ut_syslog_tests::GetSyslogEntry((char*)b, c);
        }
    }

    extern int __real_bind(int a, const struct sockaddr* b, socklen_t c);
    int __wrap_bind(int a, const struct sockaddr* b, socklen_t c)
    {
        if(SOCKET_FD != a) {
            return -1;
        } else {
            ut_syslog_tests::flags_ |= 0x0004;
            return 0;
        }
    }

    extern opal_event_base_t* __real_opal_progress_thread_init(const char* name);
    opal_event_base_t* __wrap_opal_progress_thread_init(const char* name)
    {
        if(ut_syslog_tests::use_pt_) {
            return __real_opal_progress_thread_init(name);
        } else {
            return NULL;
        }
    }

    extern int __real_opal_progress_thread_finalize(const char* name);
    int __wrap_opal_progress_thread_finalize(const char* name)
    {
        if(ut_syslog_tests::use_pt_) {
            return __real_opal_progress_thread_finalize(name);
        } else {
            return ORCM_ERROR;
        }
    }
};

__uid_t ut_syslog_tests::euid_ = 0; // root
int ut_syslog_tests::syslog_socket_ = SOCKET_FD;
unsigned short ut_syslog_tests::flags_ = 0;
std::vector<std::string> ut_syslog_tests::sysLog_;
size_t ut_syslog_tests::sysLogIndex_ = 0;
bool ut_syslog_tests::use_pt_ = true;
bool ut_syslog_tests::bad_syslog_entry_ = false;

void ut_syslog_tests::SetUpTestCase()
{
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();

    sysLog_.clear();
    sysLog_.push_back("<30>1 2014-07-31T13:47:30.957146+02:00 host1 snmpd 23611 - - Connection from UDP: [127.0.0.1]:58374->[127.0.0.1]");

    ResetTestCase();
}

void ut_syslog_tests::TearDownTestCase()
{
    // Release OPAL level resources
    opal_dss_close();

    sysLog_.clear();
}

void ut_syslog_tests::ResetTestCase()
{
    euid_ = 0;
    syslog_socket_ = SOCKET_FD;
    flags_ = 0;
    sysLogIndex_ = 0;
    use_pt_ = true;
    bad_syslog_entry_ = false;

    mca_sensor_syslog_component.test = false;
    mca_sensor_syslog_component.collect_metrics = true;
    mca_sensor_syslog_component.use_progress_thread = false;
    mca_sensor_syslog_component.sample_rate = 1;
    mca_sensor_syslog_component.diagnostics = 0;
}

ssize_t ut_syslog_tests::GetSyslogEntry(char* buffer, size_t max_size)
{
    if(!bad_syslog_entry_) {
        if(sysLog_.size() == sysLogIndex_ || NULL == buffer || 0 == max_size) {
            return 0;
        } else {
            try {
                const char* raw = sysLog_[sysLogIndex_].c_str();
                if(NULL == raw) {
                    throw logic_error("message was NULL!");
                }
                ++sysLogIndex_;
                string msg = raw;
                size_t max_len = MIN(max_size-1, msg.size()+1);
                strncpy(buffer, msg.c_str(), max_len);
                buffer[max_len] = '\0';
                return max_len;
            } catch(logic_error& err) {
                return 0;
            }
        }
    } else {
        strncpy(buffer, "Hello World!", max_size);
        bad_syslog_entry_ = false;
    }
}
bool ut_syslog_tests::ValidateSyslogData(opal_buffer_t* buffer)
{
    bool test_result = true;
    opal_buffer_t remaining;
    OBJ_CONSTRUCT(&remaining, opal_buffer_t);
    opal_dss.copy_payload(&remaining, buffer);

    while(true)
    {
        struct timeval ts;
        int count = 0;
        char* name = NULL;
        char* severity = NULL;
        char* log_line = NULL;
        char* host = NULL;
        int n = 1;
        uint8_t flags = 0;
        int rv = opal_dss.unpack(&remaining, &name, &n, OPAL_STRING);
        if(ORCM_SUCCESS != rv) {
            test_result = false;
            break;
        } else {
            SAFEFREE(name);
        }
        n = 1;
        rv = opal_dss.unpack(&remaining, &count, &n, OPAL_INT32);
        if(ORCM_SUCCESS != rv) {
            test_result = false;
            break;
        }
        n = 1;
        rv = opal_dss.unpack(&remaining, &ts, &n, OPAL_TIMEVAL);
        if(ORCM_SUCCESS != rv) {
            test_result = false;
            break;
        }
        n = 1;
        rv = opal_dss.unpack(&remaining, &flags, &n, OPAL_UINT8);
        if(ORCM_SUCCESS != rv) {
            test_result = false;
            break;
        }
        n = 1;
        rv = opal_dss.unpack(&remaining, &severity, &n, OPAL_STRING);
        if(ORCM_SUCCESS != rv) {
            test_result = false;
            break;
        } else {
            SAFEFREE(severity);
        }
        n = 1;
        rv = opal_dss.unpack(&remaining, &log_line, &n, OPAL_STRING);
        if(ORCM_SUCCESS != rv) {
            test_result = false;
            break;
        } else {
            SAFEFREE(log_line);
        }
        break;
    }
    OBJ_DESTRUCT(&remaining);
    return test_result;
}

void ut_syslog_tests::Cleanup(void* sampler, void* logBuffer, int jobid)
{
    orcm_sensor_sampler_t* psampler = (orcm_sensor_sampler_t*)sampler;
    opal_buffer_t** plogBuffer = (opal_buffer_t**)logBuffer;
    if(NULL != psampler) {
        OBJ_DESTRUCT(psampler);
    }
    if(NULL != plogBuffer) {
        SAFE_OBJ_RELEASE(*plogBuffer);
    }
    if(0 <= jobid) {
        orcm_sensor_syslog_module.stop(jobid);
        mssleep(mca_sensor_syslog_component.sample_rate * 1500);
    }
    orcm_sensor_syslog_module.finalize();
    mca_sensor_syslog_component.use_progress_thread = false;
}

void ut_syslog_tests::ExpectCorrectSeverityAndFacility(int val, const char* expected_sev, const char* expected_fac)
{
    const char* sev = syslog_get_severity(val);
    const char* fac = syslog_get_facility(val);
    if(NULL == expected_sev) {
        EXPECT_EQ(0, (uint64_t)sev);
    } else {
        EXPECT_STREQ(expected_sev, sev);
    }
    if(NULL == expected_fac) {
        EXPECT_EQ(0, (uint64_t)fac);
    } else {
        EXPECT_STREQ(expected_fac, fac);
    }
}

void ut_syslog_tests::AssertCorrectPerThreadStartup(bool use_pt)
{
    ResetTestCase();

    use_pt_ = use_pt;

    mca_sensor_syslog_component.use_progress_thread = true;
    ASSERT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_module.init());
    orcm_sensor_syslog_module.start(6);
    mssleep(mca_sensor_syslog_component.sample_rate * 1500); /* 1.5x sample rate in ms */
    Cleanup(NULL, NULL, 6);
}

void ut_syslog_tests::StoreNew(int handle, orcm_db_data_type_t type, opal_list_t* input,
                               opal_list_t* ret, orcm_db_callback_fn_t cbfunct, void* cbdata)
{
    cbfunct(handle, 0, input, ret, cbdata);
}

void ut_syslog_tests::mssleep(uint32_t milliseconds)
{
    struct timespec wait;
    wait.tv_sec = (time_t)milliseconds / 1000;
    wait.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
    nanosleep(&wait, NULL);
}

// Testing the data collection class
TEST_F(ut_syslog_tests, syslog_sensor_sample_tests)
{
    ResetTestCase();

    // Setup
    ASSERT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_module.init());
    orcm_sensor_syslog_module.start(5);
    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_syslog_component.runtime_metrics);
    void* object = orcm_sensor_base_runtime_metrics_create("syslog", false, false);
    mca_sensor_syslog_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    // Tests
    orcm_sensor_syslog_module.sample(sampler);
    EXPECT_EQ(0, (mca_sensor_syslog_component.diagnostics & 0x1));

    orcm_sensor_base_runtime_metrics_set(object, true, "syslog");
    orcm_sensor_syslog_module.sample(sampler);
    EXPECT_EQ(1, (mca_sensor_syslog_component.diagnostics & 0x1));

    // Cleanup
    SAFE_OBJ_RELEASE(sampler);
    orcm_sensor_syslog_module.stop(5);
    orcm_sensor_syslog_module.finalize();
}

TEST_F(ut_syslog_tests, syslog_api_tests)
{
    ResetTestCase();

    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("syslog", false, false);
    mca_sensor_syslog_component.runtime_metrics = object;

    // Tests
    syslog_enable_sampling("syslog");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    syslog_disable_sampling("syslog");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    syslog_enable_sampling("syslog");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    syslog_reset_sampling("syslog");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    syslog_enable_sampling("not_the_right_datagroup");
    EXPECT_FALSE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));
    syslog_enable_sampling("all");
    EXPECT_TRUE(orcm_sensor_base_runtime_metrics_do_collect(object, NULL));

    // Cleanup
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_syslog_component.runtime_metrics = NULL;
}

TEST_F(ut_syslog_tests, syslog_init_finalize_positive)
{
    ResetTestCase();

    int rv = orcm_sensor_syslog_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);
    EXPECT_NE(0, (uint64_t)(mca_sensor_syslog_component.runtime_metrics));
    orcm_sensor_syslog_module.finalize();
}

#if 0 // Deprecated
TEST_F(ut_syslog_tests, syslog_init_finalize_negative)
{
    ResetTestCase();

    euid_ = 1000; // Not Root

    int rv = orcm_sensor_syslog_module.init();
    EXPECT_NE(ORCM_SUCCESS, rv);
    EXPECT_EQ(0, (uint64_t)(mca_sensor_syslog_component.runtime_metrics));
    orcm_sensor_syslog_module.finalize();
}
#endif // 0

TEST_F(ut_syslog_tests, syslog_start_stop)
{
    ResetTestCase();

    mca_sensor_syslog_component.sample_rate = 0;
    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_module.init());
    orcm_sensor_syslog_module.start(5);
    mssleep(DEF_MS_PERIOD);
    orcm_sensor_syslog_module.stop(5);
    orcm_sensor_syslog_module.finalize();
}

TEST_F(ut_syslog_tests, syslog_start_stop_negative1)
{
    ResetTestCase();

    syslog_socket_ = -1; // Invalid socket

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_module.init());
    orcm_sensor_syslog_module.start(5);
    orcm_sensor_syslog_module.stop(5);
    orcm_sensor_syslog_module.finalize();
}

TEST_F(ut_syslog_tests, syslog_start_stop_negative2)
{
    ResetTestCase();

    mca_sensor_syslog_component.use_progress_thread = true;
    use_pt_ = false;

    orcm_sensor_syslog_module.start(5);
    mssleep(DEF_MS_PERIOD);
    orcm_sensor_syslog_module.stop(5);
}

TEST_F(ut_syslog_tests, syslog_sample_rate_tests)
{
    ResetTestCase();

    orcm_sensor_syslog_module.set_sample_rate(5);
    EXPECT_EQ(1, mca_sensor_syslog_component.sample_rate);
    mca_sensor_syslog_component.use_progress_thread = true;
    orcm_sensor_syslog_module.set_sample_rate(5);
    EXPECT_EQ(5, mca_sensor_syslog_component.sample_rate);
    int rate = 0;
    orcm_sensor_syslog_module.get_sample_rate(&rate);
    EXPECT_EQ(5, rate);
}

TEST_F(ut_syslog_tests, syslog_inventory_sample_log_test)
{
    ResetTestCase();

    mca_sensor_syslog_component.use_progress_thread = false;
    // It appears this feature is not implemented in syslog sensor plugin!
}

TEST_F(ut_syslog_tests, syslog_sample_log_test)
{
    ResetTestCase();

    ASSERT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_module.init());
    orcm_sensor_syslog_module.start(6);
    sysLogIndex_ = 0;
    while(0 == sysLogIndex_);
    mssleep(DEF_MS_PERIOD);
    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    orcm_sensor_syslog_module.sample(&sampler);
    opal_buffer_t* buffer = &sampler.bucket;

    int n = 1;
    opal_buffer_t* logBuffer = NULL;
    int rv = opal_dss.unpack(buffer, &logBuffer, &n, OPAL_BUFFER);
    EXPECT_EQ(ORCM_SUCCESS, rv);
    EXPECT_NE(0, (uint64_t)logBuffer);
    bool failed = (ORCM_SUCCESS != rv || NULL == buffer);
    if(!failed)
    {
        char* plugin = NULL;
        n = 1;
        rv = opal_dss.unpack(logBuffer, &plugin, &n, OPAL_STRING);
        failed = (ORCM_SUCCESS != rv || NULL == buffer);
        EXPECT_STREQ("syslog", plugin);
        SAFEFREE(plugin);
        EXPECT_TRUE(ValidateSyslogData(logBuffer));
        orcm_sensor_syslog_module.log(logBuffer);
    }

    Cleanup(&sampler, &logBuffer, 6);
}

TEST_F(ut_syslog_tests, syslog_sample_log_test2)
{
    ResetTestCase();

    bad_syslog_entry_ = true;

    ASSERT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_module.init());
    orcm_sensor_syslog_module.start(6);
    mssleep(DEF_MS_PERIOD);
    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    orcm_sensor_syslog_module.sample(&sampler);
    opal_buffer_t* buffer = &sampler.bucket;

    int n = 1;
    opal_buffer_t* logBuffer = NULL;
    int rv = opal_dss.unpack(buffer, &logBuffer, &n, OPAL_BUFFER);
    EXPECT_EQ(ORCM_SUCCESS, rv);
    EXPECT_NE(0, (uint64_t)logBuffer);
    bool failed = (ORCM_SUCCESS != rv || NULL == buffer);
    if(!failed)
    {
        char* plugin = NULL;
        n = 1;
        rv = opal_dss.unpack(logBuffer, &plugin, &n, OPAL_STRING);
        failed = (ORCM_SUCCESS != rv || NULL == buffer);
        EXPECT_STREQ("syslog", plugin);
        SAFEFREE(plugin);
        EXPECT_TRUE(ValidateSyslogData(logBuffer));
        orcm_sensor_syslog_module.log(logBuffer);
    }

    Cleanup(&sampler, &logBuffer, 6);
}

TEST_F(ut_syslog_tests, syslog_sample_log_test3)
{
    ResetTestCase();

    ASSERT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_module.init());
    orcm_sensor_syslog_module.start(6);
    mssleep(DEF_MS_PERIOD);
    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    mca_sensor_syslog_component.use_progress_thread = true;
    orcm_sensor_syslog_module.sample(&sampler);
    mca_sensor_syslog_component.use_progress_thread = false;
    opal_buffer_t* buffer = &sampler.bucket;

    int n = 1;
    opal_buffer_t* logBuffer = NULL;
    int rv = opal_dss.unpack(buffer, &logBuffer, &n, OPAL_BUFFER);
    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER, rv);

    Cleanup(&sampler, &logBuffer, 6);
}

TEST_F(ut_syslog_tests, syslog_sample_log_test4)
{
    ResetTestCase();

    sysLogIndex_ = 1;

    ASSERT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_module.init());
    orcm_sensor_syslog_module.start(6);
    mssleep(DEF_MS_PERIOD);
    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    orcm_sensor_syslog_module.sample(&sampler);
    opal_buffer_t* buffer = &sampler.bucket;

    int n = 1;
    opal_buffer_t* logBuffer = NULL;
    int rv = opal_dss.unpack(buffer, &logBuffer, &n, OPAL_BUFFER);
    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER, rv);

    Cleanup(&sampler, &logBuffer, 6);
}

TEST_F(ut_syslog_tests, syslog_sample_log_test5)
{
    ResetTestCase();

    ASSERT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_module.init());
    orcm_sensor_syslog_module.disable_sampling("syslog:severity");
    orcm_db_base_API_store_new_fn_t saved = orcm_db.store_new;
    orcm_db.store_new = ut_syslog_tests::StoreNew;
    mssleep(DEF_MS_PERIOD);
    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    orcm_sensor_syslog_module.sample(&sampler);
    opal_buffer_t* buffer = &sampler.bucket;

    int n = 1;
    opal_buffer_t* logBuffer = NULL;
    int rv = opal_dss.unpack(buffer, &logBuffer, &n, OPAL_BUFFER);
    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER, rv);

    orcm_db.store_new = saved;
    orcm_sensor_syslog_module.reset_sampling("syslog");
    Cleanup(&sampler, &logBuffer, -1);
}

TEST_F(ut_syslog_tests, syslog_sample_log_test6)
{
    ResetTestCase();

    ASSERT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_module.init());
    orcm_sensor_syslog_module.disable_sampling("syslog:log_message");
    orcm_db_base_API_store_new_fn_t saved = orcm_db.store_new;
    orcm_db.store_new = ut_syslog_tests::StoreNew;
    mssleep(DEF_MS_PERIOD);
    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    orcm_sensor_syslog_module.sample(&sampler);
    opal_buffer_t* buffer = &sampler.bucket;

    int n = 1;
    opal_buffer_t* logBuffer = NULL;
    int rv = opal_dss.unpack(buffer, &logBuffer, &n, OPAL_BUFFER);
    EXPECT_EQ(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER, rv);

    orcm_db.store_new = saved;
    orcm_sensor_syslog_module.reset_sampling("syslog");
    Cleanup(&sampler, &logBuffer, -1);
}

TEST_F(ut_syslog_tests, syslog_component_test)
{
    ResetTestCase();

    int priority;
    mca_base_module_t* module = NULL;

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_open());

    EXPECT_EQ(ORCM_SUCCESS, syslog_component_register());

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_query(&module, &priority));
    EXPECT_EQ(50, priority);
    EXPECT_NE(0, (uint64_t)module);

    EXPECT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_close());
}

TEST_F(ut_syslog_tests, syslog_start_stop_with_per_thread_test_positive)
{
    AssertCorrectPerThreadStartup(true);
}

TEST_F(ut_syslog_tests, syslog_start_stop_with_per_thread_test_negative)
{
    AssertCorrectPerThreadStartup(false);
}

TEST_F(ut_syslog_tests, syslog_get_severity_facility_test)
{
    ExpectCorrectSeverityAndFacility(0, "emerg", "KERNEL MESSAGES");
    ExpectCorrectSeverityAndFacility(-1, NULL, NULL);
    ExpectCorrectSeverityAndFacility(3 + (3 * 8), "error", "SYSTEM DAEMONS");
    ExpectCorrectSeverityAndFacility(3 + (24 * 8), "error", NULL);
}

TEST_F(ut_syslog_tests, test_inventory_collect)
{
    ResetTestCase();
    opal_buffer_t buffer;
    OBJ_CONSTRUCT(&buffer, opal_buffer_t);
    mca_sensor_syslog_component.test = true;
    orcm_sensor_syslog_module.init();
    orcm_sensor_syslog_module.inventory_collect(&buffer);
    orcm_sensor_syslog_module.finalize();
    mca_sensor_syslog_component.test = false;
    OBJ_DESTRUCT(&buffer);
}


TEST_F(ut_syslog_tests, test_inventory_log)
{
    ResetTestCase();
    opal_buffer_t buffer;
    OBJ_CONSTRUCT(&buffer, opal_buffer_t);
    mca_sensor_syslog_component.test = true;
    orcm_db_base_API_store_new_fn_t saved = orcm_db.store_new;
    orcm_db.store_new = ut_syslog_tests::StoreNew;
    orcm_sensor_base.dbhandle = 0;
    orcm_sensor_syslog_module.init();
    orcm_sensor_syslog_module.inventory_collect(&buffer);

    char* plugin = NULL;
    int n = 1;
    int rv = opal_dss.unpack(&buffer, &plugin, &n, OPAL_STRING);
    SAFEFREE(plugin);
    EXPECT_EQ(ORCM_SUCCESS, rv);

    orcm_sensor_syslog_module.inventory_log((char*)"testhost1", &buffer);
    orcm_sensor_syslog_module.finalize();
    orcm_db.store_new = saved;
    mca_sensor_syslog_component.test = false;
    OBJ_DESTRUCT(&buffer);
}

TEST_F(ut_syslog_tests, test_generate_vector)
{
    ResetTestCase();
    opal_buffer_t buffer;
    OBJ_CONSTRUCT(&buffer, opal_buffer_t);
    mca_sensor_syslog_component.test = true;
    orcm_sensor_syslog_module.init();
    EXPECT_TRUE(orcm_sensor_syslog_generate_test_vector(&buffer));
    orcm_sensor_syslog_module.finalize();
    mca_sensor_syslog_component.test = false;
    OBJ_DESTRUCT(&buffer);
}
