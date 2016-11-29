/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMI_SENSOR_FACTORY_H
#define IPMI_SENSOR_FACTORY_H

#include <stddef.h>
#include <stdlib.h>

#include "orcm/common/baseFactory.h"
#include "orcm/common/dataContainer.hpp"
#include "ipmiSensorInterface.h"

#include "ipmi_test_sensor/ipmi_test_sensor.hpp"

typedef ipmiSensorInterface* (*sensorInstance)(std::string);
typedef char* (*getPluginName)(void);
typedef std::map<std::string, ipmiSensorInterface*>::iterator pluginsIterator;
typedef std::map<std::string, ipmiSensorInterface*(*)(std::string)> IpmiPlugins;

class ipmiSensorFactory : public baseFactory {
public:
    static struct ipmiSensorFactory* getInstance();
    void load(bool test_vector);
    void close(void);
    void init(void);
    void sample(dataContainerMap& dc);
    void collect_inventory(dataContainerMap& dc);
    void unloadPlugin(std::map<std::string, ipmiSensorInterface*>::iterator it);
    int getLoadedPlugins(void);

    template<typename T>
    static ipmiSensorInterface *getIpmiInstance(std::string hostname){return new T(hostname);}

private:
    ipmiSensorFactory();
    virtual ~ipmiSensorFactory(){};
    void getPluginInstanceAndName(std::string ipmiObj);
    void __sample(pluginsIterator it, dataContainerMap &dc);
    void __collect_inventory(pluginsIterator it, dataContainerMap &dc);
    std::map<std::string, void*> pluginHandlers;
    std::map<std::string, ipmiSensorInterface*> pluginsLoaded;
    IpmiPlugins ipmiPlugins;
};

class ipmiSensorFactoryException : public std::runtime_error
{
public:
    ipmiSensorFactoryException(std::string str) : std::runtime_error(str) {};
};

#endif
