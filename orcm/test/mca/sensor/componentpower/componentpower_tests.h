/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef COMPONENTPOWER_TESTS_H
#define COMPONENTPOWER_TESTS_H

#include "gtest/gtest.h"

#include "opal/dss/dss_types.h"
#include "opal/mca/event/event.h"

#include <string>
#include <vector>

class ut_componentpower_tests: public testing::Test
{
    protected:
        // gtest fixture required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
        virtual void SetUp();

        //Mocking Functions
        static void  OrteErrmgrBaseLog(int err, char* file, int lineno);
        static FILE* FOpen(const char * filename, const char * mode);
        static int   OpenReturnError(char *filename, int access, int permission);
        static int   Open(char *filename, int access, int permission);
        static int   Read(int handle, void *buffer, int nbyte);
        static int   ReadReturnError(int handle, void *buffer, int nbyte);
        static opal_event_base_t* OpalProgressThreadInitReturnNULL(const char*name);
        static void  setInitTestFunctions();
        static void  resetTestFunctions();

        static int last_orte_error_;
}; // class

#endif // COMPONENTPOWER_TESTS_H
