/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <stdexcept>
#include "dataContainerHelperTests.h"
#include "orcm/common/dataContainerHelper.hpp"
#include "opal/runtime/opal.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orcm/util/utils.h"

#include <sstream>

#define OPAL_EPS (double) 1e-6

dssMocks mock;

extern "C" {
    int packMock_(opal_buffer_t *buffer,
                  const void *src,
                  int32_t num_vals,
                  opal_data_type_t type) {
        if (mock.mockPack()) {
            return OPAL_ERROR;
        }
        return mock.real_pack(buffer, src, num_vals, type);
    }

    int unpackMock_(opal_buffer_t *buffer,
                    void *dst,
                    int32_t *num_vals,
                    opal_data_type_t type) {
        if (mock.mockUnpack()) {
            return OPAL_ERROR;
        }
        return mock.real_unpack(buffer, dst, num_vals, type);
    }

    int __wrap_orcm_util_append_orcm_value(opal_list_t *list, char *key, void *data,
                                           opal_data_type_t type, char *units) {
        if (mock.mockAppendOrcmValue()) {
            return OPAL_ERROR;
        }
        return __real_orcm_util_append_orcm_value(list, key, data, type, units);
    }
}

dssMocks::dssMocks() {
    packPtr = opal_dss.pack;
    unpackPtr = opal_dss.unpack;
    opal_dss.pack = packMock_;
    opal_dss.unpack = unpackMock_;
}

dssMocks::~dssMocks() {
    opal_dss.pack = packPtr;
    opal_dss.unpack = unpackPtr;
}

void dssMocks::setupPackMock(int n) {
    packCount = 0;
    packLimit = n;
}

void dssMocks::setupUnpackMock(int n) {
    unpackCount = 0;
    unpackLimit = n;
}

void dssMocks::setupAppendOrcmValueMock(int n) {
    appendOrcmValueCount = 0;
    appendOrcmValueLimit = n;
}


void dssMocks::teardownMocks() {
    packLimit = 0;
    unpackLimit = 0;
    appendOrcmValueLimit = 0;
}


dataContainerHelperTests::dataContainerHelperTests() {
    opal_init_test();

    cnt = new dataContainer();
    cnt->put("intValue", (int) 3, "ints");
    cnt->put("floatValue", (float) 3.14, "floats");
    cnt->put("doubleValue", (double) 3.14159265, "doubles");

    dataContainer cnt2;
    dataContainer* cnt3 = new dataContainer();
    cnt3->put("intValue", (int) 5, "units-ints");

    cntMap["cnt1"] = *cnt;
    cntMap["cnt2"] = cnt2;
    cntMap["cnt3"] = *cnt3;

    buffer = OBJ_NEW(opal_buffer_t);
}

dataContainerHelperTests::~dataContainerHelperTests() {
    ORCM_RELEASE(buffer);
    delete cnt;
}

void dataContainerHelperTests::unpackKeyUnits(opal_buffer_t* buffer, char** key, char** units) {
    int number = 1;

    number = 1;
    EXPECT_EQ(OPAL_SUCCESS, opal_dss.unpack(buffer, key, &number, OPAL_STRING));

    number = 1;
    EXPECT_EQ(OPAL_SUCCESS, opal_dss.unpack(buffer, units, &number, OPAL_STRING));
}

void dataContainerHelperTests::packKeyUnits(opal_buffer_t* buffer, char* key, char* units) {
    int number = 1;

    EXPECT_EQ(OPAL_SUCCESS, opal_dss.pack(buffer, key, number, OPAL_STRING));
    EXPECT_EQ(OPAL_SUCCESS, opal_dss.pack(buffer, units, number, OPAL_STRING));
}

void dataContainerHelperTests::packDummyUnsupportedData(opal_buffer_t* buffer) {
    int number = 1;
    opal_data_type_t type = OPAL_DATA_TYPE;

#if !OPAL_ENABLE_DEBUG
    EXPECT_EQ(OPAL_SUCCESS, opal_dss.pack(buffer, &type, number, OPAL_DATA_TYPE));
#endif

    EXPECT_EQ(OPAL_SUCCESS, opal_dss.pack(buffer, &type, number, OPAL_DATA_TYPE));
}

template<typename T> void dataContainerHelperTests::unpackAndCheck(const dataContainer& cnt,
                                                                   dataContainer::iterator& it,
                                                                   opal_buffer_t* buffer,
                                                                   opal_data_type_t type) {
    T testValue;
    int number = 1;
#if !OPAL_ENABLE_DEBUG
    opal_data_type_t dataType;
    EXPECT_EQ(OPAL_SUCCESS, opal_dss.unpack(buffer, &dataType, &number, OPAL_DATA_TYPE_T));
    number = 1;
#endif
    EXPECT_EQ(OPAL_SUCCESS, opal_dss.unpack(buffer, &testValue, &number, type));
    EXPECT_NEAR(cnt.getValue<T>(it), testValue, OPAL_EPS);
}

void dataContainerHelperTests::checkPackedData(const dataContainer& cnt,
                                                   dataContainer::iterator& it,
                                                   opal_buffer_t* buffer) {
    std::string dataTypeName = cnt.getDataTypeName(it);

    if (cnt.matchType<int>(it)) {
        unpackAndCheck<int>(cnt, it, buffer, OPAL_INT32);
    } else if (cnt.matchType<float>(it)) {
        unpackAndCheck<float>(cnt, it, buffer, OPAL_FLOAT);
    } else if (cnt.matchType<double>(it)) {
        unpackAndCheck<double>(cnt, it, buffer, OPAL_DOUBLE);
    } else {
        FAIL() << "Wrong data type!" ;
    }
}

template<typename T> void dataContainerHelperTests::compareItems(const dataContainer& cnt,
                                                                 dataContainer::iterator& it,
                                                                 std::string key,
                                                                 dataContainer& dst) {
    EXPECT_TRUE(dst.matchType<T>(key));
    EXPECT_EQ(0, dst.getUnits(key).compare(cnt.getUnits(it)));
    EXPECT_NEAR(dst.getValue<T>(key), cnt.getValue<T>(it), OPAL_EPS);
}

template <> void dataContainerHelperTests::compareItems<struct timeval>(const dataContainer& cnt,
                                                                        dataContainer::iterator& it,
                                                                        std::string key,
                                                                        dataContainer& dst) {
    EXPECT_TRUE(dst.matchType<struct timeval>(key));
    EXPECT_EQ(0, dst.getUnits(key).compare(cnt.getUnits(it)));

    struct timeval dstTv = dst.getValue<struct timeval>(key);
    struct timeval srcTv = cnt.getValue<struct timeval>(it);

    EXPECT_EQ(dstTv.tv_sec, srcTv.tv_sec);
    EXPECT_EQ(dstTv.tv_usec, srcTv.tv_usec);
}

template <> void dataContainerHelperTests::compareItems<std::string>(const dataContainer& cnt,
                                                                     dataContainer::iterator& it,
                                                                     std::string key,
                                                                     dataContainer& dst) {
    EXPECT_TRUE(dst.matchType<std::string>(key));
    EXPECT_EQ(0, dst.getUnits(key).compare(cnt.getUnits(it)));
    EXPECT_EQ(0, dst.getValue<std::string>(key).compare(cnt.getValue<std::string>(it)));
}

void dataContainerHelperTests::serializationMapExceptionTester(const int rc, const std::string& message, serializationMapMethods f) {
    try {
        f(cntMap, buffer);
        FAIL() << "Exception was not thrown!";
    } catch(ErrOpal &e) {
        EXPECT_EQ(0, message.compare(e.what()));
        EXPECT_EQ(rc, e.getRC());
    } catch(std::exception &e) {
        FAIL() << "Unknown exception catched.";
    }
}

void dataContainerHelperTests::serializationExceptionTester(const int rc, const std::string& message, serializationMethods f) {
    try {
        f(*cnt, buffer);
        FAIL() << "Exception was not thrown!";
    } catch(ErrOpal &e) {
        EXPECT_EQ(0, message.compare(e.what()));
        EXPECT_EQ(rc, e.getRC());
    } catch(std::exception &e) {
        FAIL() << "Unknown exception catched.";
    }
}

void dataContainerHelperTests::dataContainerToListExceptionTester(const int rc, const std::string& message, opal_list_t* list) {
    try {
        dataContainerHelper::dataContainerToList(*cnt, (void*)list);
        FAIL() << "Exception was not thrown!";
    } catch(ErrOpal &e) {
        EXPECT_EQ(0, message.compare(e.what()));
        EXPECT_EQ(rc, e.getRC());
    } catch(std::exception &e) {
        FAIL() << "Unknown exception catched.";
    }
}


dataContainerHelperFullDataTypeTests::dataContainerHelperFullDataTypeTests() {
    opal_init_test();

    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    cnt = new dataContainer();
    cnt->put("boolValue", (bool) true, "booleans");
    cnt->put("int8Value", (int8_t) -8, "int8s");
    cnt->put("int16Value", (int16_t) -16, "int16s");
    cnt->put("int32Value", (int32_t) -32, "int32s");
    cnt->put("int64Value", (int64_t) -64, "int64s");
    cnt->put("uint8Value", (uint8_t) 8, "uint8s");
    cnt->put("uint16Value", (uint16_t) 16, "uint16s");
    cnt->put("uint32Value", (uint32_t) 32, "uint32s");
    cnt->put("uint64Value", (uint64_t) 64, "uint64s");
    cnt->put("floatValue", (float) 3.14, "floats");
    cnt->put("doubleValue", (double) 3.14159265, "doubles");
    cnt->put("timevalValue", current_time, "timestamp");
    cnt->put("stringValue", std::string("Hello, World!"), "strings");

    buffer = OBJ_NEW(opal_buffer_t);
}

void dataContainerHelperTests::verifyItem(orcm_value_t* item, dataContainer::iterator& it){
    EXPECT_EQ(0, cnt->getKey(it).compare(item->value.key));
    EXPECT_EQ(0, cnt->getUnits(it).compare(item->units));
    switch (item->value.type) {
        case OPAL_STRING:
            EXPECT_EQ(0, cnt->getValue<std::string>(it->first).compare(item->value.data.string));
            break;
        case OPAL_TIMEVAL:
            EXPECT_EQ(item->value.data.tv.tv_sec, cnt->getValue<struct timeval>(it).tv_sec);
            EXPECT_EQ(item->value.data.tv.tv_usec, cnt->getValue<struct timeval>(it).tv_usec);
            break;
        case OPAL_DOUBLE:
            EXPECT_NEAR(cnt->getValue<double>(it), item->value.data.dval, OPAL_EPS);
            break;
        case OPAL_FLOAT:
            EXPECT_NEAR(cnt->getValue<float>(it), item->value.data.fval, OPAL_EPS);
            break;
        case OPAL_UINT64:
            EXPECT_EQ(cnt->getValue<uint64_t>(it), item->value.data.uint64);
            break;
        case OPAL_INT64:
            EXPECT_EQ(cnt->getValue<int64_t>(it), item->value.data.int64);
            break;
        case OPAL_UINT32:
            EXPECT_EQ(cnt->getValue<uint32_t>(it), item->value.data.uint32);
            break;
        case OPAL_INT32:
            EXPECT_EQ(cnt->getValue<int32_t>(it), item->value.data.int32);
            break;
        case OPAL_UINT16:
            EXPECT_EQ(cnt->getValue<uint16_t>(it), item->value.data.uint16);
            break;
        case OPAL_INT16:
            EXPECT_EQ(cnt->getValue<int16_t>(it), item->value.data.int16);
            break;
        case OPAL_UINT8:
            EXPECT_EQ(cnt->getValue<uint8_t>(it), item->value.data.uint8);
            break;
        case OPAL_INT8:
            EXPECT_EQ(cnt->getValue<int8_t>(it), item->value.data.int8);
            break;
        case OPAL_BOOL:
            EXPECT_EQ(cnt->getValue<bool>(it), item->value.data.flag);
            break;
        default:
            std::stringstream ss;
            ss <<"Unsupported data type: " <<  (int)item->value.type;
            FAIL() << ss.str();
    }
}

// Test cases

TEST_F(dataContainerHelperTests, packContainerInBuffer) {
    char* key = NULL;
    char* units = NULL;
    double value = 0;

    ASSERT_NO_THROW(dataContainerHelper::serialize(*cnt, buffer));

    for (dataContainer::iterator it = cnt->begin(); it != cnt->end(); it++) {
        unpackKeyUnits(buffer, &key, &units);
        EXPECT_EQ(0, cnt->getKey(it).compare(key));
        EXPECT_EQ(0, cnt->getUnits(it).compare(units));
        SAFEFREE(key);
        SAFEFREE(units);
        checkPackedData(*cnt, it, buffer);
    }
}

TEST_F(dataContainerHelperTests, failingLabelPack) {
    mock.setupPackMock(1);
    serializationExceptionTester(OPAL_ERROR, "Unable to pack string into opal buffer", dataContainerHelper::serialize);
    mock.teardownMocks();
}

TEST_F(dataContainerHelperTests, failingDataPack) {
#if OPAL_ENABLE_DEBUG
    mock.setupPackMock(3);
#else
    mock.setupPackMock(4);
#endif
    serializationExceptionTester(OPAL_ERROR, "Unable to pack data into opal buffer", dataContainerHelper::serialize);
    mock.teardownMocks();
}

#if !OPAL_ENABLE_DEBUG
TEST_F(dataContainerHelperTests, failToPackDataType) {
    mock.setupPackMock(3);
    serializationExceptionTester(OPAL_ERROR, "Unable to pack data type into opal buffer", dataContainerHelper::serialize);
    mock.teardownMocks();
}

TEST_F(dataContainerHelperTests, failToUnpackDataType) {
    mock.setupUnpackMock(3);
    EXPECT_NO_THROW(dataContainerHelper::serialize(*cnt, buffer));
    serializationExceptionTester(OPAL_ERROR, "Unable to unpack data type", dataContainerHelper::deserialize);
    mock.teardownMocks();
}
#endif

TEST_F(dataContainerHelperTests, unpackToContainer) {
    dataContainer dst;

    EXPECT_NO_THROW(dataContainerHelper::serialize(*cnt, buffer));
    EXPECT_NO_THROW(dataContainerHelper::deserialize(dst, buffer));

    for (dataContainer::iterator it = cnt->begin(); it != cnt->end(); ++it) {
        std::string key = cnt->getKey(it);
        EXPECT_TRUE(dst.containsKey(key));

        if (cnt->matchType<int>(it)) {
            compareItems<int>(*cnt, it, key, dst);
        } else if (cnt->matchType<float>(it)) {
            compareItems<float>(*cnt, it, key, dst);
        } else if (cnt->matchType<double>(it)) {
            compareItems<double>(*cnt, it, key, dst);
        } else {
            FAIL() << "Wrong data type!" ;
        }
    }
}

TEST_F(dataContainerHelperTests, failUnpackLabelToContainer) {
    dataContainer dst;

    mock.setupUnpackMock(1);

    EXPECT_NO_THROW(dataContainerHelper::serialize(*cnt, buffer));
    serializationExceptionTester(OPAL_ERROR, "Unable to unpack string from opal buffer", dataContainerHelper::deserialize);
    mock.teardownMocks();
}

TEST_F(dataContainerHelperTests, failUnpackDataToContainer) {
    dataContainer dst;

#if OPAL_ENABLE_DEBUG
    mock.setupUnpackMock(3);
#else
    mock.setupUnpackMock(4);
#endif

    EXPECT_NO_THROW(dataContainerHelper::serialize(*cnt, buffer));
    serializationExceptionTester(OPAL_ERROR, "Unable to unpack data from opal buffer", dataContainerHelper::deserialize);
    mock.teardownMocks();
}

TEST_F(dataContainerHelperTests, failPackContainerBuffer) {

#if OPAL_ENABLE_DEBUG
    mock.setupPackMock(11);
#else
    mock.setupPackMock(14);
#endif

    serializationMapExceptionTester(OPAL_ERROR, "Unable to pack container buffer into buffer", dataContainerHelper::serializeMap);
    mock.teardownMocks();
}

TEST_F(dataContainerHelperTests, serializeMapToNullOpalBuffer) {
    opal_buffer_t* tmpPtr = buffer;
    buffer = NULL;
    serializationMapExceptionTester(OPAL_ERR_BAD_PARAM, "Invalid output buffer", dataContainerHelper::serializeMap);
    buffer = tmpPtr;
}

TEST_F(dataContainerHelperTests, deserializeMapToNullOpalBuffer) {
    opal_buffer_t* tmpPtr = buffer;
    buffer = NULL;
    serializationMapExceptionTester(OPAL_ERR_BAD_PARAM, "Invalid input buffer", dataContainerHelper::deserializeMap);
    buffer = tmpPtr;
}

TEST_F(dataContainerHelperTests, serializeDeserializeMap) {
    dataContainerMap dstMap;
    ASSERT_NO_THROW(dataContainerHelper::serializeMap(cntMap, buffer));
    ASSERT_NO_THROW(dataContainerHelper::deserializeMap(dstMap, buffer));
    ASSERT_EQ(cntMap.size(), dstMap.size());
    for (dataContainerMap::iterator it =  dstMap.begin(); it != dstMap.end(); it++) {
        EXPECT_TRUE(cntMap.end() != cntMap.find(it->first));
    }
}

TEST_F(dataContainerHelperTests, serializeDeserializeEmptyMap) {
    dataContainerMap emptyMap;
    ASSERT_NO_THROW(dataContainerHelper::serializeMap(emptyMap, buffer));
    ASSERT_NO_THROW(dataContainerHelper::deserializeMap(emptyMap, buffer));
    ASSERT_EQ(0, emptyMap.size());
}

TEST_F(dataContainerHelperTests, serializeToNullOpalBuffer) {
    opal_buffer_t* tmpPtr = buffer;
    buffer = NULL;
    serializationExceptionTester(OPAL_ERR_BAD_PARAM, "Invalid output buffer", dataContainerHelper::serialize);
    buffer = tmpPtr;
}

TEST_F(dataContainerHelperTests, deserializeFromNullOpalBuffer) {
    opal_buffer_t* tmpPtr = buffer;
    buffer = NULL;
    serializationExceptionTester(OPAL_ERR_BAD_PARAM, "Invalid input buffer", dataContainerHelper::deserialize);
    buffer = tmpPtr;
}

TEST_F(dataContainerHelperFullDataTypeTests, serializeAndDeserialize) {
    dataContainer dst;

    EXPECT_NO_THROW(dataContainerHelper::serialize(*cnt, buffer));
    EXPECT_NO_THROW(dataContainerHelper::deserialize(dst, buffer));

    for (dataContainer::iterator it = cnt->begin(); it != cnt->end(); ++it) {
        std::string key = cnt->getKey(it);
        EXPECT_TRUE(dst.containsKey(key));

        if (cnt->matchType<bool>(it)) {
            compareItems<bool>(*cnt, it, key, dst);
        } else if (cnt->matchType<int8_t>(it)) {
            compareItems<int8_t>(*cnt, it, key, dst);
        } else if (cnt->matchType<int16_t>(it)) {
            compareItems<int16_t>(*cnt, it, key, dst);
        } else if (cnt->matchType<int32_t>(it)) {
            compareItems<int32_t>(*cnt, it, key, dst);
        } else if (cnt->matchType<int64_t>(it)) {
            compareItems<int64_t>(*cnt, it, key, dst);
        } else if (cnt->matchType<uint8_t>(it)) {
            compareItems<uint8_t>(*cnt, it, key, dst);
        } else if (cnt->matchType<uint16_t>(it)) {
            compareItems<uint16_t>(*cnt, it, key, dst);
        } else if (cnt->matchType<uint32_t>(it)) {
            compareItems<uint32_t>(*cnt, it, key, dst);
        } else if (cnt->matchType<uint64_t>(it)) {
            compareItems<uint64_t>(*cnt, it, key, dst);
        } else if (cnt->matchType<float>(it)) {
            compareItems<float>(*cnt, it, key, dst);
        } else if (cnt->matchType<double>(it)) {
            compareItems<double>(*cnt, it, key, dst);
        } else if (cnt->matchType<struct timeval>(it)) {
            compareItems<struct timeval>(*cnt, it, key, dst);
        } else if (cnt->matchType<std::string>(it)) {
            compareItems<std::string>(*cnt, it, key, dst);
        } else {
            FAIL() << "Wrong data type!" ;
        }
    }
}

TEST_F(dataContainerHelperFullDataTypeTests, invalidDataType) {
    EXPECT_NO_THROW(dataContainerHelper::serialize(*cnt, buffer));

    char dummyLabel[] = "-";
    packKeyUnits(buffer, dummyLabel, dummyLabel);
    packDummyUnsupportedData(buffer);

    serializationExceptionTester(OPAL_ERR_UNKNOWN_DATA_TYPE,
                    "Unsupported data type for dataContainer",
                    dataContainerHelper::deserialize);
}

TEST_F(dataContainerHelperTests, dataContainerToList_invalidList) {
    opal_list_t *list = NULL;
    dataContainerToListExceptionTester(OPAL_ERR_BAD_PARAM, "Invalid input list", list);
    ASSERT_TRUE(NULL == list);
}

TEST_F(dataContainerHelperTests, dataContainerToList_appendOrcmValueFailed) {
    opal_list_t *list = OBJ_NEW(opal_list_t);
    mock.setupAppendOrcmValueMock(1);
    dataContainerToListExceptionTester(OPAL_ERROR,
                                       "There were some errors trying to convert dataContainer to List",
                                       list);
    ORCM_RELEASE(list);
    mock.teardownMocks();
}

TEST_F(dataContainerHelperFullDataTypeTests, dataContainerToList) {
    orcm_value_t *item = NULL;
    opal_list_t *list = OBJ_NEW(opal_list_t);

    EXPECT_NO_THROW(dataContainerHelper::dataContainerToList(*cnt, (void*)list));
    EXPECT_EQ(cnt->count(), opal_list_get_size(list));
    dataContainer::iterator it = cnt->begin();
    OPAL_LIST_FOREACH(item, list, orcm_value_t) {
        verifyItem(item, it);
        ++it;
    }
    ORCM_RELEASE(list);
}
