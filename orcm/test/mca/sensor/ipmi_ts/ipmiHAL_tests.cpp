/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmiHAL_tests.h"
#include "ipmiHAL_mocks.h"

#include "orcm/mca/sensor/ipmi_ts/ipmiLibInterface.h"
#include "orcm/mca/sensor/ipmi_ts/ipmiutilDFx.h"
#include "orcm/mca/sensor/ipmi_ts/ipmi_test_sensor/ipmi_test_sensor.hpp"

extern "C" {
    #include "orcm/mca/parser/base/base.h"
    #include "orcm/mca/parser/pugi/parser_pugi.h"
    #include "orcm/util/utils.h"
}

using namespace std;

void HAL::SetUp()
{
    opal_init_test();

    setenv(MCA_DFX_FLAG.c_str(), "1", 1);

    mocks[OPAL_EVENT_EVTIMER_NEW].restartMock();

    HWobject = ipmiHAL::getInstance();

    callbackFlag = false;
    isSensorListEmpty = true;
    isSampleContainerEmpty = true;

    emptyBuffer = new buffer();
}

void HAL::TearDown()
{
    setenv(MCA_DFX_FLAG.c_str(), "1", 1);
    ipmiHAL::terminateInstance();
    delete emptyBuffer;
}

double HAL::elapsedSecs(time_t startTime) {
    time_t now;
    time(&now);
    return difftime(now, startTime);
}

void HAL::cbFunc(string bmc, ipmiResponse response, void* cbData)
{
    HAL* ptr = (HAL*) cbData;
    ptr->callbackFlag = true;
}

void HAL::cbFunc_sensorList(string bmc, ipmiResponse response, void* cbData)
{
    HAL* ptr = (HAL*) cbData;
    ptr->isSensorListEmpty = (0 == response.getReadings().count());
    ptr->callbackFlag = true;
}

void HAL::cbFunc_readings(string bmc, ipmiResponse response, void* cbData)
{
    HAL* ptr = (HAL*) cbData;
    ptr->isSampleContainerEmpty = (0 == response.getReadings().count());
    ptr->callbackFlag = true;
}

TEST_F(HAL, singleton_instance)
{
    ASSERT_TRUE(HWobject);
    ipmiHAL *anotherHWobject = ipmiHAL::getInstance();
    ASSERT_TRUE( HWobject == anotherHWobject );
}

TEST_F(HAL, request_is_added_to_queue)
{
    EXPECT_TRUE(HWobject->isQueueEmpty());
    ASSERT_NO_THROW(HWobject->addRequest(DUMMY, *emptyBuffer, bmc, NULL, NULL));
    ASSERT_FALSE(HWobject->isQueueEmpty());
}

TEST_F(HAL, several_requests_are_processed)
{
    EXPECT_TRUE(HWobject->isQueueEmpty());
    ASSERT_NO_THROW(HWobject->addRequest(DUMMY, *emptyBuffer, bmc, NULL, NULL));
    ASSERT_NO_THROW(HWobject->addRequest(DUMMY, *emptyBuffer, bmc, NULL, NULL));
    ASSERT_NO_THROW(HWobject->addRequest(DUMMY, *emptyBuffer, bmc, NULL, NULL));
    EXPECT_FALSE(HWobject->isQueueEmpty());

    HWobject->startAgents();
    assertTimedOut(HWobject->isQueueEmpty(), TIMEOUT, true);
}

TEST_F(HAL, callback_data_is_correctly_passed)
{
    EXPECT_TRUE(HWobject->isQueueEmpty());
    ASSERT_NO_THROW(HWobject->addRequest(DUMMY, *emptyBuffer, bmc, NULL, NULL));
    ASSERT_NO_THROW(HWobject->addRequest(DUMMY, *emptyBuffer, bmc, NULL, NULL));
    ASSERT_NO_THROW(HWobject->addRequest(DUMMY, *emptyBuffer, bmc, cbFunc, this));

    HWobject->startAgents();
    assertTimedOut(callbackFlag, TIMEOUT, true);
}

TEST_F(HAL, request_sensor_list)
{
    EXPECT_TRUE(HWobject->isQueueEmpty());
    EXPECT_TRUE(isSensorListEmpty);
    ASSERT_NO_THROW(HWobject->addRequest(GETSENSORLIST, *emptyBuffer, bmc, cbFunc_sensorList, this));

    HWobject->startAgents();
    assertTimedOut(callbackFlag, TIMEOUT, true);

    ASSERT_FALSE(isSensorListEmpty);
}

TEST_F(HAL, request_sensor_readings)
{
    EXPECT_TRUE(HWobject->isQueueEmpty());
    EXPECT_TRUE(isSampleContainerEmpty);
    ASSERT_NO_THROW(HWobject->addRequest(GETSENSORREADINGS, *emptyBuffer, bmc, cbFunc_readings, this));

    HWobject->startAgents();
    assertTimedOut(callbackFlag, TIMEOUT, true);

    ASSERT_FALSE(isSampleContainerEmpty);
}

TEST_F(HAL, non_double_start_of_agents)
{
    HWobject->startAgents();
    HWobject->startAgents();
}

TEST_F(HAL, unable_to_allocate_event)
{
    mocks[OPAL_EVENT_EVTIMER_NEW].pushState(FAILURE);
    ASSERT_THROW(HWobject->addRequest(DUMMY, *emptyBuffer, bmc, NULL, NULL), std::runtime_error);
}

TEST_F(HAL, retrieve_bmc_list)
{
    set<string> bmcList = HWobject->getBmcList("localhost");
    ASSERT_FALSE(bmcList.empty());
}

TEST_F(EXTRA, terminate_without_instance)
{
    ASSERT_NO_THROW(ipmiHAL::terminateInstance());
}

TEST_F(EXTRA, throw_when_null_pointer)
{
    void* nullPtr = NULL;
    int probeValue = 5;
    int* nonNullPtr = &probeValue;

    EXPECT_NO_THROW(ipmiHAL::throwWhenNullPointer(nonNullPtr));
    ASSERT_THROW(ipmiHAL::throwWhenNullPointer(nullPtr), std::runtime_error);
}

TEST_F(EXTRA, response_object)
{
    static const string errMessage("Custom error message");
    static const string completionMessage("Custom completion message");

    buffer testBuffer;
    testBuffer.push_back(0x00);
    ipmiResponse response(&testBuffer, errMessage, completionMessage, true);

    EXPECT_TRUE(response.wasSuccessful());
    EXPECT_TRUE(0 == response.getErrorMessage().compare(errMessage));
    EXPECT_TRUE(0 == response.getCompletionMessage().compare(completionMessage));
    EXPECT_FALSE(response.getResponseBuffer().empty());
}

void TEST_SENSOR::SetUp()
{
    sensor = new ipmiSensorInterface("bmc");
}

void TEST_SENSOR::TearDown()
{
    delete sensor;

}

TEST_F(TEST_SENSOR, init_method)
{
    ASSERT_NO_THROW(sensor->init());
}

TEST_F(TEST_SENSOR, sample_method)
{
    ASSERT_NO_THROW(sensor->sample());
}

TEST_F(TEST_SENSOR, collect_inventory_method)
{
    ASSERT_NO_THROW(sensor->collect_inventory());
}

TEST_F(TEST_SENSOR, finalize_method)
{
    ASSERT_NO_THROW(sensor->finalize());
}

TEST_F(TEST_SENSOR, get_hostname_method)
{
    ASSERT_TRUE(0 == sensor->getHostname().compare("bmc"));
}

void TEST_SENSOR_DFX::SetUp()
{
    sensor = (ipmiSensorInterface*) new IpmiTestSensor("bmc");
}

void TEST_SENSOR_DFX::TearDown()
{
    delete sensor;

}

TEST_F(TEST_SENSOR_DFX, init_method)
{
    ASSERT_NO_THROW(sensor->init());
}

static void ipmi_callback(std::string hostname, dataContainer* dc)
{
    ASSERT_TRUE( 0 < dc->count() );
}

TEST_F(TEST_SENSOR_DFX, sample_method)
{
    sensor->setSamplingPtr(ipmi_callback);
    ASSERT_NO_THROW(sensor->sample());
}

TEST_F(TEST_SENSOR_DFX, collect_inventory_method)
{
    sensor->setInventoryPtr(ipmi_callback);
    ASSERT_NO_THROW(sensor->collect_inventory());
}

TEST_F(TEST_SENSOR_DFX, finalize_method)
{
    ASSERT_NO_THROW(sensor->finalize());
}

TEST_F(TEST_SENSOR_DFX, get_hostname_method)
{
    ASSERT_TRUE(0 == sensor->getHostname().compare("bmc"));
}

void realHAL::initParserFramework()
{
    OBJ_CONSTRUCT(&orcm_parser_base_framework.framework_components, opal_list_t);
    OBJ_CONSTRUCT(&orcm_parser_base.actives, opal_list_t);
    orcm_parser_active_module_t *act_module;
    act_module = OBJ_NEW(orcm_parser_active_module_t);
    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_pugi_module;
    opal_list_append(&orcm_parser_base.actives, &act_module->super);
}

void realHAL::cleanParserFramework()
{
    OPAL_LIST_DESTRUCT(&orcm_parser_base_framework.framework_components);
    OPAL_LIST_DESTRUCT(&orcm_parser_base.actives);
}

void realHAL::setFileName()
{
    char* srcdir = getenv("srcdir");
    if (NULL != srcdir){
        fileName = string(srcdir) + "/ipmi.xml";
    } else {
        fileName = "ipmi.xml";
    }
}

void realHAL::SetUp()
{
    opal_init_test();

    initParserFramework();
    setFileName();

    orcm_cfgi_base.config_file = strdup(fileName.c_str());
}

void realHAL::TearDown()
{
    cleanParserFramework();
}

TEST_F(realHAL, without_MCA_parameter)
{
    ipmiHAL *HWobject = ipmiHAL::getInstance();
}

TEST_F(realHAL, with_MCA_parameter)
{
    static const char MCA_DFX_FLAG[] = "ORCM_MCA_sensor_ipmi_ts_dfx";
    setenv(MCA_DFX_FLAG, "0", 1);

    ipmiHAL *HWobject = ipmiHAL::getInstance();

    setenv(MCA_DFX_FLAG, "1", 1);
}
