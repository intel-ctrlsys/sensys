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

        static void populate_capsule(ipmi_capsule_t* capsule);
}; // class

#endif // SENSOR_IPMI_TESTS_H
