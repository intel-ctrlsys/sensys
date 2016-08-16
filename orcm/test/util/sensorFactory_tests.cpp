/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sensorFactory_tests.h"

void ut_sensorFactory::SetUp()
{
    mock_dlopen = false;
    mock_dlsym = false;
    mock_dlerror = false;
    mock_plugin = false;
    obj = sensorFactory::getInstance();
}

void ut_sensorFactory::TearDown()
{
    obj->pluginFilesFound.clear();
    obj->pluginsLoaded.clear();
    obj->close();
}

int mockPlugin::init()
{
    std::cout << "In the init\n";
    if (throwOnInit) {
        throw std::runtime_error("Failing on mockPlugin.init");
    }
    return 0;
}

int mockPlugin::finalize()
{
    return 0;
}

TEST_F(ut_sensorFactory, openWithNullParamsAndExpectDefaultPath)
{
    std::string path;
    const char *default_path = "/usr/lib64/";
    EXPECT_NO_THROW(obj->open(NULL, NULL));
    path = obj->getPluginsPath();
    EXPECT_EQ(0, path.compare(default_path));
}

TEST_F(ut_sensorFactory, openWithNullParamsAndExpectDefaultPrefix)
{
    std::string prefix;
    const char *default_prefix = "libudplugin_";
    EXPECT_NO_THROW(obj->open(NULL, NULL));
    prefix = obj->getPluginsPrefix();
    EXPECT_EQ(0, prefix.compare(default_prefix));
}

TEST_F(ut_sensorFactory, openWithGoodParamsAndExpectSameValues)
{
    std::string prefix;
    std::string path;
    const char *myprefix = "myprefix_";
    const char *mypath = "/etc/";

    EXPECT_NO_THROW(obj->open(mypath, myprefix));
    prefix = obj->getPluginsPrefix();
    path = obj->getPluginsPath();

    EXPECT_EQ(0, prefix.compare(myprefix));
    EXPECT_EQ(0, path.compare(mypath));
}

TEST_F(ut_sensorFactory, openAndCheckTrailingSlashOnPath)
{
    std::string path;
    EXPECT_THROW(obj->open("/my/path", NULL), sensorFactoryException);
    path = obj->getPluginsPath();
    EXPECT_EQ(0, path.compare("/my/path/"));
}

TEST_F(ut_sensorFactory, openPluginsOnDefaultPathWithExistingFiles)
{
    mock_dlopen = true;
    EXPECT_NO_THROW(obj->open(NULL, "lib"));
    EXPECT_TRUE(obj->pluginFilesFound.size() != 0);
    mock_dlopen = false;
}

TEST_F(ut_sensorFactory, openPluginsOnDefaultPathAndCheckFullPathInVector)
{
    mock_dlopen = true;
    EXPECT_NO_THROW(obj->open(NULL, "lib"));
    const char *default_path = "/usr/lib64/lib";
    ASSERT_TRUE(obj->pluginFilesFound.size() != 0);
    std::vector<std::string>::iterator it;
    for (it = obj->pluginFilesFound.begin(); it != obj->pluginFilesFound.end(); ++it) {
        EXPECT_TRUE(NULL != strstr(it->c_str(), default_path));
    }
    mock_dlopen = false;
}

TEST_F(ut_sensorFactory, openPluginsWithNonExistingPathAndGetException)
{
    const char *mypath = "/my/funny/path/";
    std::string str = "Cannot open directory " + std::string (mypath);
    try {
        obj->open("/my/funny/path", NULL);
    } catch (sensorFactoryException& e) {
        EXPECT_EQ(0, str.compare(e.what()));
    }
}

TEST_F(ut_sensorFactory, openPluginsAndCheckLoadedPluginsOnPathWithoutValidPlugins)
{
    EXPECT_NO_THROW(obj->open(NULL, NULL));
    EXPECT_EQ(0, obj->getFoundPlugins());
}

TEST_F(ut_sensorFactory, openPluginLoadOneValidPlugin)
{
    EXPECT_NO_THROW(obj->open("/tmp/", NULL));
    EXPECT_EQ(1, obj->getFoundPlugins());
}

TEST_F(ut_sensorFactory, openPluginAndCheckLoadedHandlers)
{
    EXPECT_NO_THROW(obj->open("/tmp/", NULL));
    EXPECT_EQ(1, obj->getAmountOfPluginHandlers());
}

TEST_F(ut_sensorFactory, openPluginWithoutValidSymbol)
{
    mock_dlsym = true;
    EXPECT_NO_THROW(obj->open("/tmp/", NULL));
    mock_dlsym = false;
}

TEST_F(ut_sensorFactory, openPluginWithoutValidSymbolAndDlErrorisNull)
{
    mock_dlsym = true;
    mock_dlerror = true;
    EXPECT_NO_THROW(obj->open("/tmp/", NULL));
    mock_dlsym = false;
    mock_dlerror = false;
}

TEST_F(ut_sensorFactory, openValidPluginAndClose)
{
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    obj->close();
    EXPECT_EQ(0, obj->getLoadedPlugins());
}

TEST_F(ut_sensorFactory, openInvalidPluginAndClose)
{
    mock_dlopen = true;
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    obj->close();
    EXPECT_EQ(0, obj->getLoadedPlugins());
    mock_dlopen = false;
}

TEST_F(ut_sensorFactory, getAmountOfFoundPlugins)
{
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    EXPECT_EQ(1, obj->getFoundPlugins());
}

TEST_F(ut_sensorFactory, getAmountOfLoadedPlugins)
{
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    EXPECT_EQ(1, obj->getLoadedPlugins());
}

TEST_F(ut_sensorFactory, getAmountOfLoadedPluginsWithInvalidPointer)
{
    mock_dlsym = true;
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    EXPECT_EQ(0, obj->getLoadedPlugins());
    mock_dlsym = false;
}

TEST_F(ut_sensorFactory, initWithoutErrors)
{
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    EXPECT_NO_THROW(obj->init());
}

TEST_F(ut_sensorFactory, initWithPluginException)
{
    mock_dlsym = true;
    mock_plugin = true;
    throwOnInit = true;
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    EXPECT_THROW(obj->init(), std::runtime_error);
    mock_plugin = false;
    mock_dlsym = false;
    throwOnInit = false;
}

TEST_F(ut_sensorFactory, initWithPluginExceptionAndErrorMsg)
{
    mock_dlsym = true;
    mock_plugin = true;
    throwOnInit = true;
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    try {
        obj->init();
    } catch (std::runtime_error& e) {
        EXPECT_TRUE(NULL != strstr(e.what(), "Failing on mockPlugin.init"));
    }
    mock_dlsym = true;
    mock_plugin = true;
    throwOnInit = false;
}
