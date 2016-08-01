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
    obj = sensorFactory::getInstance();
}

void ut_sensorFactory::TearDown()
{
    obj->pluginFilesFound.clear();
    obj->pluginsLoaded.clear();
}

TEST_F(ut_sensorFactory, openWithNullParamsAndExpectDefaultPath)
{
    std::string path;
    const char *default_path = "/usr/lib/";
    obj->open(NULL, NULL);
    path = obj->getPluginsPath();
    EXPECT_EQ(0, path.compare(default_path));
}

TEST_F(ut_sensorFactory, openWithNullParamsAndExpectDefaultPrefix)
{
    std::string prefix;
    const char *default_prefix = "libudplugin_";
    obj->open(NULL, NULL);
    prefix = obj->getPluginsPrefix();
    EXPECT_EQ(0, prefix.compare(default_prefix));
}

TEST_F(ut_sensorFactory, openWithGoodParamsAndExpectSameValues)
{
    std::string prefix;
    std::string path;
    const char *myprefix = "myprefix_";
    const char *mypath = "/etc/";

    obj->open(mypath, myprefix);
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
    obj->open(NULL, "lib");
    EXPECT_TRUE(obj->pluginFilesFound.size() != 0);
}

TEST_F(ut_sensorFactory, openPluginsOnDefaultPathAndCheckFullPathInVector)
{
    obj->open(NULL, "lib");
    const char *default_path = "/usr/lib/lib";
    ASSERT_TRUE(obj->pluginFilesFound.size() != 0);
    std::vector<std::string>::iterator it;
    for (it = obj->pluginFilesFound.begin(); it != obj->pluginFilesFound.end(); ++it) {
        EXPECT_TRUE(NULL != strstr(it->c_str(), default_path));
    }
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
    obj->open(NULL, NULL);
    EXPECT_EQ(0, obj->pluginsLoaded.size());
}

TEST_F(ut_sensorFactory, openPluginLoadOneValidPlugin)
{
    obj->open("/tmp/", NULL);
    EXPECT_EQ(1, obj->pluginsLoaded.size());
}

TEST_F(ut_sensorFactory, openPluginWithoutValidSymbol)
{
    mock_dlsym = true;
    EXPECT_THROW(obj->open("/tmp/", NULL), sensorFactoryException);
    mock_dlsym = false;
}

TEST_F(ut_sensorFactory, openPluginWithoutValidSymbolAndDlErrorisNull)
{
    mock_dlsym = true;
    mock_dlerror = true;
    EXPECT_THROW(obj->open("/tmp/", NULL), sensorFactoryException);
    mock_dlsym = false;
    mock_dlerror = false;
}
