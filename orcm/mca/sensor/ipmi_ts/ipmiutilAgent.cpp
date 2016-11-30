/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/sensor/ipmi_ts/ipmiutilAgent.h"
#include "orcm/mca/sensor/ipmi/ipmi_parser.h"

using namespace std;

class ipmiutilAgent::implPtr
{
private:
    void loadConfiguration_(string file);
public:
    ipmiCollectorMap config;
    set<string> hostList;

    implPtr(string file);
};

ipmiutilAgent::implPtr::implPtr(string file)
{
    loadConfiguration_(file);
}

void ipmiutilAgent::implPtr::loadConfiguration_(string file)
{
    ipmiParser *p = new ipmiParser(file);
    config = p->getIpmiCollectorMap();
    delete p;

    for (ipmiCollectorMap::iterator it = config.begin(); it != config.end(); ++it)
        hostList.insert(it->first);
}

ipmiutilAgent::ipmiutilAgent(string file) : impl_(new implPtr(file))
{
}

ipmiutilAgent::ipmiutilAgent()
{
    ipmiutilAgent("");
}

ipmiutilAgent::~ipmiutilAgent()
{
    delete impl_;
}

set<string> ipmiutilAgent::getBmcList()
{
    return impl_->hostList;
}

ipmiResponse_t ipmiutilAgent::sendCommand(ipmiCommands command, const buffer& data, std::string bmc)
{
    // TODO implementation for sendCommand
    ipmiResponse_t response;
    return response;
}
