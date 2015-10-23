#ifndef IPMI_SEL_COLLECTOR_TESTS_H
#define IPMI_SEL_COLLECTOR_TESTS_H

#include "gtest/gtest.h"

class ut_ipmi_sel_collection: public testing::Test
{
    protected: // gtest required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
}; // class

#endif // IPMI_SEL_COLLECTOR_TESTS_H_INCLUDED
