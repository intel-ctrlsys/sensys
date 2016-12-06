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
    if (GETFULLSENSORLIST == command || GETSENSORLIST == command )
        return ipmiResponse(getSensorList_(), "","",true);

    if (GETFULLSENSORREADING == command || GETSENSORREADINGS == command )
        return ipmiResponse(getReadings_(), "","",true);

    ipmiResponse response;
    return response;
}

dataContainer ipmiutilDFx::getSensorList_()
{
    dataContainer dc;
    dc.put("sensor_ipmi_ts_1", string("doubleValue"), "");
    dc.put("sensor_ipmi_ts_2", string("intValue"), "");
    dc.put("sensor_ipmi_ts_3", string("boolValue"), "");
    dc.put("sensor_ipmi_ts_4", string("int8Value"), "");
    dc.put("sensor_ipmi_ts_5", string("int16Value"), "");
    dc.put("sensor_ipmi_ts_6", string("int32Value"), "");
    dc.put("sensor_ipmi_ts_7", string("int64Value"), "");
    dc.put("sensor_ipmi_ts_8", string("uint8Value"), "");
    dc.put("sensor_ipmi_ts_9", string("uint16Value"), "");
    dc.put("sensor_ipmi_ts_10", string("uint32Value"), "");
    dc.put("sensor_ipmi_ts_11", string("uint64Value"), "");
    dc.put("sensor_ipmi_ts_12", string("floatValue"), "");
    dc.put("bmc_ver", string("1.22"), "");
    dc.put("ipmi_ver", string("2"), "");
    dc.put("bb_serial", string("QSIP23300384"), "");
    dc.put("bb_vendor", string("Intel Corporation"), "");
    dc.put("bb_manufactured_date", string("08/18/12"), "");
    return dc;

}

dataContainer ipmiutilDFx::getReadings_()
{
    dataContainer dc;
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
