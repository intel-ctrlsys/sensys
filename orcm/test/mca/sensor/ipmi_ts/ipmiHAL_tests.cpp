/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmiHAL_tests.h"

#include "orcm/mca/sensor/ipmi_ts/ipmiLibInterface.h"
#include "orcm/mca/sensor/ipmi_ts/ipmiutilDFx.h"

using namespace std;

#define assertTimedOut(f, TO, INV) \
    {                                                           \
        bool timedOut = false;                                  \
        time_t begin;                                           \
        time(&begin);                                           \
        while (f!=INV && !(timedOut = elapsedSecs(begin) > TO)) \
            usleep(100);                                        \
        ASSERT_FALSE(timedOut);                                 \
    }

void HAL::SetUp()
{
    opal_init_test();
    // TODO set MCA parameters to use DFx

    HWobject = ipmiHAL::getInstance();

    callbackFlag = false;
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

void HAL::cbFunc(string bmc, ipmiResponse_t response, void* cbData)
{
    HAL* ptr = (HAL*) cbData;
    ptr->callbackFlag = true;
}

TEST_F(HAL, singleton_instance)
{
    ASSERT_TRUE(HWobject);
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