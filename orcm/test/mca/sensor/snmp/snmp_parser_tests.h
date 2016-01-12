/*
 * Copyright (c) 2015-2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SNMP_PARSER_TESTS_H
#define SNMP_PARSER_TESTS_H

#include "gtest/gtest.h"
#include "orcm/mca/sensor/snmp/snmp_parser.h"
extern "C"{
    #include "opal/runtime/opal.h"
}
#include <string.h>
#include <iostream>

class ut_snmp_parser_tests: public testing::Test {
    protected:
        std::string test_files_dir;
        ut_snmp_parser_tests(){
            // 'make check' will provide $(srcdir) with the path to
            // test source folder.
            char* srcdir = getenv("srcdir");
            if (NULL != srcdir)
                test_files_dir = std::string(srcdir) + "/test_files/";
            else
                test_files_dir = "test_files/";
        }

        virtual void SetUp(){
            opal_init_test();
        }

        virtual void TearDown(){
        }
};

#endif
