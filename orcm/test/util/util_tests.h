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
#include "orcm/util/string_utils.h"

extern "C" {
    #include "orcm/util/attr.h"
    #include "orcm/util/cli.h"
    #include "orcm/util/logical_group.h"
    #include "orcm/util/utils.h"
    #include "opal/runtime/opal.h"
}

class ut_util_tests: public testing::Test {
    protected:
        static  void SetUpTestCase();
        virtual void SetUp();
        static  void TearDownTestCase();
};

#endif
