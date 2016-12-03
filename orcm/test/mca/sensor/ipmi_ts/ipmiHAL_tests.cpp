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

using namespace std;

void HAL::SetUp()
{
    opal_init_test();
    // TODO set MCA parameters to use DFx

    mocks[OPAL_EVENT_EVTIMER_NEW].restartMock();

    HWobject = ipmiHAL::getInstance();

    callbackFlag = false;
    isSensorListEmpty = true;
    isSampleContainerEmpty = true;

    emptyBuffer = new buffer();
}

void HAL::TearDown()
{
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
    ptr->isSensorListEmpty = response.getSensorList().empty();
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

TEST_F(HAL, request_is_taken_from_queue)
{
    EXPECT_TRUE(HWobject->isQueueEmpty());
    ASSERT_NO_THROW(HWobject->addRequest(DUMMY, *emptyBuffer, bmc, NULL, NULL));
    EXPECT_FALSE(HWobject->isQueueEmpty());
    HWobject->startAgents();
    ASSERT_TRUE(HWobject->isQueueEmpty());
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
    ASSERT_NO_THROW(HWobject->addRequest(DUMMY, *emptyBuffer, bmc, NULL, NULL));
}

TEST_F(HAL, retrieve_bmc_list)
{
    set<string> bmcList = HWobject->getBmcList();
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