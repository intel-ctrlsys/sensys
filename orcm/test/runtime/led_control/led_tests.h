/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef CONTROL_LED_TESTS_H
#define CONTROL_LED_TESTS_H

#include "gtest/gtest.h"

class ut_control_led_tests: public testing::Test
{
    protected:
        /* gtests */
        virtual void SetUp();
        virtual void TearDown();
};

#endif // CONTROL_LED_TESTS_H
