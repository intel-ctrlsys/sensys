/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "gtest/gtest.h"
#include "analytics_factory_test.h"
#include "analytics_factory_test_mocking.h"

#include "orcm/mca/analytics/base/analytics_private.h"

struct dirent dir = {0, 0, 0, '0', "DummyPlugin1"};

DummyPlugin::DummyPlugin()
{

}

DummyPlugin::~DummyPlugin()
{

}

int DummyPlugin::analyze(DataSet& data_set)
{
    return ANALYTICS_SUCCESS;
}

void DummyPlugin::entry_func(void)
{
    return;
}

Analytics* DummyPlugin::creator(void)
{
    Analytics *analytics = new DummyPlugin();
    return analytics;
}

bool AnalyticsFactoryTest::readdir_success;
bool AnalyticsFactoryTest::dlopen_success;
bool AnalyticsFactoryTest::dlerror_success;

void AnalyticsFactoryTest::SetUpTestCase()
{
    return;
}

void AnalyticsFactoryTest::TearDownTestCase()
{
    return;
}

void AnalyticsFactoryTest::reset_var()
{
    readdir_success = false;
    dlopen_success = false;
    dlerror_success = false;
}

struct dirent* AnalyticsFactoryTest::readdir_mock(DIR* dirstream)
{
    if (readdir_success) {
        readdir_success = false;
        return &dir;
    }
    return NULL;
}

void* AnalyticsFactoryTest::dlopen_mock(const char *__file, int __mode)
{
    const char *output = "DummyOutput";
    if (dlopen_success) {
        return (void*)output;
    }
    return NULL;
}

char* AnalyticsFactoryTest::dlerror_mock()
{
    std::string str = "This is an error message";
    if (dlerror_success) {
        dlerror_success = false;
        return (char*)(str.c_str());
    } else {
        dlerror_success = true;
        return NULL;
    }
}

void* AnalyticsFactoryTest::dlsym_mock(void *__restrict __handle, const char *__restrict __name)
{
    return (void*)(&(DummyPlugin::entry_func));
}

int AnalyticsFactoryTest::dlclose_mock(void *__handle)
{
    return ANALYTICS_SUCCESS;
}

static void reset_mocking()
{
    analytics_factory_mocking.readdir_callback = NULL;
    analytics_factory_mocking.dlopen_callback = NULL;
    analytics_factory_mocking.dlerror_callback = NULL;
    analytics_factory_mocking.dlsym_callback = NULL;
    analytics_factory_mocking.dlclose_callback = NULL;
}

TEST_F(AnalyticsFactoryTest, get_instance)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    ASSERT_TRUE(NULL != factory);
}

TEST_F(AnalyticsFactoryTest, cleanup)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    factory->cleanup();
    ASSERT_EQ(factory->pluginFilesFound.size(), 0);
}

TEST_F(AnalyticsFactoryTest, set_plugin_creator)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    factory->setPluginCreator("dummy_plugin", NULL);
    bool exist = factory->checkPluginExisit("dummy_plugin");
    factory->cleanup();
    ASSERT_TRUE(exist);
}

TEST_F(AnalyticsFactoryTest, create_plugin_match_name)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    factory->setPluginCreator("DummyPlugin", DummyPlugin::creator);
    Analytics *analytics = factory->createPlugin("DummyPlugin");
    dataContainer result;
    std::list<Event> events;
    DataSet data_set(result, events);
    int rc = analytics->analyze(data_set);
    delete analytics;
    factory->cleanup();
    ASSERT_EQ(ANALYTICS_SUCCESS, rc);
}

TEST_F(AnalyticsFactoryTest, create_plugin_not_match_name)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    factory->setPluginCreator("DummyPlugin", DummyPlugin::creator);
    Analytics *analytics = factory->createPlugin("AnotherDummyPlugin");
    factory->cleanup();
    ASSERT_TRUE(NULL == analytics);
}

TEST_F(AnalyticsFactoryTest, check_plugin_exist_false)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    factory->setPluginCreator("DummyPlugin", DummyPlugin::creator);
    bool exist = check_plugin_exist("RandomPlugin");
    factory->cleanup();
    ASSERT_EQ(exist, false);
}

TEST_F(AnalyticsFactoryTest, set_plugin_creator_null)
{
    int rc = search_plugin_creator(NULL, NULL);
    ASSERT_EQ(ANALYTICS_SUCCESS, rc);
}

TEST_F(AnalyticsFactoryTest, search_no_such_directory)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    int rc = factory->search("random_directory", "");
    factory->cleanup();
    ASSERT_EQ(-ENOTDIR, rc);
}

TEST_F(AnalyticsFactoryTest, empty_plugin_name_dlopen_null)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    analytics_factory_mocking.readdir_callback = AnalyticsFactoryTest::readdir_mock;
    AnalyticsFactoryTest::readdir_success = true;
    analytics_factory_mocking.dlopen_callback = AnalyticsFactoryTest::dlopen_mock;
    AnalyticsFactoryTest::dlopen_success = false;
    analytics_factory_mocking.dlerror_callback = AnalyticsFactoryTest::dlerror_mock;
    AnalyticsFactoryTest::dlerror_success = true;
    int rc = factory->search("/tmp", "DummyPlugin");
    factory->cleanup();
    reset_mocking();
    AnalyticsFactoryTest::reset_var();
    ASSERT_EQ(ANALYTICS_ERROR, rc);
}

TEST_F(AnalyticsFactoryTest, match_plugin_name_init_plugin_failure)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    analytics_factory_mocking.readdir_callback = AnalyticsFactoryTest::readdir_mock;
    AnalyticsFactoryTest::readdir_success = true;
    analytics_factory_mocking.dlopen_callback = AnalyticsFactoryTest::dlopen_mock;
    AnalyticsFactoryTest::dlopen_success = true;
    analytics_factory_mocking.dlsym_callback = AnalyticsFactoryTest::dlsym_mock;
    analytics_factory_mocking.dlerror_callback = AnalyticsFactoryTest::dlerror_mock;
    AnalyticsFactoryTest::dlerror_success = true;
    analytics_factory_mocking.dlclose_callback = AnalyticsFactoryTest::dlclose_mock;
    int rc = factory->search("/tmp", "DummyPlugin");
    factory->cleanup();
    reset_mocking();
    AnalyticsFactoryTest::reset_var();
    ASSERT_EQ(ANALYTICS_ERROR, rc);
}

TEST_F(AnalyticsFactoryTest, match_plugin_name_init_plugin_success)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    analytics_factory_mocking.readdir_callback = AnalyticsFactoryTest::readdir_mock;
    AnalyticsFactoryTest::readdir_success = true;
    analytics_factory_mocking.dlopen_callback = AnalyticsFactoryTest::dlopen_mock;
    AnalyticsFactoryTest::dlopen_success = true;
    analytics_factory_mocking.dlerror_callback = AnalyticsFactoryTest::dlerror_mock;
    AnalyticsFactoryTest::dlerror_success = false;
    analytics_factory_mocking.dlsym_callback = AnalyticsFactoryTest::dlsym_mock;
    analytics_factory_mocking.dlclose_callback = AnalyticsFactoryTest::dlclose_mock;
    int rc = factory->search("/tmp", "DummyPlugin");
    factory->cleanup();
    reset_mocking();
    AnalyticsFactoryTest::reset_var();
    ASSERT_EQ(ANALYTICS_SUCCESS, rc);
}

TEST_F(AnalyticsFactoryTest, get_plugin_name_match)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    factory->setPluginCreator("DummyPlugin", DummyPlugin::creator);
    const char *module_name = orcm_analytics_get_plugin_name("DummyPlugin");
    factory->cleanup();
    ASSERT_STREQ(module_name, "extension");
}

TEST_F(AnalyticsFactoryTest, get_plugin_name_not_match)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    factory->setPluginCreator("DummyPlugin", DummyPlugin::creator);
    const char *module_name = orcm_analytics_get_plugin_name("filter");
    factory->cleanup();
    ASSERT_STREQ(module_name, "filter");
}

TEST_F(AnalyticsFactoryTest, close_clean_plugin)
{
    AnalyticsFactory *factory = AnalyticsFactory::getInstance();
    factory->setPluginCreator("DummyPlugin", DummyPlugin::creator);
    close_clean_plugin();
    ASSERT_EQ(factory->pluginFilesFound.size(), 0);
}
