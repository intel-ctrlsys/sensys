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

enum ipmiCommands {
    DUMMY,
    GET_FRU_INV_AREA
};

typedef std::vector<unsigned char> buffer;

typedef struct
{
    buffer response;
    std::string errorMessage;
    std::string completionMessage;
} ipmiResponse_t;

class ipmiLibInterface
{
public:
    virtual ~ipmiLibInterface() {};
    virtual std::set<std::string> getBmcList() = 0;
    virtual ipmiResponse_t sendCommand(ipmiCommands command, const buffer& data, std::string bmc) = 0;
};

#endif //IPMILIBINTERFACE_H