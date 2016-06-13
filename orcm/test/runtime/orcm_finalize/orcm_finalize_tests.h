/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_FINALIZE_TESTS_H
#define ORCM_FINALIZE_TESTS_H

#include "gtest/gtest.h"

class ut_orcm_finalize_tests: public testing::Test
{
    protected:
        static int OrteEssFinalize(void);
};

#endif
