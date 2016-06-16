/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef UTIL_TESTS_H
#define UTIL_TESTS_H

#include "gtest/gtest.h"
#include "util_tests_mocking.h"

class ut_util_tests: public testing::Test {
    protected:
        // Mocking
        static void OrteErrmgrBaseLog(int err, char* file, int lineno);

        static  void SetUpTestCase();
        virtual void SetUp();
        static  void TearDownTestCase();

        static void on_failure_return(int *value);
        static void on_null_return(void **value);
        static int  on_null_return_error(void *ptr, int error);

        static int last_orte_error_;
};

#endif
