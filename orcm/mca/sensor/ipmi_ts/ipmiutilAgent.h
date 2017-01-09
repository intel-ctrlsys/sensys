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

#include <map>
#include <stdexcept>

#include "orcm/mca/sensor/ipmi_ts/ipmiLibInterface.h"
#include "orcm/mca/sensor/ipmi_ts/ipmiutilAgent_exceptions.h"

extern "C" {
    typedef unsigned char uchar;
    typedef unsigned short int ushort;
    #include <../share/ipmiutil/isensor.h>
};

class ipmiutilAgent : public ipmiLibInterface
{
private:
    class implPtr;
    implPtr* impl_;
public:
    ipmiutilAgent();
    virtual ~ipmiutilAgent();
    std::set<std::string> getBmcList(string agg);
    ipmiResponse sendCommand(ipmiCommands command, buffer* data, std::string bmc);

    // For testing purposes only. Please do not use in production code
    ipmiutilAgent(std::string file);
};

#endif //IPMIUTILDFX_H
