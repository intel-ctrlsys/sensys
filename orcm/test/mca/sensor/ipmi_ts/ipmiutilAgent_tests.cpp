/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmiutilAgent_tests.h"
#include "ipmiutilAgent_mocks.h"
#include "orcm/mca/sensor/ipmi_ts/ipmiutilAgent.h"
#include "orcm/mca/sensor/ipmi_ts/ipmiutilAgent_exceptions.h"
#include "orcm/mca/sensor/ipmi/ipmi_parser.h"


extern "C" {
    #include "orcm/mca/parser/base/base.h"
    #include "orcm/mca/parser/pugi/parser_pugi.h"
    #include "orcm/util/utils.h"
}

using namespace std;

extern std::map<supportedMocks, mockManager> mocks;
extern sensor_ipmi_sel_mocked_functions sel_mocking;

void ipmiutilAgent_tests::SetUp()
{
    initParserFramework();
    setFileName();

    orcm_cfgi_base.config_file = strdup(fileName.c_str());

    for (map<supportedMocks, mockManager>::iterator it = mocks.begin(); it != mocks.end(); ++it)
        it->second.restartMock();

    agent = new ipmiutilAgent(fileName);
}

void ipmiutilAgent_tests::TearDown()
{
    delete agent;
    cleanParserFramework();
}

void ipmiutilAgent_tests::initParserFramework()
{
    OBJ_CONSTRUCT(&orcm_parser_base_framework.framework_components, opal_list_t);
    OBJ_CONSTRUCT(&orcm_parser_base.actives, opal_list_t);
    orcm_parser_active_module_t *act_module;
    act_module = OBJ_NEW(orcm_parser_active_module_t);
    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_pugi_module;
    opal_list_append(&orcm_parser_base.actives, &act_module->super);
}

void ipmiutilAgent_tests::cleanParserFramework()
{
    OPAL_LIST_DESTRUCT(&orcm_parser_base_framework.framework_components);
    OPAL_LIST_DESTRUCT(&orcm_parser_base.actives);
}

const set<string> ipmiutilAgent_tests::getNodesInConfigFile()
{
    // Every node configured in the example config file, must be listed here.
    // Tests will fail otherwise.
    set<string> list;
    list.insert("cn01");
    list.insert("cn02");
    list.insert("cn03");
    list.insert("cn04");
    list.insert("cn05");
    list.insert("cn06");
    return list;
}

bool ipmiutilAgent_tests::isStringInTheSet(const string &name, const set<string> &list)
{
    return list.find(name) != list.end();
}

void ipmiutilAgent_tests::setFileName()
{
    char* srcdir = getenv("srcdir");
    if (NULL != srcdir){
        fileName = string(srcdir) + "/ipmi.xml";
    } else {
        fileName = "ipmi.xml";
    }
}

void ipmiutilAgent_tests::assertHostNameListIsConsistent(set<string> list)
{
    set<string> configuredList = getNodesInConfigFile();
    for (set<string>::const_iterator it = list.begin() ; it != list.end(); ++it)
        ASSERT_TRUE(isStringInTheSet(*it, configuredList));
}

// Test cases
TEST_F(ipmiutilAgent_tests, empty_constructor)
{
    ipmiutilAgent *ptr = new ipmiutilAgent("");
    delete ptr;
}

TEST_F(ipmiutilAgent_tests, invalid_file)
{
    ipmiutilAgent *ptr = new ipmiutilAgent("unexistent");
    delete ptr;
}


TEST_F(ipmiutilAgent_tests, getBmcList)
{
    set<string> bmcList = agent->getBmcList("localhost");
    ASSERT_TRUE(bmcList.empty());
    assertHostNameListIsConsistent(bmcList);
}

TEST_F(ipmiutilAgent_tests, getSelRecords_invalidbmc)
{
    mocks[IPMI_CMD].pushState(FAILURE);
    agent->sendCommand(GETSELRECORDS, NULL, "bmc");
}

TEST_F(ipmiutilAgent_tests, getSelRecords_validbmc)
{
    mocks[IPMI_CMD].pushState(FAILURE);
    agent->sendCommand(GETSELRECORDS, NULL, "cn01");
}

TEST_F(ipmiutilAgent_tests, getSelRecords_wrong_address)
{
    mocks[STRDUP].pushState(FAILURE);
    mocks[STRDUP].pushState(FAILURE);
    mocks[STRDUP].pushState(FAILURE);
    mocks[IPMI_CMD].pushState(FAILURE);
    agent->sendCommand(GETSELRECORDS, NULL, "cn01");
}

TEST_F(ipmiutilAgent_tests, sendCommand)
{
    mocks[SET_LAN_OPTIONS].pushState(SUCCESS);
    ASSERT_NO_THROW(agent->sendCommand(DUMMY, NULL, PROBE_BMC));
}

TEST_F(ipmiutilAgent_tests, unableToSetLanOptions)
{
    mocks[SET_LAN_OPTIONS].pushState(FAILURE);
    ASSERT_THROW(agent->sendCommand(DUMMY, NULL, PROBE_BMC), badConnectionParameters);
}

TEST_F(ipmiutilAgent_tests, requestDeviceId)
{
    mocks[IPMI_CMD_MC].pushState(SUCCESS);
    ipmiResponse response = agent->sendCommand(GETDEVICEID, &emptyBuffer, PROBE_BMC);
    ASSERT_TRUE(response.wasSuccessful());
}

TEST_F(ipmiutilAgent_tests, requestAcpiPower)
{
    mocks[IPMI_CMD_MC].pushState(SUCCESS);
    ipmiResponse response = agent->sendCommand(GETACPIPOWER, &emptyBuffer, PROBE_BMC);
    ASSERT_TRUE(response.wasSuccessful());
}

TEST_F(ipmiutilAgent_tests, requestSensorList)
{
    mocks[GET_SDR_CACHE].pushState(SUCCESS);
    ipmiResponse response = agent->sendCommand(GETSENSORLIST, &emptyBuffer, PROBE_BMC);
    ASSERT_TRUE(response.wasSuccessful());
}

TEST_F(ipmiutilAgent_tests, requestSensorList_negative)
{
    mocks[GET_SDR_CACHE].pushState(FAILURE);
    ipmiResponse response = agent->sendCommand(GETSENSORLIST, &emptyBuffer, PROBE_BMC);
    ASSERT_FALSE(response.wasSuccessful());
}

TEST_F(ipmiutilAgent_tests, requestSensorReadings)
{
    mocks[GET_SDR_CACHE].pushState(SUCCESS);
    ipmiResponse response = agent->sendCommand(GETSENSORREADINGS, &emptyBuffer, PROBE_BMC);
    ASSERT_TRUE(response.wasSuccessful());
}

TEST_F(ipmiutilAgent_tests, requestSensorReadings_negative)
{
    mocks[GET_SDR_CACHE].pushState(FAILURE);
    ipmiResponse response = agent->sendCommand(GETSENSORREADINGS, &emptyBuffer, PROBE_BMC);
    ASSERT_FALSE(response.wasSuccessful());
}

TEST_F(ipmiutilAgent_tests, requestFruInventory_Failure)
{
    mocks[IPMI_CMD].pushState(FAILURE);
    ipmiResponse response = agent->sendCommand(READFRUDATA, &emptyBuffer, PROBE_BMC);
    ASSERT_FALSE(response.wasSuccessful());
}

TEST_F(ipmiutilAgent_tests, requestFruInventory_Success)
{
    for (int i = 0; i < MAX_FRU_DEVICES; i++ ) mocks[IPMI_CMD].pushState(SUCCESS);
    ipmiResponse response = agent->sendCommand(READFRUDATA, &emptyBuffer, PROBE_BMC);
    mocks[IPMI_CMD].restartMock();
    ASSERT_TRUE(response.wasSuccessful());
}

TEST_F(ipmiutilAgent_tests, requestFruInventory_getFruData_Failure)
{
    for (int i = 0; i < MAX_FRU_DEVICES; i++ ) mocks[IPMI_CMD].pushState(RETURN_FRU_AREA_1);
    for (int i = 0; i < 15; i++ ) mocks[IPMI_CMD].pushState(FAILURE);

    ipmiResponse response = agent->sendCommand(READFRUDATA, &emptyBuffer, PROBE_BMC);

    mocks[IPMI_CMD].restartMock();
    ASSERT_FALSE(response.wasSuccessful());
}

TEST_F(ipmiutilAgent_tests, requestFruInventory_getFruData_Success)
{
    for (int i = 0; i < MAX_FRU_DEVICES/2; i++ ) mocks[IPMI_CMD].pushState(RETURN_FRU_AREA_1);
    for (int i = 0; i < MAX_FRU_DEVICES/2; i++ ) mocks[IPMI_CMD].pushState(RETURN_FRU_AREA_2);
    for (int i = 0; i < AREA_FRU_PAGE_SIZE; i++ ) mocks[IPMI_CMD].pushState(SUCCESS);

    ipmiResponse response = agent->sendCommand(READFRUDATA, &emptyBuffer, PROBE_BMC);

    mocks[IPMI_CMD].restartMock();
    ASSERT_TRUE(response.wasSuccessful());
}

TEST_F(ipmiutilAgent_tests, requestFruInventory_isNewAreaLarger)
{
    for (int i = 0; i < MAX_FRU_DEVICES/2; i++ ) mocks[IPMI_CMD].pushState(RETURN_FRU_AREA_1);
    for (int i = 0; i < MAX_FRU_DEVICES/2; i++ ) mocks[IPMI_CMD].pushState(RETURN_FRU_AREA_2);
    for (int i = 0; i < AREA_FRU_PAGE_SIZE; i++ ) mocks[IPMI_CMD].pushState(SUCCESS);

    ipmiResponse response = agent->sendCommand(READFRUDATA, &emptyBuffer, PROBE_BMC);
    mocks[IPMI_CMD].restartMock();
    ASSERT_TRUE(response.getReadings().count() > 0);
}

TEST_F(ipmiutilAgent_tests, requestFruInventory_requestNodepower)
{
    mocks[SET_LAN_OPTIONS].pushState(SUCCESS);
    mocks[IPMI_CMDRAW].pushState(RETRIEVE_BUFFER_1);
    mocks[IPMI_CMDRAW].pushState(RETRIEVE_BUFFER_1);
    ipmiResponse response = agent->sendCommand(GETPSUPOWER, &emptyBuffer, PROBE_BMC);
    mocks[IPMI_CMDRAW].restartMock();
    mocks[SET_LAN_OPTIONS].restartMock();
    EXPECT_GT(response.getReadings().count(), 0);
    EXPECT_GT(response.getReadings().getValue<uint64_t>("accu_a"), 0);
    EXPECT_GT(response.getReadings().getValue<uint64_t>("accu_b"), 0);
    EXPECT_GT(response.getReadings().getValue<uint64_t>("cnt_a"), 0);
    EXPECT_GT(response.getReadings().getValue<uint64_t>("cnt_b"), 0);
}

TEST_F(ipmiutilAgent_tests, requestFruInventory_requestNodepower_onlyFirstPSU)
{
    mocks[SET_LAN_OPTIONS].pushState(SUCCESS);
    mocks[IPMI_CMDRAW].pushState(RETRIEVE_BUFFER_1);
    mocks[IPMI_CMDRAW].pushState(SUCCESS);
    ipmiResponse response = agent->sendCommand(GETPSUPOWER, &emptyBuffer, PROBE_BMC);
    mocks[IPMI_CMDRAW].restartMock();
    mocks[SET_LAN_OPTIONS].restartMock();
    EXPECT_GT(response.getReadings().count(), 0);
    EXPECT_GT(response.getReadings().getValue<uint64_t>("accu_a"), 0);
    EXPECT_EQ(response.getReadings().getValue<uint64_t>("accu_b"), 0);
    EXPECT_GT(response.getReadings().getValue<uint64_t>("cnt_a"), 0);
    EXPECT_EQ(response.getReadings().getValue<uint64_t>("cnt_b"), 0);
}

TEST_F(ipmiutilAgent_tests, requestFruInventory_requestNodepower_onlySecondPSU)
{
    mocks[SET_LAN_OPTIONS].pushState(SUCCESS);
    mocks[IPMI_CMDRAW].pushState(SUCCESS);
    mocks[IPMI_CMDRAW].pushState(RETRIEVE_BUFFER_1);
    ipmiResponse response = agent->sendCommand(GETPSUPOWER, &emptyBuffer, PROBE_BMC);
    mocks[IPMI_CMDRAW].restartMock();
    mocks[SET_LAN_OPTIONS].restartMock();
    EXPECT_GT(response.getReadings().count(), 0);
    EXPECT_EQ(response.getReadings().getValue<uint64_t>("accu_a"), 0);
    EXPECT_GT(response.getReadings().getValue<uint64_t>("accu_b"), 0);
    EXPECT_EQ(response.getReadings().getValue<uint64_t>("cnt_a"), 0);
    EXPECT_GT(response.getReadings().getValue<uint64_t>("cnt_b"), 0);
}

TEST_F(ipmiutilAgent_tests, requestFruInventory_requestNodepower_NoPSU)
{
    mocks[SET_LAN_OPTIONS].pushState(SUCCESS);
    mocks[IPMI_CMDRAW].pushState(SUCCESS);
    mocks[IPMI_CMDRAW].pushState(SUCCESS);
    ipmiResponse response = agent->sendCommand(GETPSUPOWER, &emptyBuffer, PROBE_BMC);
    mocks[IPMI_CMDRAW].restartMock();
    mocks[SET_LAN_OPTIONS].restartMock();
    EXPECT_GT(response.getReadings().count(), 0);
    EXPECT_EQ(response.getReadings().getValue<uint64_t>("accu_a"), 0);
    EXPECT_EQ(response.getReadings().getValue<uint64_t>("accu_b"), 0);
    EXPECT_EQ(response.getReadings().getValue<uint64_t>("cnt_a"), 0);
    EXPECT_EQ(response.getReadings().getValue<uint64_t>("cnt_b"), 0);
}

TEST_F(ipmiutilAgent_extra, errorException)
{
    // The following values comes from the error codes defined by ipmiutil
    const int rc = -504;
    const string errMessage = "error getting msg from BMC";

    try {
        throw baseException("", rc, baseException::ERROR);
    } catch(runtime_error e) {
        ASSERT_TRUE(0 == errMessage.compare(e.what()));
    } catch(...) {
        FAIL();
    }
}

TEST_F(ipmiutilAgent_extra, completionException)
{
    // The following values comes from the error codes defined by ipmiutil
    const int cc = 255;
    const string completionMessage = "Unspecified error";

    try {
        throw baseException("", cc, baseException::COMPLETION);
    } catch(runtime_error e) {
        ASSERT_TRUE(0 == completionMessage.compare(e.what()));
    } catch(...) {
        FAIL();
    }
}

TEST_F(ipmiutilAgent_extra, functions_for_message_interpretation)
{
    EXPECT_TRUE(getErrorMessage(1).empty());
    EXPECT_TRUE(getCompletionMessage(-1).empty());
}
