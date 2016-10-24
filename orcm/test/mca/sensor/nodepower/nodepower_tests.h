/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef NODEPOWER_TESTS_H
#define NODEPOWER_TESTS_H

#include "gtest/gtest.h"
#include "nodepower_tests_mocking.h"

#include "orcm/mca/db/db.h"

#include <string>
#include <vector>
#include <queue>

class ut_nodepower_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
        static void ResetTest();

        static int IpmiCmdRaw(unsigned char a,unsigned char b,unsigned char c,
                              unsigned char d,unsigned char e,unsigned char* f,int g,
                              unsigned char* h,int* i,unsigned char* j,unsigned char k);
        static int IpmiCmdRawFail(unsigned char a,unsigned char b,unsigned char c,
                                  unsigned char d,unsigned char e,unsigned char* f,int g,
                                  unsigned char* h,int* i,unsigned char* j,unsigned char k);
        static int IpmiClose(void);
        static void StoreNew(int, orcm_db_data_type_t, opal_list_t* records, opal_list_t*,
                             orcm_db_callback_fn_t cbfunc, void*);
        static __uid_t GetUIDNormal(void);
        static __uid_t GetUIDRoot(void);
        static struct tm* LocalTimeFail(const time_t* tp);

        static int NPInit(void);

        /* Sample ipmi_cmdraw data for PSU2*/
        static int psu02_index;
        static unsigned char psu02[53][7];
        static std::queue<int> ipmi_error_queue;
}; // class

#endif // NODEPOWER_TESTS_H
