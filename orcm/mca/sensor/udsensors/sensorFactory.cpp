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

void sensorFactory::close()
{
    std::map<std::string, void*>::iterator it;
    for (it = pluginHandlers.begin(); it != pluginHandlers.end(); ++it) {
        if (it->second) {
            dlclose(it->second);
        }
        pluginsLoaded.erase(it->first);
        pluginHandlers.erase(it);
    }
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
            pluginsLoaded[*it] = entryPoint;
        }
        pluginHandlers[*it] = plugin;
    }
}

int sensorFactory::getFoundPlugins()
{
    return pluginsLoaded.size();
}

int sensorFactory::getAmountOfPluginHandlers()
{
    return pluginHandlers.size();
}

int sensorFactory::getLoadedPlugins()
{
    int c = 0;
    std::map<std::string, pluginInstance*>::iterator it;
    for (it = pluginsLoaded.begin(); it != pluginsLoaded.end(); ++it) {
        if (it->second) {
            c++;
        }
    }
    return c;
}

sensorFactory* sensorFactory::getInstance()
{
    static sensorFactory instance;
    return &instance;
}
