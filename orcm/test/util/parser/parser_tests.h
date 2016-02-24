/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PARSER_TESTS_H
#define PARSER_TESTS_H

#include "gtest/gtest.h"
#include "orcm/util/xmlparser.h"
#include <string.h>
#include <iostream>
#include "orcm/constants.h"
#include "orcm/util/parser_api.h"

extern "C" {
    #include "opal/runtime/opal.h"
}

class ut_parser_tests: public testing::Test {
    protected:
        char const* validFile;
        char const* invalidFile;
        char const* invalidKey;
        std::string srcFiledir;

        virtual void SetUp(){
            char* srcdir = getenv("srcdir");
            if (NULL != srcdir)
                srcFiledir=std::string(srcdir) + "/example.xml";
            else
                srcFiledir="example.xml";
            validFile = srcFiledir.c_str();
            invalidFile = "myfile";
            invalidKey = "someInvalidKey";
            opal_init_test();
        }
        virtual void TearDown() {
        }
};

#endif
