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

#include "orcm/common/baseFactory.h"

class sensorFactory : public baseFactory {
public:
    static struct sensorFactory* getInstance();
    int open(const char *plugin_path, const char *plugin_prefix);
    void close(void);
    void loadPlugins(void);
    int getFoundPlugins(void);
    int getLoadedPlugins(void);
    int getAmountOfPluginHandlers(void);

private:
    sensorFactory(){};
    virtual ~sensorFactory(){};
    void openAndGetSymbolsFromPlugin(void);
    std::map<std::string, void*> pluginHandlers;
};

class sensorFactoryException : public std::runtime_error
{
public:
    sensorFactoryException(std::string str) : std::runtime_error(str) {};
};

#endif
