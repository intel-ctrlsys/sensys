/*
 * Copyright (c) 2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef DB_POSTGRES_TEST_H
#define DB_POSTGRES_TEST_H

#include "gtest/gtest.h"

class ut_db_postgres_tests: public testing::Test
{
protected:
    static void SetUpTestCase();
    static void TearDownTestCase();
};

#endif	// DB_POSTGRES_TEST_H
