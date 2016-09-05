/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/common/dataContainerHelper.hpp"

extern "C" {
    #include "orcm/util/utils.h"
}

#include "orte/mca/errmgr/errmgr.h"

#include <typeinfo>

using std::string;

static std::map<string, int> dataTypeMap = initializeDataTypesEquivalence();

void dataContainerHelper::appendContainerToOpalBuffer(dataContainer& cnt,
                                                      opal_buffer_t* buffer) {

    for (dataContainer::iterator it = cnt.begin(); it != cnt.end(); it++) {
        packStringLabel(cnt.getKey(it), buffer);
        packStringLabel(cnt.getUnits(it), buffer);
        packDataFromContainer(cnt, it, buffer);
    }
}

void dataContainerHelper::packStringLabel(const string& label, opal_buffer_t* buffer) {
    int rc = OPAL_SUCCESS;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &label, 1, OPAL_STRING))) {
        throw ErrOpal("Unable to pack string into opal buffer", rc);
    }
}

string dataContainerHelper::unpackStringLabel(opal_buffer_t* buffer) {
    int rc = OPAL_SUCCESS;
    int number = 1;
    char* label = NULL;

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &label, &number, OPAL_STRING))) {
        throw ErrOpal("Unable to unpack string from opal buffer", rc);
    }

    string retValue(label);
    SAFEFREE(label);

    return retValue;
}

template<typename T> T dataContainerHelper::extractFromBuffer(opal_buffer_t* buffer,
                                                              const opal_data_type_t& type) {
    int rc = OPAL_SUCCESS;
    int number = 1;
    T data;

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &data, &number, type))) {
        throw ErrOpal("Unable to unpack data from opal buffer", rc);
    }

    return data;
}

template <> string dataContainerHelper::extractFromBuffer(opal_buffer_t* buffer,
                                                          const opal_data_type_t& type) {
    return unpackStringLabel(buffer);
}

opal_data_type_t dataContainerHelper::getOpalType(opal_buffer_t* buffer) {
    int32_t number = 1;
    int rc;
    opal_data_type_t localType;
#if OPAL_ENABLE_DEBUG
    if (OPAL_SUCCESS != (rc = opal_dss.peek(buffer, &localType, &number))) {
        throw ErrOpal("Unable to peek data type", rc);
    }
#else
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &localType, &number, OPAL_DATA_TYPE_T))) {
        throw ErrOpal("Unable to unpack data type", rc);
    }
#endif
    return localType;

}

void dataContainerHelper::pushBufferItemToContainer(dataContainer& cnt, opal_buffer_t* buffer) {
    string key = unpackStringLabel(buffer);
    string units = unpackStringLabel(buffer);

    opal_data_type_t type = getOpalType(buffer);
    switch(type) {
        case OPAL_BOOL:
            cnt.put(key, extractFromBuffer<bool>(buffer, type), units);
            break;
        case OPAL_INT8:
            cnt.put(key, extractFromBuffer<int8_t>(buffer, type), units);
            break;
        case OPAL_INT16:
            cnt.put(key, extractFromBuffer<int16_t>(buffer, type), units);
            break;
        case OPAL_INT32:
            cnt.put(key, extractFromBuffer<int32_t>(buffer, type), units);
            break;
        case OPAL_INT64:
            cnt.put(key, extractFromBuffer<int64_t>(buffer, type), units);
            break;
        case OPAL_UINT8:
            cnt.put(key, extractFromBuffer<uint8_t>(buffer, type), units);
            break;
        case OPAL_UINT16:
            cnt.put(key, extractFromBuffer<uint16_t>(buffer, type), units);
            break;
        case OPAL_UINT32:
            cnt.put(key, extractFromBuffer<uint32_t>(buffer, type), units);
            break;
        case OPAL_UINT64:
            cnt.put(key, extractFromBuffer<uint64_t>(buffer, type), units);
            break;
        case OPAL_FLOAT:
            cnt.put(key, extractFromBuffer<float>(buffer, type), units);
            break;
        case OPAL_DOUBLE:
            cnt.put(key, extractFromBuffer<double>(buffer, type), units);
            break;
        case OPAL_TIMEVAL:
            cnt.put(key, extractFromBuffer<struct timeval>(buffer, type), units);
            break;
        case OPAL_STRING:
            cnt.put(key, extractFromBuffer<string>(buffer, type), units);
            break;
        default:
            throw ErrOpal("Unsupported data type for dataContainer", OPAL_ERR_UNKNOWN_DATA_TYPE);
    }
}

bool dataContainerHelper::isBufferEmpty(opal_buffer_t* buffer) {
    if (buffer->unpack_ptr < buffer->base_ptr + buffer->bytes_used)
        return false;
    return true;
}

void dataContainerHelper::pushBufferToContainer(dataContainer& cnt, opal_buffer_t* buffer) {
    while( !isBufferEmpty(buffer) ) {
        pushBufferItemToContainer(cnt, buffer);
    }
}

void dataContainerHelper::packDataFromContainer(const dataContainer& cnt,
                                                const dataContainer::iterator it,
                                                opal_buffer_t* buffer) {
    int rc = OPAL_SUCCESS;
    int dataType = dataTypeMap[cnt.getDataTypeName(it)];

#if !OPAL_ENABLE_DEBUG
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &dataType, 1, OPAL_DATA_TYPE_T))) {
        throw ErrOpal("Unable to pack data type into opal buffer", rc);
    }
#endif

    if (cnt.matchType<std::string>(it)) {
        const string label = cnt.getValue<std::string>(it);
        packStringLabel(label, buffer);
        return;
    }

    void* dataPtr = cnt.getRawDataPtr(it);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, dataPtr, 1, dataType))) {
        throw ErrOpal("Unable to pack data into opal buffer", rc);
    }
}

void dataContainerHelper::whenNullThrowBadParam(const std::string& msg,
                                                                const void* ptr) {
    if (NULL == ptr) {
        throw ErrOpal(msg, OPAL_ERR_BAD_PARAM);
    }
}

void dataContainerHelper::pushBufferToContainerMap(dataContainerMap &cntMap, opal_buffer_t* buffer) {
    string label;
    opal_buffer_t* cntBuffer = NULL;
    dataContainer* cnt = NULL;

    while (!isBufferEmpty(buffer)){
        label  = unpackStringLabel(buffer);
        cntBuffer = extractFromBuffer<opal_buffer_t*>(buffer, OPAL_BUFFER);
        cnt = new dataContainer();
        deserialize(*cnt, cntBuffer);
        cntMap[label] =  *cnt;
        delete cnt;
        ORCM_RELEASE(cntBuffer);
    }
}

void dataContainerHelper::appendContainerMapToOpalBuffer(dataContainerMap &cntMap, opal_buffer_t* buffer) {
    opal_buffer_t* cntBuffer;
    for (dataContainerMap::iterator it = cntMap.begin() ; it != cntMap.end() ; it++) {
       packStringLabel(it->first, buffer);
       cntBuffer = OBJ_NEW(opal_buffer_t);
       serialize(it->second, cntBuffer);
       packContainerBufferToOpalBuffer(cntBuffer, buffer);
       ORCM_RELEASE(cntBuffer);
    }
}

void dataContainerHelper::packContainerBufferToOpalBuffer(opal_buffer_t* cntBuffer, opal_buffer_t* buffer) {
    int rc = OPAL_SUCCESS;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &cntBuffer, 1, OPAL_BUFFER))) {
        throw ErrOpal("Unable to pack container buffer into buffer", rc);
    }
}

void dataContainerHelper::serialize(dataContainer &cnt, void* buffer) {
    whenNullThrowBadParam("Invalid output buffer", buffer);
    appendContainerToOpalBuffer(cnt, (opal_buffer_t*) buffer);
}

void dataContainerHelper::serializeMap(dataContainerMap &cntMap, void* buffer) {
    whenNullThrowBadParam("Invalid output buffer", buffer);
    appendContainerMapToOpalBuffer(cntMap, (opal_buffer_t*) buffer);
}


void dataContainerHelper::deserialize(dataContainer& cnt, void* buffer) {
    whenNullThrowBadParam("Invalid input buffer", buffer);
    pushBufferToContainer(cnt, (opal_buffer_t*) buffer);
}

void dataContainerHelper::deserializeMap(dataContainerMap &cntMap, void* buffer) {
    whenNullThrowBadParam("Invalid input buffer", buffer);
    pushBufferToContainerMap(cntMap, (opal_buffer_t*) buffer);
}

void dataContainerHelper::pushContainerToList(dataContainer& cnt, void* list) {
    char* key = NULL;
    void* data = NULL;
    opal_data_type_t type;
    char* units = NULL;
    int rc = OPAL_SUCCESS;

    for (dataContainer::iterator it = cnt.begin() ; it != cnt.end() ; ++it) {
        key = strdup(cnt.getKey(it).c_str());
        data = cnt.getRawDataPtr(it);
        type = dataTypeMap[cnt.getDataTypeName(it)];
        units = strdup(cnt.getUnits(it).c_str());
        rc |= orcm_util_append_orcm_value((opal_list_t*) list, key, data, type, units);
        SAFEFREE(key);
        SAFEFREE(units);
    }

    if (OPAL_SUCCESS != rc) {
        throw ErrOpal("There were some errors trying to convert dataContainer to List", rc);
    }
}

void dataContainerHelper::dataContainerToList(dataContainer& cnt, void* list) {
    whenNullThrowBadParam("Invalid input list", list);
    pushContainerToList(cnt, list);
}

#define ADD_EQUIVALENCE_FOR(TYPE, OPAL_TYPE) \
    retValue[typeid(TYPE).name()] = OPAL_TYPE;

static const std::map<string, int> initializeDataTypesEquivalence() {
    std::map<string, int> retValue;

    ADD_EQUIVALENCE_FOR(bool, OPAL_BOOL)
    ADD_EQUIVALENCE_FOR(int8_t, OPAL_INT8)
    ADD_EQUIVALENCE_FOR(int16_t, OPAL_INT16)
    ADD_EQUIVALENCE_FOR(int32_t, OPAL_INT32)
    ADD_EQUIVALENCE_FOR(int64_t, OPAL_INT64)
    ADD_EQUIVALENCE_FOR(uint8_t, OPAL_UINT8)
    ADD_EQUIVALENCE_FOR(uint16_t, OPAL_UINT16)
    ADD_EQUIVALENCE_FOR(uint32_t, OPAL_UINT32)
    ADD_EQUIVALENCE_FOR(uint64_t, OPAL_UINT64)
    ADD_EQUIVALENCE_FOR(float, OPAL_FLOAT)
    ADD_EQUIVALENCE_FOR(double, OPAL_DOUBLE)
    ADD_EQUIVALENCE_FOR(struct timeval, OPAL_TIMEVAL)
    ADD_EQUIVALENCE_FOR(string, OPAL_STRING)

    return retValue;
}
