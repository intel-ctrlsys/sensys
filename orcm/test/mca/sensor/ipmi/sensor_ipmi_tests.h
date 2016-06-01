/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SENSOR_IPMI_TESTS_H
#define SENSOR_IPMI_TESTS_H

#include "gtest/gtest.h"


extern "C" {
    #include "opal/class/opal_list.h"
    #include "orcm/mca/sensor/ipmi/sensor_ipmi_decls.h"
    #include "orcm/mca/sensor/ipmi/sensor_ipmi.h"
};

class ut_sensor_ipmi_tests: public testing::Test
{
    protected: // gtest required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
        virtual void SetUp();

    public: // Helper Functions
        static void populate_capsule(ipmi_capsule_t* capsule);
        static bool WildcardCompare(const char* value, const char* pattern);
}; // class

#endif // SENSOR_IPMI_TESTS_H
