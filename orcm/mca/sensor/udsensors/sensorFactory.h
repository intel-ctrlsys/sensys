/*
 * Copyright (c) 2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SENSOR_FACTORY_H
#define SENSOR_FACTORY_H

#include <stddef.h>
#include <stdlib.h>

#include "orcm/common/baseFactory.h"
#include "orcm/common/dataContainer.hpp"
#include "orcm/common/sensorInterface.h"

typedef sensorInterface* (*sensorInstance)(void);
typedef char* (*getPluginName)(void);
typedef std::map<std::string, sensorInterface*>::iterator pluginsIterator;

class sensorFactory : public baseFactory {
public:
    static struct sensorFactory* getInstance();
    int open(const char *plugin_path, const char *plugin_prefix);
    void close(void);
    void init(void);
    void sample(dataContainerMap& dc);
    void loadPlugins(void);
    void unloadPlugin(std::map<std::string, void*>::iterator it);
    int getFoundPlugins(void);
    int getLoadedPlugins(void);
    int getAmountOfPluginHandlers(void);

private:
    sensorFactory(){};
    virtual ~sensorFactory(){};
    void openAndGetSymbolsFromPlugin(void);
    void getPluginInstanceAndName(void *plugin);
    void __sample(pluginsIterator it, dataContainerMap &dc);
    std::map<std::string, void*> pluginHandlers;
    std::map<std::string, sensorInterface*> pluginsLoaded;
};

class sensorFactoryException : public std::runtime_error
{
public:
    sensorFactoryException(std::string str) : std::runtime_error(str) {};
};

#endif
