/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmi_collector.h"

using namespace std;

ipmiCollector::ipmiCollector()
{
    hostname = bmc_address = aggregator = user = pass = "";
    setDefaults();
}

void ipmiCollector::setDefaults()
{
    auth_method = DEFAULT_AUTH_METHOD;
    priv_level = DEFAULT_PRIV_LEVEL;
    port = DEFAULT_PORT;
    channel = DEFAULT_CHANNEL;
}

ipmiCollector::ipmiCollector(string hostname, string bmc_address, string aggregator,
                             string user, string pass)
{
    this->hostname = hostname;
    this->bmc_address = bmc_address;
    this->aggregator = aggregator;
    this->user = user;
    this->pass = pass;
    setDefaults();
}

ipmiCollector::ipmiCollector(string hostname, string bmc_address, string aggregator,
                             string user, string pass,  auth_methods auth_method,
                             priv_levels priv_level, int port, int channel)
{
    this->hostname = hostname;
    this->bmc_address = bmc_address;
    this->aggregator = aggregator;
    this->user = user;
    this->pass = pass;
    if (ORCM_ERROR == setAuthMethod(auth_method)){
        this->auth_method = DEFAULT_AUTH_METHOD;
    }
    if (ORCM_ERROR == setPrivLevel(priv_level)){
        this->priv_level = DEFAULT_PRIV_LEVEL;
    }
    if (ORCM_ERROR ==  setPort(port)){
        this->port = DEFAULT_PORT;
    }
    if (ORCM_ERROR == setChannel(channel)){
        this->channel = DEFAULT_CHANNEL;
    }
}

int ipmiCollector::setAuthMethod(auth_methods auth_method)
{
    if (0 > auth_method){
        return ORCM_ERROR;
    }
    this->auth_method = auth_method;
    return ORCM_SUCCESS;
}

int ipmiCollector::setPrivLevel(priv_levels priv_level)
{
    if (0 > priv_level){
        return ORCM_ERROR;
    }
    this->priv_level = priv_level;
    return ORCM_SUCCESS;
}

int ipmiCollector::setPort(int port)
{
    if (0 > port){
        return ORCM_ERROR;
    }
    if (1024 > port){
        port += 1024;
    }
    this->port = port;
    return ORCM_SUCCESS;
}

int ipmiCollector::setChannel(int channel)
{
    if (0 > channel){
        return ORCM_ERROR;
    }
    this->channel = channel;
    return ORCM_SUCCESS;
}
