/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef DB_BASE_TEST_H
#define DB_BASE_TEST_H

#include "gtest/gtest.h"

class ut_db_base_utils_tests: public testing::Test
{
protected:
    static void SetUpTestCase();
    static void TearDownTestCase();
};

#endif	// DB_BASE_TEST_H
