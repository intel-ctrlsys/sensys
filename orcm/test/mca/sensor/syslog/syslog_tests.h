/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SYSLOG_TESTS_H
#define SYSLOG_TESTS_H

#include "gtest/gtest.h"

#include "opal/dss/dss.h"

#include <stddef.h>

#include <string>
#include <vector>

class ut_syslog_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
        static void ResetTestCase();

    public:
        static ssize_t GetSyslogEntry(char* buffer, size_t max_size);
        static bool ValidateSyslogData(opal_buffer_t* buffer);
        static void Cleanup(void* sampler, void* logBuffer, int jobid);
        static void ExpectCorrectSeverityAndFacility(int val, const char* sev, const char* fac);
        static void AssertCorrectPerThreadStartup(bool use_pt);

        static __uid_t euid_;
        static int syslog_socket_;
        static unsigned short flags_;
        static std::vector<std::string> sysLog_;
        static size_t sysLogIndex_;
        static bool use_pt_;
        static bool bad_syslog_entry_;
}; // class

#endif // SYSLOG_TESTS_H
