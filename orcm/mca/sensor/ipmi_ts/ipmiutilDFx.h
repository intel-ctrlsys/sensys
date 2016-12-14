/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMIUTILDFX_H
#define IPMIUTILDFX_H

#include "orcm/mca/sensor/ipmi_ts/ipmiLibInterface.h"

class ipmiutilDFx : public ipmiLibInterface
{
private:
    dataContainer getSensorList_();
    dataContainer getReadings_();
    dataContainer getACPIPower_();
    dataContainer getSELRecords_();
    dataContainer getDeviceID_();
    dataContainer getFRUData_();
public:
    ipmiutilDFx() {};
    virtual ~ipmiutilDFx() {};
    std::set<std::string> getBmcList();
    ipmiResponse sendCommand(ipmiCommands command, buffer* data, std::string bmc);
};

#endif //IPMIUTILDFX_H
