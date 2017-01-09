/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMILIBINTERFACE_H
#define IPMILIBINTERFACE_H

#include <set>
#include <vector>
#include <string>
#include <map>

#include "ipmiResponse.h"

enum ipmiCommands {
    DUMMY = 0,
    GETDEVICEID,
    GETACPIPOWER,
    READFRUDATA,
    GETSENSORLIST,
    GETSENSORREADINGS,
    GETSELRECORDS,
    GETPSUPOWER
};

typedef std::vector<unsigned char> buffer;

class ipmiLibInterface
{
public:
    virtual ~ipmiLibInterface() {};
    virtual std::set<std::string> getBmcList(string agg) = 0;
    virtual ipmiResponse sendCommand(ipmiCommands command, buffer* data, std::string bmc) = 0;
};

#endif //IPMILIBINTERFACE_H
