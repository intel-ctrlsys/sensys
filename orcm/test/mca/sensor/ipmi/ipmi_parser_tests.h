/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMI_PARSER_TESTS_H
#define IPMI_PARSER_TESTS_H

#define MY_MODULE_PRIORITY 20
#define NUM_NODES 6

#include "gtest/gtest.h"
#include <string>

extern "C" {
    #include "orcm/mca/parser/base/base.h"
    #include "orcm/mca/parser/pugi/parser_pugi.h"
    #include "orcm/util/utils.h"
}

using namespace std;

class ut_ipmi_parser: public testing::Test
{
    protected:
        virtual void SetUp()
        {
            initParserFramework();
            setFileName();
            invalidFile="myfile.xml";
        }

        virtual void TearDown()
        {
            cleanParserFramework();
        }
        void initParserFramework();
        void cleanParserFramework();
        void setFileName();

        string fileName;
        string invalidFile;
};

#endif // IPMI_PARSER_TESTS_H
