#include "gtest/gtest.h"

#include <string>
#include <vector>

#include "orcm/mca/dispatch/base/base.h"
#include "orcm/mca/dispatch/dispatch.h"

class ut_dispatch_base_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
        static void reset_module_list();
        static void reset_base_framework_comp();

        static orcm_dispatch_base_module_t test_module;
        static orcm_dispatch_active_module_t* active_module;
}; // class
