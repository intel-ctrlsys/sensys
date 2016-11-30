/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMIUTILAGENT_H
#define IPMIUTILAGENT_H

#include "orcm/mca/sensor/ipmi_ts/ipmiLibInterface.h"

class ipmiutilAgent : public ipmiLibInterface
{
private:
    class implPtr;
    implPtr* impl_;
public:
    ipmiutilAgent();
    ~ipmiutilAgent();
    std::set<std::string> getBmcList();
    ipmiResponse_t sendCommand(ipmiCommands command, const buffer& data, std::string bmc);

    // For testing purposes only. Please do not use in production code
    ipmiutilAgent(std::string file);
};

#endif //IPMIUTILDFX_H