/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <iostream>
#include <cstdlib>
#include <map>
#include <dlfcn.h>
#include <string.h>
#include <dirent.h>

#include "analytics_factory.h"
#include "c_analytics_factory.h"

typedef void (*EntryPointFunc) (void);

int AnalyticsFactory::search(const char* directory, const char* plugin_prefix)
{
    setPluginsPath(directory);
    setPluginsPrefix(plugin_prefix);
    return findAndRegistPlugins();
}

void AnalyticsFactory::cleanup()
{
    plugins.clear();
    pluginFilesFound.clear();
    std::vector<void*>::iterator it = pluginHandlers.begin();
    while (it != pluginHandlers.end()) {
        dlclose(*it);
        it = pluginHandlers.erase(it);
    }
}

int AnalyticsFactory::findAndRegistPlugins()
{
    int ret = getPluginFilenames();
    if (ANALYTICS_SUCCESS != ret) {
        return ret;
    }
    registPlugins();
    return ret;
}

static int ret_on_error()
{
    char* error = NULL;
    if (NULL != (error = dlerror())) {
        return ANALYTICS_ERROR;
    }
    return ANALYTICS_SUCCESS;
}

int AnalyticsFactory::openAndSetPluginCreator(std::string plugin_name)
{
    void* plugin = openPlugin(plugin_name);
    int erri = ANALYTICS_SUCCESS;
    if (NULL != plugin) {
        EntryPointFunc entryPoint = (EntryPointFunc)getPluginSymbol(plugin, "initPlugin");
        if (NULL == entryPoint) {
            dlclose(plugin);
            return ret_on_error();
        }
        entryPoint();
        pluginHandlers.push_back(plugin);
        return ANALYTICS_SUCCESS;
    } else {
        return ret_on_error();
    }
}

void AnalyticsFactory::registPlugins(void)
{
    for (int index = 0; index < pluginFilesFound.size(); index++) {
        openAndSetPluginCreator(pluginFilesFound.at(index));
    }
}

void AnalyticsFactory::setPluginCreator(std::string plugin_name, create_obj creator)
{
    plugins[plugin_name] = creator;
}

AnalyticsFactory* AnalyticsFactory::getInstance()
{
    static AnalyticsFactory instance;
    return &instance;
}

Analytics* AnalyticsFactory::createPlugin(std::string plugin_name)
{
    std::map<std::string, create_obj>::iterator it = plugins.find(plugin_name);
    if (it != plugins.end()) {
        return (it->second)();
    }
    return NULL;
}

bool AnalyticsFactory::checkPluginExisit(const char* plugin_name)
{
    std::string plugin_name_str = plugin_name;
    std::map<std::string, create_obj>::iterator it = plugins.find(plugin_name_str);
    if (it != plugins.end()) {
        return true;
    }
    return false;
}

int search_plugin_creator(const char* directory, const char* plugin_prefix)
{
    AnalyticsFactory* analytics_factory = AnalyticsFactory::getInstance();
    return analytics_factory->search(directory, plugin_prefix);
}

bool check_plugin_exist(const char* plugin_name)
{
    AnalyticsFactory* analytics_factory = AnalyticsFactory::getInstance();
    return analytics_factory->checkPluginExisit(plugin_name);
}

void close_clean_plugin(void)
{
    AnalyticsFactory* analytics_factory = AnalyticsFactory::getInstance();
    analytics_factory->cleanup();
}
