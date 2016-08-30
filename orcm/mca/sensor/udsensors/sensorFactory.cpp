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
        unloadPlugin(it);
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
            unloadPlugin(pluginHandlers.find(it->first));
        }
    }
    if (errors.compare("")) {
        throw sensorFactoryException(errors);
    }
}

void sensorFactory::unloadPlugin(std::map<std::string, void*>::iterator it)
{
    closePlugin(it->second);
    pluginsLoaded.erase(it->first);
    pluginHandlers.erase(it);
}

void sensorFactory::sample(dataContainerMap &dc)
{
    pluginsIterator it;
    std::string errors = "";
    for (it = pluginsLoaded.begin(); it != pluginsLoaded.end(); ++it) {
        try {
            __sample(it, dc);
        } catch (std::runtime_error &e) {
            errors.append("Plugin " + it->first + " failed in sample with:\n");
            errors.append(std::string(e.what()) + "\n");
        }
    }
    if (errors.compare("")) {
        throw sensorFactoryException(errors);
    }
    return;
}

void sensorFactory::__sample(pluginsIterator it, dataContainerMap &dc)
{
    dataContainer *tmpdataContainer;
    tmpdataContainer = new dataContainer;
    it->second->sample(*tmpdataContainer);
    if (tmpdataContainer->count())
        dc[it->first] = *tmpdataContainer;
    delete tmpdataContainer;
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
    std::vector<std::string>::iterator it;
    for (it = pluginFilesFound.begin(); it != pluginFilesFound.end(); ++it) {
        plugin = openPlugin(*it);
        if (plugin) {
            getPluginInstanceAndName(plugin);
        }
    }
}

void sensorFactory::getPluginInstanceAndName(void *plugin)
{
    sensorInstance entryPoint;
    getPluginName p_name;
    char *tmp_name = NULL;

    entryPoint = (sensorInstance)getPluginSymbol(plugin, "initPlugin");
    p_name = (getPluginName)getPluginSymbol(plugin, "getPluginName");

    if (entryPoint && p_name) {
        std::string plugin_name = std::string(p_name());
        pluginsLoaded[plugin_name] = entryPoint();
        pluginHandlers[plugin_name] = plugin;
    } else {
        closePlugin(plugin);
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
