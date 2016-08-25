/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef UDSENSORS_TESTS_H
#define UDSENSORS_TESTS_H

#include "gtest/gtest.h"
#include "mockFactory.h"

class ut_udsensors_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
        static void setFullMock(bool mock_status, int nPlugins);
        static bool isOutputError(std::string output);

    public:
        static bool use_pt_;
}; // class

class ut_udsensors_log: public ut_udsensors_tests
{
    protected:
        void* bufferPtr;

        void SetUp();
        void TearDown();
};


class ut_udsensors_sample : public ut_udsensors_tests
{
    protected:
        void SetUp();
        void TearDown();
        void *samplerPtr;
        void *object;
};

class ut_udsensors_init : public ut_udsensors_tests
{
    protected:
        void SetUp();
        void TearDown();
        struct sensorFactory *obj;
        void setMockFailAtInit();
};

#endif // UDSENSORS_TESTS_H
