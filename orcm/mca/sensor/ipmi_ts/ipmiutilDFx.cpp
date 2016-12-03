/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/sensor/ipmi_ts/ipmiutilDFx.h"

using namespace std;

set<string> ipmiutilDFx::getBmcList()
{
    set<string> list;
    list.insert("fake_bmc_1");
    list.insert("fake_bmc_2");
    list.insert("fake_bmc_3");
    list.insert("fake_bmc_4");
    list.insert("fake_bmc_5");
    return list;
}

ipmiResponse ipmiutilDFx::sendCommand(ipmiCommands command, buffer* data, std::string bmc)
{
    if (GETSENSORLIST == command)
        return ipmiResponse(getSensorList_(), "","",true);

    ipmiResponse response;
    return response;
}

map<unsigned short int, string> ipmiutilDFx::getSensorList_()
{
    map<unsigned short int, string> retValue;
    retValue[0] = "fake_sensor_1";
    retValue[1] = "fake_sensor_2";
    retValue[2] = "fake_sensor_3";
    retValue[3] = "fake_sensor_4";
    retValue[4] = "fake_sensor_5";
    retValue[5] = "fake_sensor_6";
    return retValue;

}