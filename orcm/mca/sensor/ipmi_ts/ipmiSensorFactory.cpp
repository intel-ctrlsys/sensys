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
};

void ipmiSensorFactory::load(bool test_vector)
{
    //std::string ipmiObj = (test_vector) ? "IpmiTestSensor" : "IpmiBmc";
    if (test_vector){
        getPluginInstanceAndName("IpmiTestSensor");
    }
    else{
        throw ipmiSensorFactoryException("Not implemented");
    }
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
            errors.append("Device:  " + it->first + " failed in finilize with:\n");
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

void ipmiSensorFactory::unloadPlugin(std::map<std::string, ipmiSensorInterface*>::iterator it)
{
    pluginsLoaded.erase(it->first);
}

void ipmiSensorFactory::sample(dataContainerMap &dc)
{
    pluginsIterator it;
    std::string errors = "";
    for (it = pluginsLoaded.begin(); it != pluginsLoaded.end(); ++it) {
        try {
            __sample(it, dc);
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

void ipmiSensorFactory::collect_inventory(dataContainerMap &dc)
{
    pluginsIterator it;
    std::string errors = "";
    for (it = pluginsLoaded.begin(); it != pluginsLoaded.end(); ++it) {
        try {
            __collect_inventory(it, dc);
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

void ipmiSensorFactory::__collect_inventory(pluginsIterator it, dataContainerMap &dc)
{
    dataContainer *tmpdataContainer;
    tmpdataContainer = new dataContainer;
    it->second->collect_inventory(*tmpdataContainer);
    if (tmpdataContainer->count())
        dc[it->first] = *tmpdataContainer;
    delete tmpdataContainer;
}

void ipmiSensorFactory::__sample(pluginsIterator it, dataContainerMap &dc)
{
    dataContainer *tmpdataContainer;
    tmpdataContainer = new dataContainer;
    it->second->sample(*tmpdataContainer);
    if (tmpdataContainer->count())
        dc[it->first] = *tmpdataContainer;
    delete tmpdataContainer;
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
