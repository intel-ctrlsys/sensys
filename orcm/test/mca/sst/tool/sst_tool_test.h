/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SST_TOOL_TESTS_H
#define SST_TOOL_TESTS_H

#include "gtest/gtest.h"

class ut_sst_tool_tests: public testing::Test
{
protected:
    static void SetUpTestCase();
    static void TearDownTestCase();
    static void InitMockingFlags();
};

#endif  // SST_TOOL_TESTS_H
