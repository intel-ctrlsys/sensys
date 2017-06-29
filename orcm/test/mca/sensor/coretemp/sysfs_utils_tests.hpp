/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SYSFS_UTILS_TESTS_H
#define SYSFS_UTILS_TESTS_H

#include "gtest/gtest.h"

class ut_sysfs_utils_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
}; // class

#endif // SYSFS_UTILS_TESTS_H
