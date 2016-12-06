/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <set>
#include "ipmiSensorFactory.hpp"
#include "orcm/mca/sensor/ipmi_ts/ipmiHAL.h"

ipmiSensorFactory::ipmiSensorFactory(){
     this->ipmiPlugins["IpmiTestSensor"] =  getIpmiInstance<IpmiTestSensor>;
     this->ipmiPlugins["ipmiSensor"] =  getIpmiInstance<ipmiSensor>;
};

void ipmiSensorFactory::load(bool test_vector)
{
    //std::string ipmiObj = (test_vector) ? "IpmiTestSensor" : "IpmiBmc";
    if (test_vector)
        getPluginInstanceAndName("IpmiTestSensor");
    else
        getPluginInstanceAndName("ipmiSensor");
}

void ipmiSensorFactory::close()
{
    pluginsIterator it;
    std::string errors = "";
    for (it = pluginsLoaded.begin(); it != pluginsLoaded.end(); ++it) {
        try {
           it->second->finalize();
           unloadPlugin(it);
        } catch (std::runtime_error &e) {
            errors.append("Device:  " + it->first + " failed in finalize with:\n");
            errors.append(std::string(e.what()) + "\n");
        }
    }
    if (errors.compare("")) {
        throw ipmiSensorFactoryException(errors);
    }
}

void ipmiSensorFactory::init()
{
    std::map<std::string, ipmiSensorInterface*>::iterator it;
    std::string errors = "";
    for (it = pluginsLoaded.begin(); it != pluginsLoaded.end(); ++it) {
        try {
            it->second->init();
        } catch (std::runtime_error &e) {
            errors.append("Device: " + it->first + " failed in init with:\n");
            errors.append(std::string(e.what()) + "\n");
        }
    }
    if (errors.compare("")) {
        throw ipmiSensorFactoryException(errors);
    }
}

void ipmiSensorFactory::setCallbackPointers(ipmiDataLoggerCallback sampling,
                                            ipmiDataLoggerCallback inventory,
                                            ipmiErrorLoggerCallback errorlog)
{
    std::map<std::string, ipmiSensorInterface*>::iterator it;

    for (it = pluginsLoaded.begin(); it != pluginsLoaded.end(); ++it)
    {
        it->second->setSamplingPtr(sampling);
        it->second->setInventoryPtr(inventory);
        it->second->setErrorPtr(errorlog);
    }
}

void ipmiSensorFactory::unloadPlugin(std::map<std::string, ipmiSensorInterface*>::iterator it)
{
    pluginsLoaded.erase(it->first);
}

void ipmiSensorFactory::sample()
{
    pluginsIterator it;
    std::string errors = "";
    for (it = pluginsLoaded.begin(); it != pluginsLoaded.end(); ++it) {
        try {
            it->second->sample();
        } catch (std::runtime_error &e) {
            errors.append("Device:  " + it->first + " failed in sample with:\n");
            errors.append(std::string(e.what()) + "\n");
        }
    }
    if (errors.compare("")) {
        throw ipmiSensorFactoryException(errors);
    }
    return;
}

void ipmiSensorFactory::collect_inventory()
{
    pluginsIterator it;
    std::string errors = "";
    for (it = pluginsLoaded.begin(); it != pluginsLoaded.end(); ++it) {
        try {
            it->second->collect_inventory();
        } catch (std::runtime_error &e) {
            errors.append("Device: " + it->first + " failed in inventory collection with:\n");
            errors.append(std::string(e.what()) + "\n");
        }
    }
    if (errors.compare("")) {
        throw ipmiSensorFactoryException(errors);
    }
    return;
}

void ipmiSensorFactory::getPluginInstanceAndName(std::string ipmiObj)
{
    ipmiHAL *HWobj = ipmiHAL::getInstance();
    std::set<std::string> bmcList = HWobj->getBmcList();
    std::set<std::string>::iterator it;

    for (it = bmcList.begin(); it != bmcList.end(); ++it)
    {
       pluginsLoaded[*it] = this->ipmiPlugins[ipmiObj](*it);
    }
}

int ipmiSensorFactory::getLoadedPlugins()
{
    return pluginsLoaded.size();
}

ipmiSensorFactory* ipmiSensorFactory::getInstance()
{
    static ipmiSensorFactory instance;
    return &instance;
}
