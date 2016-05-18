/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef FREQ_TESTS_H
#define FREQ_TESTS_H

#include "gtest/gtest.h"

#include <sys/types.h>
#include <dirent.h>

#include <string>
#include <vector>

#include "orcm/mca/db/db.h"
#include "orcm/mca/analytics/analytics.h"

class ut_freq_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();

        static DIR* OpenDir(const char*);
        static int CloseDir(DIR*);
        static struct dirent* ReadDir(DIR*);
}; // class

#endif // FREQ_TESTS_H
