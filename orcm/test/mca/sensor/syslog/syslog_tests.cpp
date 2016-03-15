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

#define SOCKET_FD (30042)

#define SAFE_OBJ_RELEASE(x) if(NULL!=x) { OBJ_RELEASE(x); x=NULL; }

extern "C" {
    ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
    extern void collect_syslog_sample(orcm_sensor_sampler_t *sampler);
    extern int syslog_enable_sampling(const char* sensor_specification);
    extern int syslog_disable_sampling(const char* sensor_specification);
    extern int syslog_reset_sampling(const char* sensor_specification);

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
            if(SOCKET_FD == a) {
                ut_syslog_tests::flags_ |= 0x0002;
            }
            return ut_syslog_tests::GetSyslogEntry((char*)b, c);
        }
    }

    extern int __real_bind(int a, const struct sockaddr* b, socklen_t c);
    int __wrap_bind(int a, const struct sockaddr* b, socklen_t c)
    {
        if(SOCKET_FD != a) {
            return -1;
        } else {
            if(SOCKET_FD == a) {
                ut_syslog_tests::flags_ |= 0x0004;
            }
            return 0;
        }
    }
};

__uid_t ut_syslog_tests::euid_ = 0; // root
int ut_syslog_tests::syslog_socket_ = SOCKET_FD;
unsigned short ut_syslog_tests::flags_ = 0;
std::vector<std::string> ut_syslog_tests::sysLog_;
size_t ut_syslog_tests::sysLogIndex_ = 0;

void ut_syslog_tests::SetUpTestCase()
{
    mca_sensor_syslog_component.collect_metrics = true;
    // Configure/Create OPAL level resources
    opal_dss_register_vars();
    opal_dss_open();

    euid_ = 0;
    syslog_socket_ = SOCKET_FD;
    flags_ = 0;
    sysLog_.clear();
    sysLogIndex_ = 0;
    sysLog_.push_back("<30>1 2014-07-31T13:47:30.957146+02:00 host1 snmpd 23611 - - Connection from UDP: [127.0.0.1]:58374->[127.0.0.1]");
}

void ut_syslog_tests::TearDownTestCase()
{
    // Release OPAL level resources
    opal_dss_close();

    euid_ = 0;
    syslog_socket_ = SOCKET_FD;
    flags_ = 0;
    sysLog_.clear();
    sysLogIndex_ = 0;
}

ssize_t ut_syslog_tests::GetSyslogEntry(char* buffer, size_t max_size)
{
    if(sysLog_.size() == sysLogIndex_ || NULL == buffer || 0 == max_size) {
        return 0;
    } else {
        string msg = sysLog_[sysLogIndex_++];
        size_t max_len = MIN(max_size-1, msg.size()+1);
        strncpy(buffer, msg.c_str(), max_len);
        buffer[max_len] = '\0';
        return max_len;
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
        int n = 1;
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
        n = 1;
        rv = opal_dss.unpack(&remaining, &log_line, &n, OPAL_STRING);
        if(OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER != rv) {
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


// Testing the data collection class
TEST_F(ut_syslog_tests, syslog_sensor_sample_tests)
{
    // Setup
    void* object = orcm_sensor_base_runtime_metrics_create("syslog", false, false);
    mca_sensor_syslog_component.runtime_metrics = object;
    orcm_sensor_sampler_t* sampler = (orcm_sensor_sampler_t*)OBJ_NEW(orcm_sensor_sampler_t);

    // Tests
    collect_syslog_sample(sampler);
    EXPECT_EQ(0, (mca_sensor_syslog_component.diagnostics & 0x1));

    orcm_sensor_base_runtime_metrics_set(object, true, "syslog");
    collect_syslog_sample(sampler);
    EXPECT_EQ(1, (mca_sensor_syslog_component.diagnostics & 0x1));

    // Cleanup
    OBJ_RELEASE(sampler);
    orcm_sensor_base_runtime_metrics_destroy(object);
    mca_sensor_syslog_component.runtime_metrics = NULL;
}

TEST_F(ut_syslog_tests, syslog_api_tests)
{
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
    int rv = orcm_sensor_syslog_module.init();
    EXPECT_EQ(ORCM_SUCCESS, rv);
    EXPECT_NE(0, (uint64_t)(mca_sensor_syslog_component.runtime_metrics));
    orcm_sensor_syslog_module.finalize();
}

TEST_F(ut_syslog_tests, syslog_init_finalize_negative)
{
    euid_ = 1000;
    int rv = orcm_sensor_syslog_module.init();
    EXPECT_NE(ORCM_SUCCESS, rv);
    EXPECT_EQ(0, (uint64_t)(mca_sensor_syslog_component.runtime_metrics));
    orcm_sensor_syslog_module.finalize();
    euid_ = 0;
}

TEST_F(ut_syslog_tests, syslog_start_stop)
{
    syslog_socket_ = SOCKET_FD;

    orcm_sensor_syslog_module.start(5);
    sleep(1);
    orcm_sensor_syslog_module.stop(5);
    EXPECT_EQ(15, flags_);
    flags_ = 0;
}

TEST_F(ut_syslog_tests, syslog_start_stop_negative)
{
    syslog_socket_ = -1;

    orcm_sensor_syslog_module.start(5);
    EXPECT_EQ(0, flags_);
    orcm_sensor_syslog_module.stop(5);

    flags_ = 0;
    syslog_socket_ = SOCKET_FD;
}

TEST_F(ut_syslog_tests, syslog_sample_rate_tests)
{
    mca_sensor_syslog_component.use_progress_thread = false;
    mca_sensor_syslog_component.sample_rate = 0;
    orcm_sensor_syslog_module.set_sample_rate(5);
    EXPECT_EQ(0, mca_sensor_syslog_component.sample_rate);
    mca_sensor_syslog_component.use_progress_thread = true;
    orcm_sensor_syslog_module.set_sample_rate(5);
    EXPECT_EQ(5, mca_sensor_syslog_component.sample_rate);
    int rate = 0;
    orcm_sensor_syslog_module.get_sample_rate(&rate);
    EXPECT_EQ(5, rate);
    mca_sensor_syslog_component.use_progress_thread = false;
}

TEST_F(ut_syslog_tests, syslog_inventory_sample_log_test)
{
    mca_sensor_syslog_component.use_progress_thread = false;
    // It appears this feature is not implemented in syslog sensor plugin!
}

TEST_F(ut_syslog_tests, syslog_sample_log_test)
{
    mca_sensor_syslog_component.use_progress_thread = false;
    ASSERT_EQ(ORCM_SUCCESS, orcm_sensor_syslog_module.init());
    orcm_sensor_syslog_module.start(6);
    sleep(1);
    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    orcm_sensor_syslog_module.sample(&sampler);
    opal_buffer_t* buffer = &sampler.bucket;

    int n = 1;
    opal_buffer_t* logBuffer = NULL; //OBJ_NEW(opal_buffer_t);
    int rv = opal_dss.unpack(buffer, &logBuffer, &n, OPAL_BUFFER);
    EXPECT_NE(0, (uint64_t)logBuffer);
    bool failed = (ORCM_SUCCESS != rv || NULL == buffer);
    EXPECT_EQ(OPAL_SUCCESS, rv);
    if(!failed)
    {
        char* plugin = NULL;
        n = 1;
        rv = opal_dss.unpack(logBuffer, &plugin, &n, OPAL_STRING);
        failed = (ORCM_SUCCESS != rv || NULL == buffer);
        EXPECT_STREQ("syslog", plugin);
        SAFEFREE(plugin);
    } else {
        SAFE_OBJ_RELEASE(logBuffer);
    }

    if(!failed) {
        EXPECT_TRUE(ValidateSyslogData(logBuffer));
        orcm_sensor_syslog_module.log(logBuffer);
        SAFE_OBJ_RELEASE(logBuffer);
    }

    OBJ_DESTRUCT(&sampler);
    SAFE_OBJ_RELEASE(logBuffer);
    orcm_sensor_syslog_module.stop(6);
    orcm_sensor_syslog_module.finalize();
}
