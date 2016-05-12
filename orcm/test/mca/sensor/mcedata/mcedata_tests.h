/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCEDATA_TESTS_H
#define MCEDATA_TESTS_H

#include "gtest/gtest.h"

#include <sys/types.h>
#include <dirent.h>

#include <string>
#include <vector>

#include "orcm/mca/db/db.h"
#include "orcm/mca/analytics/analytics.h"

class ut_mcedata_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();

        static DIR* OpenDir(const char*);
        static int CloseDir(DIR*);
        static struct dirent* ReadDir(DIR*);

        static void StoreNew(int dbhandle, orcm_db_data_type_t data_type,
                             opal_list_t *input, opal_list_t *ret,
                             orcm_db_callback_fn_t cbfunc, void *cbdata);
        static void SendData(orcm_analytics_value_t *analytics_vals);
        static int FSeekError(FILE*,long, int);
}; // class

#endif // MCEDATA_TESTS_H
