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

    if (GETSENSORREADINGS == command)
        return ipmiResponse(getReadings_(), "","",true);

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

dataContainer ipmiutilDFx::getReadings_()
{
    dataContainer dc;
    dc.put("intValue", (int) 3, "ints");
    dc.put("floatValue", (float) 3.14, "floats");
    dc.put("doubleValue", (double) 3.14159265, "doubles");
    dc.put("intValue", (int) 5, "units-ints");
    dc.put("boolValue", (bool) true, "booleans");
    dc.put("int8Value", (int8_t) -8, "int8s");
    dc.put("int16Value", (int16_t) -16, "int16s");
    dc.put("int32Value", (int32_t) -32, "int32s");
    dc.put("int64Value", (int64_t) -64, "int64s");
    dc.put("uint8Value", (uint8_t) 8, "uint8s");
    dc.put("uint16Value", (uint16_t) 16, "uint16s");
    dc.put("uint32Value", (uint32_t) 32, "uint32s");
    dc.put("uint64Value", (uint64_t) 64, "uint64s");
    dc.put("floatValue", (float) 3.14, "floats");
    return dc;

}