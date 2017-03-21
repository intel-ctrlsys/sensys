/*
 * Copyright (c) 2016-2017  Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sensorFactory.h"
#include "orcm/runtime/orcm_globals.h"

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

void sensorFactory::init(string configPath)
{
    std::string errors = "";
    std::vector<std::string> removal_list;
    for (auto it = pluginsLoaded.begin(); it != pluginsLoaded.end(); ++it) {
        try {
            it->second->setConfigFilePath(configPath);
            it->second->init();
        } catch (std::runtime_error &e) {
            errors.append("Plugin " + it->first + " failed in init with:\n");
            errors.append(std::string(e.what()) + "\n");
            removal_list.push_back(it->first);
        }
    }

    for (auto i=removal_list.begin(); i!=removal_list.end(); ++i)
        unloadPlugin(pluginHandlers.find(*i));

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
    UDSensor* tmp_plugin = NULL;

    entryPoint = (sensorInstance)getPluginSymbol(plugin, "initPlugin");
    p_name = (getPluginName)getPluginSymbol(plugin, "getPluginName");

    if (entryPoint && p_name) {
        std::string plugin_name = std::string(p_name());
        tmp_plugin = entryPoint();
        //OOB sensors run only in the aggregator nodes
        if (!ORCM_PROC_IS_AGGREGATOR && (tmp_plugin->sensor_type == OOB)) {
            delete tmp_plugin;
            closePlugin(plugin);
        }
        else{
           pluginsLoaded[plugin_name] = tmp_plugin;
           pluginHandlers[plugin_name] = plugin;
        }

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
