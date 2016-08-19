/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sensorFactory_tests.h"

#define N_MOCKED_PLUGINS 1

void ut_sensorFactory::SetUp()
{
    setFullMock(false, 0);
    throwOnInit = false;
    throwOnSample = false;
    emptyContainer = false;
    obj = sensorFactory::getInstance();
}

void ut_sensorFactory::TearDown()
{
    obj->pluginFilesFound.clear();
    obj->close();
}

void ut_sensorFactory::setFullMock(bool mockStatus, int nPlugins)
{
    mock_readdir = mockStatus;
    n_mocked_plugins = nPlugins;
    mock_dlopen = mockStatus;
    mock_dlclose = mockStatus;
    mock_dlsym = mockStatus;
    mock_plugin = mockStatus;
    getPluginNameMock = mockStatus;
    initPluginMock = mockStatus;
    dlopenReturnHandler = mockStatus;
}

int mockPlugin::init()
{
    if (throwOnInit) {
        throw std::runtime_error("Failing on mockPlugin.init");
    }
    return 0;
}

int mockPlugin::finalize()
{
    return 0;
}

void mockPlugin::sample(dataContainer &dc)
{
    if (throwOnSample) {
        throw std::runtime_error("Failing on mockPlugin.sample");
    }
    if (!emptyContainer) {
        dc.put("intValue_1", 1234, "ints");
    }
    return;
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
    EXPECT_NO_THROW(obj->open(NULL, "libc"));
    const char *default_path = "/usr/lib64/libc";
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
    setFullMock(true, N_MOCKED_PLUGINS);
    EXPECT_NO_THROW(obj->open("/tmp/", NULL));
    EXPECT_EQ(N_MOCKED_PLUGINS, obj->getFoundPlugins());

}

TEST_F(ut_sensorFactory, openPluginAndCheckLoadedHandlers)
{
    setFullMock(true, N_MOCKED_PLUGINS);
    EXPECT_NO_THROW(obj->open("/tmp/", NULL));
    EXPECT_EQ(N_MOCKED_PLUGINS, obj->getAmountOfPluginHandlers());
}

TEST_F(ut_sensorFactory, openPluginWithoutValidInitSymbol)
{
    setFullMock(true, N_MOCKED_PLUGINS);
    initPluginMock = false;
    EXPECT_NO_THROW(obj->open("/tmp/", NULL));
}

TEST_F(ut_sensorFactory, openPluginWithoutValidNameSymbol)
{
    setFullMock(true, N_MOCKED_PLUGINS);
    getPluginNameMock = false;
    EXPECT_NO_THROW(obj->open("/tmp/", NULL));
}

TEST_F(ut_sensorFactory, openPluginWithoutValidSymbols)
{
    setFullMock(true, N_MOCKED_PLUGINS);
    mock_plugin = false;
    EXPECT_NO_THROW(obj->open("/tmp/", NULL));
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
    setFullMock(true, N_MOCKED_PLUGINS);
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    obj->close();
    EXPECT_EQ(0, obj->getLoadedPlugins());
}

TEST_F(ut_sensorFactory, openInvalidPluginAndClose)
{
    setFullMock(true, N_MOCKED_PLUGINS);
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    obj->close();
    EXPECT_EQ(0, obj->getLoadedPlugins());
}

TEST_F(ut_sensorFactory, getAmountOfLoadedPlugins)
{
    setFullMock(true, N_MOCKED_PLUGINS);
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    EXPECT_EQ(N_MOCKED_PLUGINS, obj->getLoadedPlugins());
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
    setFullMock(true, N_MOCKED_PLUGINS);
    throwOnInit = true;
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    EXPECT_THROW(obj->init(), std::runtime_error);
    throwOnInit = false;
}

TEST_F(ut_sensorFactory, initWithPluginExceptionAndErrorMsg)
{
    setFullMock(true, N_MOCKED_PLUGINS);
    throwOnInit = true;
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    try {
        obj->init();
    } catch (std::runtime_error& e) {
        EXPECT_TRUE(NULL != strstr(e.what(), "Failing on mockPlugin.init"));
    }
    throwOnInit = false;
}

TEST_F(ut_sensorFactory, callSampleOnPluginsAndFillMap)
{
    dataContainer cnt;
    dataContainerMap res;
    std::string expected_units = "ints";
    int expected_value = 1234;

    setFullMock(true, N_MOCKED_PLUGINS);
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    EXPECT_NO_THROW(obj->init());
    EXPECT_NO_THROW(obj->sample(res));
    EXPECT_EQ(N_MOCKED_PLUGINS, res.size());
    std::string plugin_name = res.begin()->first;
    EXPECT_EQ(0, plugin_name.compare("MyFakePlugin"));

    cnt = res.begin()->second;
    EXPECT_EQ(0, expected_units.compare(cnt.getUnits("intValue_1")));
    EXPECT_TRUE(expected_value == cnt.getValue<int>("intValue_1"));
}

TEST_F(ut_sensorFactory, callSampleWithoutDataAndExpectEmptyMap)
{
    dataContainerMap res;
    emptyContainer = true;
    setFullMock(true, N_MOCKED_PLUGINS);
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    EXPECT_NO_THROW(obj->init());
    EXPECT_NO_THROW(obj->sample(res));
    EXPECT_EQ(0, res.size());
    emptyContainer = true;
}

TEST_F(ut_sensorFactory, callSampleOnPluginAndReceiveException)
{
    dataContainerMap res;
    setFullMock(true, N_MOCKED_PLUGINS);
    throwOnSample = true;
    EXPECT_NO_THROW(obj->open("/tmp", NULL));
    EXPECT_NO_THROW(obj->init());
    try {
        obj->sample(res);
    } catch (std::runtime_error& e) {
        EXPECT_TRUE(NULL != strstr(e.what(), "Failing on mockPlugin.sample"));
    }
}
