/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef UDSENSORS_TESTS_H
#define UDSENSORS_TESTS_H

#include "gtest/gtest.h"

class ut_udsensors_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();

    public:
        static bool use_pt_;
}; // class

#endif // UDSENSORS_TESTS_H
