/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/sensor/ipmi_ts/ipmiutilAgent.h"
#include "orcm/mca/sensor/ipmi_ts/ipmi_ts_sel_collector.h"
#include "orcm/mca/sensor/ipmi_ts/ipmiResponseFactory.hpp"

#include "orcm/mca/sensor/ipmi/ipmi_parser.h"
#include "orcm/mca/sensor/ipmi/ipmi_collector.h"

#include <ipmicmd.h>

#include "orcm/util/string_utils.h"

using namespace std;

class ipmiutilAgent::implPtr
{
private:
    static const int MAX_RESPONSE_SIZE = 256;
    static const int MAX_RETRIES_FOR_FRU_DATA = 15;
    static const int MAX_FRU_DEVICES  = 254;
    static const int FRU_PAGE_SIZE = 16;
    static const char DEBUG = 0;
    static const unsigned short GET_ACPI_POWER = (0x07 | (NETFN_APP << 8));

    void loadConfiguration_(string file);
    bool isNewAreaLarger_(long int* area, unsigned char* rdata);
    ipmiResponse getFruData_(int id, long int area);
    void setAddressForNextFruPage_(buffer &inputBuffer);
    void initializeFruInputBuffer_(buffer &inputBuffer,int id);
    dataContainer getSensorListFromSDR_();
    dataContainer getReadingsFromSDR_();
    string getSelFilename_();
    dataContainer getDataContainerFromResponseBuffer(ResponseBuffer buff, MessageType type);
    dataContainer getDataContainerFromCBuffer(unsigned char* rdata, int rlen, MessageType type);
    dataContainer getNodePowerDataContainer(const unsigned char npwr_device);
    void appendNodePowerDataContainerToDataContainer(dataContainer *dc, dataContainer npwr_dc, string tag);

    static void sel_error_callback_(int level, const char* msg);
    static void sel_ras_event_callback_(const char* event, const char* hostname, void* user_object);


public:
    ipmiCollectorMap config;
    ipmiCollectorVector config_vector;
    set<string> hostList;
    set<string> aggregators;

    implPtr(string file);
    string setConnectionParameters(string bmc);
    ipmiResponse getDeviceId(buffer* data);
    ipmiResponse getAcpiPower(buffer* data);
    ipmiResponse getFruInventory(buffer* data);
    ipmiResponse getDummyResponse(buffer* data);
    ipmiResponse getSensorList(buffer* data);
    ipmiResponse getSensorReadings(buffer* data);
    ipmiResponse getSelRecords(string bmc);
    ipmiResponse getPsuPower(buffer* data);
};

static string selErrorMessage("");

class connectionInfo
{
public:
    // This class is needed because the compiler is complaining about casting const char* to char*
    static const int ADDRESS_LEN = 16;
    static const int cipher = 3;

    char address[ADDRESS_LEN];
    char *bmcAddress = NULL;
    char *user = NULL;
    char *pass = NULL;
    int auth;
    int priv;

    connectionInfo(string bmc, ipmiCollectorMap config)
    {
        ipmiCollector *ptr = &config[bmc];
        bmcAddress = strdup(ptr->getBmcAddress().c_str());
        user = strdup(ptr->getUser().c_str());
        pass = strdup(ptr->getPass().c_str());
        auth = ptr->getAuthMethod();
        priv = ptr->getPrivLevel();
    }

    ~connectionInfo()
    {
        free(bmcAddress);
        free(user);
        free(pass);
    }
};

ipmiutilAgent::implPtr::implPtr(string file)
{
    loadConfiguration_(file);
}


void ipmiutilAgent::implPtr::loadConfiguration_(string file)
{
    ipmiParser *p = new ipmiParser(file);
    config = p->getIpmiCollectorMap();
    config_vector = p->getIpmiCollectorVector();

    delete p;
}

ipmiutilAgent::ipmiutilAgent(string file) : impl_(new implPtr(file))
{
}

ipmiutilAgent::ipmiutilAgent() : impl_(new implPtr(""))
{
    ipmiutilAgent("");
}

ipmiutilAgent::~ipmiutilAgent()
{
    delete impl_;
}


set<string> ipmiutilAgent::getBmcList(string agg)
{
    ipmiCollectorVector::iterator it;
    set<string> bmcList;

    for (it = impl_->config_vector.begin(); it != impl_->config_vector.end(); ++it) {
         if (0 == agg.compare(it->getAggregator()))
              bmcList.insert(it->getHostname());
    }

    return bmcList;
}

string ipmiutilAgent::implPtr::setConnectionParameters(string bmc)
{
    connectionInfo conn(bmc, config);

    int rc = set_lan_options(conn.bmcAddress,
                             conn.user,
                             conn.pass,
                             conn.auth,
                             conn.priv,
                             conn.cipher,
                             &conn.address,
                             conn.ADDRESS_LEN);

    if (0 != rc)
        throw badConnectionParameters(rc);

    return string(conn.address);
}

ipmiResponse ipmiutilAgent::sendCommand(ipmiCommands command, buffer* data, string bmc)
{
    ipmi_close();
    impl_->setConnectionParameters(bmc);

    switch (command)
    {
        case GETDEVICEID:
            return impl_->getDeviceId(data);
        case GETACPIPOWER:
            return impl_->getAcpiPower(data);
        case READFRUDATA:
            return impl_->getFruInventory(data);
        case GETSENSORLIST:
            return impl_->getSensorList(data);
        case GETSENSORREADINGS:
            return impl_->getSensorReadings(data);
        case GETSELRECORDS:
            return impl_->getSelRecords(bmc);
        case GETPSUPOWER:
            return impl_->getPsuPower(data);
        default:
            return impl_->getDummyResponse(data);
    }
}

string ipmiutilAgent::implPtr::getSelFilename_()
{
    return string();
}

ipmiResponse ipmiutilAgent::implPtr::getSelRecords(string bmc)
{
    int rc = 0;
    int cc = 0;

    selErrorMessage = "";
    dataContainer selRecords;
    ipmi_ts_sel_collector scanner(bmc.c_str(), sel_error_callback_, (void*) &selRecords);

    if(false == scanner.is_bad()) {
        scanner.load_last_record_id(getSelFilename_().c_str());
        if (scanner.scan_new_records(sel_ras_event_callback_))
        {
            rc = -15;
            cc = 255;
        }

        return ipmiResponse(selRecords, getErrorMessage(rc), getCompletionMessage(cc), true);
    }

    return ipmiResponse(NULL, selErrorMessage, "", false);
}

void ipmiutilAgent::implPtr::sel_error_callback_(int level, const char* msg)
{
    char* line;
    asprintf(&line, "%s: collecting IPMI SEL records: %s\n", (0 == level)?"ERROR":"INFO", msg);
    selErrorMessage = string(line);
    free(line);
}

void ipmiutilAgent::implPtr::sel_ras_event_callback_(const char* event, const char* hostname, void* user_object)
{
    dataContainer* dc = (dataContainer*)user_object;
    string n = cast_to_str(dc->count());
    dc->put(string("sel_event_record_") + n, string(event), "");
}

ipmiResponse ipmiutilAgent::implPtr::getSensorList(buffer* data)
{
    try {
        dataContainer list = getSensorListFromSDR_();
        ipmi_close();
        return ipmiResponse(list, getErrorMessage(0), getCompletionMessage(0), true);
    } catch(runtime_error &e) {
        ipmi_close();
        return ipmiResponse(NULL, e.what(), "", false);
    }
}

ipmiResponse ipmiutilAgent::implPtr::getSensorReadings(buffer* data)
{
    try {
        dataContainer dc = getReadingsFromSDR_();
        ipmi_close();
        return ipmiResponse(dc, getErrorMessage(0), getCompletionMessage(0), true);
    } catch(runtime_error &e) {
        ipmi_close();
        return ipmiResponse(NULL, e.what(), "", false);
    }
}

dataContainer ipmiutilAgent::implPtr::getSensorListFromSDR_()
{
    dataContainer list;
    unsigned char *sdrlist = NULL;

    if (0 != get_sdr_cache(&sdrlist))
    {
        free_sdr_cache(sdrlist);
        throw unableToCollectSensorList();
    }

    unsigned short int id = 0;
    unsigned char sdrbuf[SDR_SZ];

    string prefix = "sensor_ipmi_ts_";
    int sufix = 0;
    while(0 == find_sdr_next(sdrbuf, sdrlist, id))
    {
        id = sdrbuf[0] + (sdrbuf[1] << 8); // this SDR id
        if (0x01 == sdrbuf[3]) // full SDR, let's keep it
        {
            int name_size = (int)(((SDR01REC*)sdrbuf)->id_strlen & 0x1f);
            string sensor_name = string((char*)&sdrbuf[48], name_size);
            list.put(prefix+cast_to_str(++sufix), sensor_name, "");
        }
    }
    list.put(prefix+cast_to_str(++sufix), string("Node Power"), "");

    free_sdr_cache(sdrlist);

    return list;
}

ipmiResponse ipmiutilAgent::implPtr::getDummyResponse(buffer* data)
{
    return ipmiResponse(data, getErrorMessage(0), getCompletionMessage(0), true);
}

dataContainer ipmiutilAgent::implPtr::getDataContainerFromResponseBuffer(
    buffer buff, MessageType type)
{
    IPMIResponse ipmi_rsp(&buff, type);
    return ipmi_rsp.getDataContainer();
}

dataContainer ipmiutilAgent::implPtr::getDataContainerFromCBuffer(
    unsigned char* rdata, int rlen, MessageType type)
{
    buffer buff(rdata, rdata+rlen);
    return getDataContainerFromResponseBuffer(buff, type);
}

ipmiResponse ipmiutilAgent::implPtr::getDeviceId(buffer* data)
{
    unsigned char rdata[MAX_RESPONSE_SIZE];
    int rlen = MAX_RESPONSE_SIZE;
    unsigned char cc = 0;

    int rc = ipmi_cmd_mc(GET_DEVICE_ID, &(data->front()), 0, rdata, &rlen, &cc, DEBUG);
    ipmi_close();

    dataContainer dc = getDataContainerFromCBuffer(rdata, rlen, GETDEVICEID_MSG);
    return ipmiResponse(dc, getErrorMessage(rc), getCompletionMessage(cc), 0 == rc);
}

dataContainer ipmiutilAgent::implPtr::getNodePowerDataContainer(const unsigned char npwr_device)
{
    // unsigned char cmd[8] = {0x00, 0x20, 0x18, 0x52, 0x0f, npwr_device, 0x07, 0x86};

    // The following values are hardcoded for nodepower-specific IPMI messages
    unsigned char cc    = 0;
    unsigned char cmd   = 0x52;
    unsigned char netfn = 0x06;
    unsigned char slave = 0x20;
    unsigned char bus   = 0x00;
    unsigned char lun   = 0x00;

    unsigned char in_buff[4] = {0x0f, npwr_device, 0x07, 0x86};
    int in_len = 4;

    unsigned char out_buff[MAX_RESPONSE_SIZE];
    int out_len = MAX_RESPONSE_SIZE;

    ipmi_cmdraw(cmd, netfn, slave, bus, lun, in_buff, in_len, out_buff, &out_len, &cc, 0);
    ipmi_close();
    return getDataContainerFromCBuffer(out_buff, out_len, GETPSUPOWER_MSG);
}

void ipmiutilAgent::implPtr::appendNodePowerDataContainerToDataContainer(
    dataContainer *dc, dataContainer npwr_dc, string tag)
{
    uint64_t accu = 0;
    uint64_t cnt = 0;

    if (0 != npwr_dc.count())
    {
        accu = npwr_dc.getValue<uint64_t>("accu");
        cnt = npwr_dc.getValue<uint64_t>("cnt");
    }

    dc->put("accu_"+tag, accu, "");
    dc->put("cnt_"+tag, cnt, "");
}

ipmiResponse ipmiutilAgent::implPtr::getPsuPower(buffer* data)
{
    const unsigned char NODEPOWER_PA_R = 0xb0;
    const unsigned char NODEPOWER_PB_R = 0xb2;

    dataContainer dc_a = getNodePowerDataContainer(NODEPOWER_PA_R);
    dataContainer dc_b = getNodePowerDataContainer(NODEPOWER_PB_R);

    dataContainer dc;
    appendNodePowerDataContainerToDataContainer(&dc, dc_a, "a");
    appendNodePowerDataContainerToDataContainer(&dc, dc_b, "b");

    return ipmiResponse(dc, getErrorMessage(0), getCompletionMessage(0), true);
}


ipmiResponse ipmiutilAgent::implPtr::getAcpiPower(buffer* data)
{
    unsigned char rdata[MAX_RESPONSE_SIZE];
    int rlen = MAX_RESPONSE_SIZE;
    unsigned char cc = 0;

    int rc = ipmi_cmd_mc(GET_ACPI_POWER, &(data->front()), 0, rdata, &rlen, &cc, DEBUG);
    ipmi_close();

    dataContainer dc = getDataContainerFromCBuffer(rdata, rlen, GETACPIPOWER_MSG);
    return ipmiResponse(dc, getErrorMessage(rc), getCompletionMessage(cc), 0 == rc);
}

ipmiResponse ipmiutilAgent::implPtr::getFruInventory(buffer* data)
{
    int id = 0;
    long int max_fru_area = 0;
    buffer inputBuffer(4,0);

    for (inputBuffer[0] = 0; inputBuffer[0] < MAX_FRU_DEVICES; ++inputBuffer[0]) {
        unsigned char cc = 0;
        int rlen = MAX_RESPONSE_SIZE;
        unsigned char rdata[MAX_RESPONSE_SIZE] = {0};
        int rc = ipmi_cmd(GET_FRU_INV_AREA, &inputBuffer.front(), 1, rdata, &rlen, &cc, DEBUG);
        if (0 != rc)
            return ipmiResponse(rdata, rlen, getErrorMessage(rc), getCompletionMessage(cc), false);

        if (isNewAreaLarger_(&max_fru_area, rdata))
            id = (int) inputBuffer[0];
    }

    return getFruData_(id, max_fru_area);
}

dataContainer ipmiutilAgent::implPtr::getReadingsFromSDR_()
{
    dataContainer readings;
    unsigned char *sdrlist = NULL;

    if (0 != get_sdr_cache(&sdrlist))
    {
        free_sdr_cache(sdrlist);
        throw unableToCollectSensorReadings();
    }

    unsigned short int id = 0;
    unsigned char sdrbuf[SDR_SZ];

    while(0 == find_sdr_next(sdrbuf, sdrlist, id))
    {
        id = sdrbuf[0] + (sdrbuf[1] << 8); // this SDR id
        if (0x01 == sdrbuf[3]) // full SDR, let's keep it
        {
            int snum = sdrbuf[7];
            unsigned char reading[4] = {0};
            if (0 == GetSensorReading(snum, sdrbuf, reading))
            {
                int name_size = (int)(((SDR01REC*)sdrbuf)->id_strlen & 0x1f);;
                string key = string((char*)&sdrbuf[48], name_size);
                string unit = string(get_unit_type( sdrbuf[20], sdrbuf[21], sdrbuf[22],0));
                double value = RawToFloat(reading[0], sdrbuf);
                readings.put(key, value, unit);
            }
        }
    }

    free_sdr_cache(sdrlist);

    return readings;
}

bool ipmiutilAgent::implPtr::isNewAreaLarger_(long int* area, unsigned char* rdata)
{
    long int tmp_val = *(rdata + 0);
    tmp_val += ((long int) *(rdata + 1)) << 8;

    bool retValue = tmp_val > *area;
    if (retValue)
        *area = tmp_val;

    return retValue;
}

ipmiResponse ipmiutilAgent::implPtr::getFruData_(int id, long int area)
{
    buffer fruData;
    fruData.reserve(area);

    buffer inputBuffer;
    initializeFruInputBuffer_(inputBuffer, id);

    for (int i = 0; i < (area/FRU_PAGE_SIZE); ++i, setAddressForNextFruPage_(inputBuffer)) {
        int rc = -1;
        unsigned char cc = 0;
        unsigned char rdata[MAX_RESPONSE_SIZE] = {0};
        int rlen = MAX_RESPONSE_SIZE;

        for (int attempt = 0; attempt < MAX_RETRIES_FOR_FRU_DATA && 0 != rc; ++attempt) {
            rc = ipmi_cmd(READ_FRU_DATA, &inputBuffer.front(), 4, rdata, &rlen, &cc, 0);
        }

        if (0 != rc)
            return ipmiResponse(rdata, rlen, getErrorMessage(rc), getCompletionMessage(cc), false);

        fruData.insert(fruData.end(), rdata + 1, rdata + FRU_PAGE_SIZE + 1);
    }

    dataContainer dc = getDataContainerFromResponseBuffer(fruData, READFRUDATA_MSG);
    return ipmiResponse(dc, getErrorMessage(0), getCompletionMessage(0), true);
}

void ipmiutilAgent::implPtr::initializeFruInputBuffer_(buffer &inputBuffer, int id)
{
    inputBuffer.push_back(id);      // id of the fru device to read from
    inputBuffer.push_back(0x00);    // LSByte of the offset, start at 0
    inputBuffer.push_back(0x00);    // MSbyte of the offset, start at 0
    inputBuffer.push_back(0x10);    // reading 16 bytes at a time
}

void ipmiutilAgent::implPtr::setAddressForNextFruPage_(buffer &inputBuffer)
{
    // We need to increment the MSByte instead of the LSByte
    if (inputBuffer[1] == 240) {
        inputBuffer[1] = 0x00;
        inputBuffer[2]++;
    } else {
        inputBuffer[1] += 0x10;
    }
}
