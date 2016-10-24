/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCMD_TESTS_H
#define ORCMD_TESTS_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    // OPAL
    #include "opal/dss/dss.h"
    // ORTE
    #include "orte/mca/rml/rml.h"
    // ORCM
    #include "orcm/runtime/orcm_globals.h"
    #include "orcm/tools/orcmd/orcmd_core.h"

#ifdef __cplusplus
};
#endif // __cplusplus

#include "gtest/gtest.h"
#include "orcmd_mocking.h"

class ut_orcmd_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
        static void ResetTestCase();
        static void OrcmInit();
        static void OrcmInitJobid();
        static void OrcmInitVpid();
}; // class
#endif // ORCMD_TESTS_H
