/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "munge_tests.hpp"

#include "opal/constants.h"
#include "opal/mca/sec/base/base.h"

extern opal_sec_base_module_t opal_sec_munge_module;

void ut_munge_tests::SetUpTestCase()
{
    return;
}

void ut_munge_tests::TearDownTestCase()
{
    return;
}

TEST_F(ut_munge_tests, dummy)
{
    int rc = opal_sec_munge_module.init();
    EXPECT_EQ(OPAL_ERR_SERVER_NOT_AVAIL, rc);
}
