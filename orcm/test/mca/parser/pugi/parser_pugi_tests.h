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
#include <string.h>
#include <iostream>

#include "orcm/constants.h"
#include "orcm/mca/sensor/base/sensor_private.h"

#include "orcm/mca/parser/pugi/parser_pugi.h"
#include "orcm/mca/parser/pugi/pugi_impl.h"

extern "C" {
    #include "opal/runtime/opal.h"
}

class ut_parser_pugi_tests: public testing::Test {
    protected:
        std::string validFile;
        char const *invalidFile;
        char const *rootKey, *validKey, *invalidKey;
        char const *invalidName, *validName, *validNameTag;
        int numOfValidKey;
        virtual void SetUp(){
            char* srcdir = getenv("srcdir");
            if (NULL != srcdir)
                validFile=std::string(srcdir) + "/example.xml";
            else
                validFile="example.xml";
            invalidFile = "myfile";
            validKey = "group";
            numOfValidKey = 4;
            invalidName = invalidKey = "groupA";
            validName = "group_name1";
            validNameTag = "group_name3";
            rootKey = "root";
            opal_init_test();
        }
        virtual void TearDown() {
        }
};

static int openedFiles = 0;
#endif