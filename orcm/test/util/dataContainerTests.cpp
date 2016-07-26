/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "dataContainerTests.h"
#include <sys/time.h>

using namespace std;

/**
 * The following templates perform the following internal validations:
 *   - Data type is stored correctly
 *   - Data is using just the amount of memory required
 *   - The data type name is stored correctly and can be retrieved
 *   - The units are being stored correctly and can be retrieved
 */
template<typename T> T putAndGet(dataContainer* cnt,
                                 const char key[],
                                 const char units[],
                                 T data) {
    string label(key);
    string uLabel(units);
    cnt->put(label, data, uLabel);
    EXPECT_TRUE(cnt->matchType<T>(label));
    EXPECT_EQ(sizeof(T), cnt->getDataSize(key));
    EXPECT_TRUE( 0 == cnt->getDataTypeName(key).compare(typeid(T).name()) );
    EXPECT_TRUE( 0 == cnt->getUnits(key).compare(uLabel) );
    return cnt->getValue<T>(label);
}

template <> string putAndGet(dataContainer* cnt,
                             const char key[],
                             const char units[],
                             string data) {
    string label(key);
    string uLabel(units);
    cnt->put(label, data, units);
    EXPECT_TRUE(cnt->matchType<string>(label));
    EXPECT_EQ(sizeof(char)*(data.size()+1), cnt->getDataSize(key));
    EXPECT_TRUE( 0 == cnt->getDataTypeName(key).compare(typeid(string).name()) );
    EXPECT_TRUE( 0 == cnt->getUnits(key).compare(uLabel) );
    return cnt->getValue<string>(label);
}

// The following tests check the correct storage of the supported data types.
TEST_F(utDataContainer, putAndGetBooleanValues) {
    EXPECT_EQ(true, putAndGet<bool>(cnt, "boolValue_1", "booleans", true));
    EXPECT_EQ(false, putAndGet<bool>(cnt, "boolValue_2", "booleans", false));
}

TEST_F(utDataContainer, putAndGetIntegerValues) {
    EXPECT_EQ(1, putAndGet<int>(cnt, "intValue_1", "ints", 1));
    EXPECT_EQ(2, putAndGet<int>(cnt, "intValue_2", "ints", 2));
}

TEST_F(utDataContainer, putAndGetFloatValues) {
    EXPECT_FLOAT_EQ(1.0, putAndGet<float>(cnt, "floatValue_1", "floats", 1.0));
    EXPECT_FLOAT_EQ(3.1416, putAndGet<float>(cnt, "floatValue_2", "floats", 3.1416));
}

TEST_F(utDataContainer, putAndGetDoubleValues) {
    EXPECT_FLOAT_EQ(1.0, putAndGet<double>(cnt, "doubleValue_1", "doubles", 1.0));
    EXPECT_FLOAT_EQ(3.14159265, putAndGet<double>(cnt, "doubletValue_2", "doubles", 3.14159265));
}

TEST_F(utDataContainer, putAndGetSignedIntegerTypes) {
    EXPECT_EQ(1, putAndGet<int8_t>(cnt, "int8Value_1", "ints", 1));
    EXPECT_EQ(2, putAndGet<int8_t>(cnt, "int8Value_2", "ints", 2));

    EXPECT_EQ(1, putAndGet<int16_t>(cnt, "int16Value_1", "ints", 1));
    EXPECT_EQ(2, putAndGet<int16_t>(cnt, "int16Value_2", "ints", 2));

    EXPECT_EQ(1, putAndGet<int32_t>(cnt, "int32Value_1", "ints", 1));
    EXPECT_EQ(2, putAndGet<int32_t>(cnt, "int32Value_2", "ints", 2));

    EXPECT_EQ(1, putAndGet<int64_t>(cnt, "int64Value_1", "ints", 1));
    EXPECT_EQ(2, putAndGet<int64_t>(cnt, "int64Value_2", "ints", 2));
}

TEST_F(utDataContainer, putAndGetIUnsignedIntegerTypes) {
    EXPECT_EQ(1, putAndGet<uint8_t>(cnt, "uint8Value_1", "ints", 1));
    EXPECT_EQ(2, putAndGet<uint8_t>(cnt, "uint8Value_2", "ints", 2));

    EXPECT_EQ(1, putAndGet<uint16_t>(cnt, "uint16Value_1", "ints", 1));
    EXPECT_EQ(2, putAndGet<uint16_t>(cnt, "uint16Value_2", "ints", 2));

    EXPECT_EQ(1, putAndGet<uint32_t>(cnt, "uint32Value_1", "ints", 1));
    EXPECT_EQ(2, putAndGet<uint32_t>(cnt, "uint32Value_2", "ints", 2));

    EXPECT_EQ(1, putAndGet<uint64_t>(cnt, "uint64Value_1", "ints", 1));
    EXPECT_EQ(2, putAndGet<uint64_t>(cnt, "uint64Value_2", "ints", 2));
}

TEST_F(utDataContainer, putAndGetTimeval) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    struct timeval tv = putAndGet<struct timeval>(cnt, "timestamp", "tv", current_time);
    EXPECT_EQ( current_time.tv_sec, tv.tv_sec );
    EXPECT_EQ( current_time.tv_usec, tv.tv_usec );
}

TEST_F(utDataContainer, putAndGetStrings) {
    string stringProbe("Hello, World!");
    EXPECT_EQ(0, stringProbe.compare(putAndGet(cnt, "label", "strings", stringProbe)) );
}

TEST_F(utDataContainerIteration, eraseItem) {
    EXPECT_TRUE( cnt->containsKey(probeValue) );
    EXPECT_NO_THROW( cnt->erase(probeValue) );
    EXPECT_FALSE( cnt->containsKey(probeValue) );

    EXPECT_EQ(numberOfElements - 1, cnt->count());
    ASSERT_THROW( cnt->getValue<int>(probeValue), unableToFindKey );
}

TEST_F(utDataContainerIteration, iterateItems) {
    EXPECT_EQ( 0, cnt->getKey(it).compare(probeValue) );
    EXPECT_EQ( 3, cnt->getValue<int>(it) );
}

TEST_F(utDataContainerIteration, iterateAndErase) {
    EXPECT_TRUE( cnt->containsKey(probeValue) );
    EXPECT_NO_THROW( cnt->erase(it) );
    EXPECT_FALSE( cnt->containsKey(probeValue) );

    EXPECT_EQ(numberOfElements - 1, cnt->count());
    ASSERT_THROW( cnt->getValue<int>(probeValue), unableToFindKey );
}

TEST_F(utDataContainerIteration, iterateAllItems) {
    int n = 0;

    for (it = cnt->begin(); it != cnt->end(); it++) {
        ++n;
    }

    EXPECT_EQ(cnt->count(), n);
}

TEST(utDataHolder, emptyConstructor) {
    dataHolder h;
    EXPECT_TRUE(h.getUnits().empty());
    EXPECT_TRUE(h.getDataTypeName().empty());
    EXPECT_EQ(0, h.size());
}

TEST(utDataHolder, getPointer) {
    int intValue = 3;
    dataHolder h(intValue);
    EXPECT_EQ(intValue, *h.ptr<int>());
}
