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
}

ipmiResponse_t ipmiutilDFx::sendCommand(ipmiCommands command, const buffer& data, std::string bmc)
{
    ipmiResponse_t response;

    buffer bmcResponse;
    response.completionMessage = string();
    response.errorMessage = string();
    response.response = bmcResponse;

    return response;
}
