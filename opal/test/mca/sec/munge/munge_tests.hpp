/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MUNGE_TESTS_H
#define MUNGE_TESTS_H

#include "gtest/gtest.h"

class ut_munge_tests: public testing::Test
{
protected:
    static void SetUpTestCase();
    static void TearDownTestCase();
};


#endif 
