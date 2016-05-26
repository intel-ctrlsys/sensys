/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef BASE_TESTS_H
#define BASE_TESTS_H

#include "gtest/gtest.h"

#include <string>
#include <vector>

#include "opal/dss/dss.h"

#include "orcm/mca/sensor/sensor.h"
#include "orcm/mca/sensor/base/sensor_private.h"

class ut_base_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();

        static int sampling_callback(const char* spec);

        static bool sampling_result;
        static orcm_sensor_base_module_t test_module;
        static orcm_sensor_active_module_t* active_module;
}; // class

#endif // BASE_TESTS_H
