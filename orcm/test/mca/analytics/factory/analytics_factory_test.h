/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_TEST_MCA_ANALYTICS_FACTORY_ANALYTICS_FACTORY_TEST_H_
#define ORCM_TEST_MCA_ANALYTICS_FACTORY_ANALYTICS_FACTORY_TEST_H_

#include "orcm/mca/analytics/base/analytics_factory.h"
#include "orcm/mca/analytics/analytics_interface.h"
#include "orcm/mca/analytics/base/c_analytics_factory.h"
#include <dirent.h>

class DummyPlugin : public Analytics {
    public:
        DummyPlugin();
        virtual ~DummyPlugin();
        int analyze(DataSet& data_set);
        static Analytics* creator(void);
        static void entry_func(void);
    private:
        DummyPlugin(DummyPlugin const &);
};

class AnalyticsFactoryTest: public testing::Test
{
    public:
        static bool readdir_success;
        static bool dlopen_success;
        static bool dlerror_success;
        static bool dlsym_success;

        static void SetUpTestCase();
        static void TearDownTestCase();
        static void reset_var();
        static struct dirent* readdir_mock(DIR*);
        static void* dlopen_mock(const char*, int);
        static char* dlerror_mock(void);
        static void* dlsym_mock(void *__restrict, const char *__restrict);
        static int dlclose_mock(void*);
};
#endif /* ORCM_TEST_MCA_ANALYTICS_FACTORY_ANALYTICS_FACTORY_TEST_H_ */
