/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef VARDATA_TESTS_H
#define VARDATA_TESTS_H

#include "gtest/gtest.h"
#include "orcm/mca/sensor/snmp/vardata.h"
#include "opal/dss/dss.h"
#include "opal/dss/dss_internal.h"
#include "opal/dss/dss_types.h"
#include "opal/runtime/opal.h"
#include <string.h>

using namespace std;

class ut_vardata_constructor_tests: public testing::Test {
    protected:
    virtual void SetUp() {
        opal_init_test();
    }
};


class ut_vardata_tests: public testing::Test {
    protected:
    int32_t testValue;
    vardata *testData;
    string testString;
    vardata *testDataString;

    virtual void SetUp() {
        opal_init_test();
        testValue = (int32_t) 31416;
        testString = string("Hello, World!");
        testData = new vardata(testValue);
        testDataString = new vardata(testString);
    }

    virtual void TearDown() {
        delete testData;
    }

};

class ut_vardataList_tests: public testing::Test {
    protected:
    vector<vardata> probeList;
    opal_buffer_t buffer;

    virtual void SetUp() {
        opal_init_test();
        probeList.push_back(vardata((int64_t) 314159265));
        probeList.push_back(vardata((float) 3.14156));
        probeList.push_back(vardata(string("Hello, World!")));

        OBJ_CONSTRUCT(&buffer, opal_buffer_t);
    }

    virtual void TearDown() {
        OBJ_DESTRUCT(&buffer);
    }
};

#endif
