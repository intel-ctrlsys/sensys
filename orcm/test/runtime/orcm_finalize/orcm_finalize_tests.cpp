/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_finalize_tests.h"
#include "orcm/constants.h"

extern "C" {
    #include "orcm/runtime/runtime.h"
    #include "orte/mca/ess/ess.h"
}

using namespace std;

int ut_orcm_finalize_tests::OrteEssFinalize(void)
{
    return ORCM_SUCCESS;
}

TEST_F(ut_orcm_finalize_tests, orcm_initialized_zero)
{
    int saved = orcm_initialized;
    orcm_initialized = 0;

    EXPECT_EQ(ORCM_ERROR, orcm_finalize());

    orcm_initialized = saved;
}

TEST_F(ut_orcm_finalize_tests, orcm_initialized_one)
{
    int saved = orcm_initialized;
    orcm_initialized = 1;

    EXPECT_EQ(ORCM_SUCCESS, orcm_finalize());

    orcm_initialized = saved;
}

TEST_F(ut_orcm_finalize_tests, orcm_initialized_mock_orte_ess_finalize)
{
    int saved = orcm_initialized;
    orte_ess_base_module_finalize_fn_t saved_fn = orte_ess.finalize;
    orte_ess.finalize = OrteEssFinalize;
    orcm_initialized = 1;

    EXPECT_EQ(ORCM_SUCCESS, orcm_finalize());

    orcm_initialized = saved;
    orte_ess.finalize = saved_fn;
}

TEST_F(ut_orcm_finalize_tests, orcm_initialized_bigger_than_one)
{
    int saved = orcm_initialized;
    orcm_initialized = 2;

    EXPECT_EQ(ORCM_ERROR, orcm_finalize());

    orcm_initialized = saved;
}
