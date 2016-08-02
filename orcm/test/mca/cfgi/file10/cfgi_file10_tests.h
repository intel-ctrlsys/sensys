/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef CFGI_FILE10_TESTS_H
#define CFGI_FILE10_TESTS_H

#include "gtest/gtest.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    // OPAL
    #include "opal/runtime/opal.h"
    #include "orcm/mca/cfgi/file10/cfgi_file10.h"
#ifdef __cplusplus
};
#endif // __cplusplus
#include "cfgi_file10_mocking.h"

class ut_cfgi_file10_tests: public testing::Test
{
    protected:
        static void SetUpTestCase();
        static void TearDownTestCase();
    public:
        static char* MockedStrdup(const char *s);
        static int ParserOpen (char const *file);
        static int ParserClose (int file_id);
        static opal_list_t* ParserRetrieveDocument (int file_id);
        static char* CopyConfigFileName(char *config_file);

};

extern cfgi_file10_mocking cfgi_file10_mocking_object;

#endif
