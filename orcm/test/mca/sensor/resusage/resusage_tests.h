/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef RESUSAGE_TESTS_H
#define RESUSAGE_TESTS_H

#include "gtest/gtest.h"

#include <string>
#include <vector>

class ut_resusage_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
}; // class

#endif // RESUSAGE_TESTS_H
