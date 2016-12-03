/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMI_TS_TESTS_H
#define IPMI_TS_TESTS_H

#include "gtest/gtest.h"
#include "mockTest.h"

class ut_ipmi_ts_tests: public testing::Test
{
protected:
    static void SetUpTestCase();
    static void TearDownTestCase();
    static void setFullMock(bool mock_status, int nPlugins);
    static bool isOutputError(std::string output);
public:
    static bool use_pt_;
};

class ut_ipmi_ts_init : public ut_ipmi_ts_tests
{
protected:
    void SetUp();
    void TearDown();
    struct ipmiSensorFactory *obj;
    void setMockFailAtInit();
};

class ut_ipmi_ts_sample : public ut_ipmi_ts_tests
{
protected:
    void SetUp();
    void TearDown();
    void *samplerPtr;
    void *object;
};

class ut_ipmi_ts_log: public ut_ipmi_ts_tests
{
    protected:
        void* bufferPtr;
        void SetUp();
        void TearDown();
};

class ut_ipmi_ts_inventory : public ut_ipmi_ts_tests
{
protected:
    void SetUp();
    void TearDown();
    void *samplerPtr;
    void *object;
};

class ut_ipmi_ts_inventory_log : public ut_ipmi_ts_tests
{
protected:
    void SetUp();
    void TearDown();
    void *bufferPtr;
    void *object;
};

#endif
