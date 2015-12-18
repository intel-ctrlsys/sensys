/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
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

class ut_snmp_parser_tests: public testing::Test {

    protected:
        ut_snmp_parser_tests(){
        }

        virtual void SetUp(){
            opal_init_test();
        }

        virtual void TearDown(){
        }
};

#endif
