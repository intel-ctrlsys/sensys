/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PARSER_BASE_TESTS_H
#define PARSER_BASE_TESTS_H

#include "gtest/gtest.h"

#include <string>
#include <vector>

#include "orcm/mca/parser/base/base.h"
#include "orcm/mca/parser/parser.h"

class ut_parser_base_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
        static void reset_module_list();

        static orcm_parser_base_module_t test_module;
        static orcm_parser_active_module_t* active_module;
}; // class

#endif // PARSER_BASE_TESTS_H
