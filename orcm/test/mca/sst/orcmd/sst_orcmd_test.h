/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SST_ORCMD_TESTS_H
#define SST_ORCMD_TESTS_H

#include "gtest/gtest.h"

class ut_sst_orcmd_tests: public testing::Test
{
protected:
    static void InitMockingFlags();
};

#endif  // SST_ORCMD_TESTS_H
