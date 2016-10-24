/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef CFGI_FILE30_TESTS_H
#define CFGI_FILE30_TESTS_H

#include "gtest/gtest.h"
#include <string>
using namespace std;

class ut_cfgi30_tests: public testing::Test
{
protected:
    virtual void SetUp()
    {
        errno = 0;
        set_file_path();
    }

    static void InitMockingFlags();
    const char* set_name(const char *name);
    void set_file_path();
    string file_path;
};

#endif  // CFGI_FILE30_TESTS_H
