/*
 * Copyright (c) 2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sensorFactory.h"

int sensorFactory::open(const char *plugin_path, const char *plugin_prefix)
{
    setPluginsPath(plugin_path);
    setPluginsPrefix(plugin_prefix);
    loadPlugins();
    return 0;
}

void sensorFactory::loadPlugins()
{
    int res = 0;
    res = getPluginFilenames();
    if (res) {
        throw sensorFactoryException("Cannot open directory " + plugins_path);
    }
    openAndGetSymbolsFromPlugin();
}

void sensorFactory::openAndGetSymbolsFromPlugin()
{
    void *plugin;
    pluginInstance *entryPoint;

    std::vector<std::string>::iterator it;
    for (it = pluginFilesFound.begin(); it != pluginFilesFound.end(); ++it) {
        plugin = openPlugin(*it);
        if (plugin) {
            entryPoint = (pluginInstance*)getPluginSymbol(plugin, "initPlugin");
            if (entryPoint) {
                pluginsLoaded[*it] = entryPoint;
            } else {
                throw sensorFactoryException("Symbol not found on plugin");
            }
        }
    }
}

sensorFactory* sensorFactory::getInstance()
{
    static sensorFactory instance;
    return &instance;
}
