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

void sensorFactory::init()
{
    std::map<std::string, sensorInterface*>::iterator it;
    std::string errors = "";
    for (it = pluginsLoaded.begin(); it != pluginsLoaded.end(); ++it) {
        try {
            it->second->init();
        } catch (std::runtime_error &e) {
            errors.append("Plugin " + it->first + " failed in init with:\n");
            errors.append(std::string(e.what()) + "\n");
        }
    }
    if (errors.compare("")) {
        throw sensorFactoryException(errors);
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
    sensorInstance entryPoint;
    std::vector<std::string>::iterator it;
    for (it = pluginFilesFound.begin(); it != pluginFilesFound.end(); ++it) {
        plugin = openPlugin(*it);
        if (plugin) {
            entryPoint = (sensorInstance)getPluginSymbol(plugin, "initPlugin");
            if (entryPoint) {
                pluginsLoaded[*it] = entryPoint();
            }
        }
        pluginHandlers[*it] = plugin;
    }
}

int sensorFactory::getFoundPlugins()
{
    return pluginFilesFound.size();
}

int sensorFactory::getAmountOfPluginHandlers()
{
    return pluginHandlers.size();
}

int sensorFactory::getLoadedPlugins()
{
    return pluginsLoaded.size();
}

sensorFactory* sensorFactory::getInstance()
{
    static sensorFactory instance;
    return &instance;
}
