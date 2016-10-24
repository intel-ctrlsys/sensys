/*
 * Copyright (c) 2015  Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "vardata.h"

using namespace std;

vardata::vardata(const string& value) {
    type = OPAL_STRING;
    strData = value;
}

vardata::vardata(float value) {
    type = OPAL_FLOAT;
    data.fval = value;
}

vardata::vardata(double value) {
    type = OPAL_DOUBLE;
    data.dval = value;
}

vardata::vardata(int32_t value) {
    type = OPAL_INT32;
    data.int32 = value;
}

vardata::vardata(int64_t value) {
    type = OPAL_INT64;
    data.int64 = value;
}

vardata::vardata(uint32_t value) {
    type = OPAL_UINT32;
    data.int32 = value;
}

vardata::vardata(uint64_t value) {
    type = OPAL_UINT64;
    data.int64 = value;
}

vardata::vardata(const struct timeval& value) {
    type = OPAL_TIMEVAL;
    data.tv = value;
}

template int32_t vardata::getValue<int32_t>();
template float   vardata::getValue<float>();
template double  vardata::getValue<double>();
template int64_t vardata::getValue<int64_t>();
template uint32_t vardata::getValue<uint32_t>();
template uint64_t vardata::getValue<uint64_t>();

template <> string vardata::getValue<string>() {
    return strData;
};

template <> char* vardata::getValue<char*>() {
    return const_cast<char*>(strData.c_str());
};

void vardata::packTo(opal_buffer_t* buffer) {
    int rc;

    string label = getKey();
    if (label.empty()) {
        label = string("-");
    }
    char* labelPtr = strdup(label.c_str());

    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &labelPtr, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        throw unableToPack();
    }

    if (NULL != labelPtr) {
        free(labelPtr);
    }
#if !OPAL_ENABLE_DEBUG
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &type, 1, OPAL_DATA_TYPE_T))) {
        ORTE_ERROR_LOG(rc);
        throw unableToPack();
    }
#endif
    if (OPAL_STRING == type) {
        char* strPtr = strdup(strData.c_str());
        rc = opal_dss.pack(buffer, &strPtr, 1, type);
        if (NULL != strPtr) {
            free(strPtr);
        }
    } else {
        rc = opal_dss.pack(buffer, &data, 1, type);
    }

    if (OPAL_SUCCESS != rc) {
        ORTE_ERROR_LOG(rc);
        throw unableToPack();
    }
}

orcm_value_t* vardata::loadToOrcmValue() {
    orcm_value_t *retValue;

    if (type == OPAL_STRING) {
        retValue = orcm_util_load_orcm_value((char*) key.c_str(), (void*) strData.c_str(), type, NULL);
    } else {
        retValue = orcm_util_load_orcm_value((char*) key.c_str(), &data, type, NULL);
    }

    if (NULL == retValue) {
        throw unableToCreateObject();
    }

    return retValue;
}

void vardata::appendToOpalList(opal_list_t *opalList) {
    orcm_value_t* value = loadToOrcmValue();
    opal_list_append(opalList, (opal_list_item_t *)value);
}

string unpackKey(opal_buffer_t *buffer) {
    char* label = NULL;
    int32_t number = 1;
    int rc;

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &label, &number, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        throw invalidBuffer();
    }
    string key = string(label);
    SAFEFREE(label);
    return key;
}

opal_data_type_t getUnpackType(opal_buffer_t *buffer) {
    int32_t number = 1;
    int rc;
    opal_data_type_t localType;
#if OPAL_ENABLE_DEBUG
    if ( OPAL_SUCCESS != (rc = opal_dss.peek(buffer, &localType, &number)) ) {
        ORTE_ERROR_LOG(rc);
        throw invalidBuffer();
    }
#else
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &localType, &number, OPAL_DATA_TYPE_T))) {
        ORTE_ERROR_LOG(rc);
        throw invalidBuffer();
    }
#endif
    return localType;
}

vardata getUnpackedData(opal_buffer_t *buffer, opal_data_type_t localType, string key) {
    int32_t number = 1;
    int rc;

    if (OPAL_STRING == localType) {
        char* s = NULL;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &s, &number, localType))) {
            ORTE_ERROR_LOG(rc);
            throw invalidBuffer();
        }
        string ss(s);
        SAFEFREE(s);
        return vardata(ss).setKey(key);
    } else {
        union supportedDataTypes localData;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &localData, &number, localType))) {
            ORTE_ERROR_LOG(rc);
            throw invalidBuffer();
        }

        switch(localType) {
            case OPAL_FLOAT:
                return vardata(localData.fval).setKey(key);
                break;
            case OPAL_DOUBLE:
                return vardata(localData.dval).setKey(key);
                break;
            case OPAL_INT32:
                return vardata(localData.int32).setKey(key);
                break;
            case OPAL_INT64:
                return vardata(localData.int64).setKey(key);
                break;
            case OPAL_UINT32:
                return vardata(localData.uint32).setKey(key);
                break;
            case OPAL_UINT64:
                return vardata(localData.uint64).setKey(key);
                break;
            case OPAL_TIMEVAL:
                return vardata(localData.tv).setKey(key);
                break;
            default:
                throw unsupportedDataType();
        }
    }
}

vardata fromOpalBuffer(opal_buffer_t *buffer) {
    string key = unpackKey(buffer);
    opal_data_type_t localType = getUnpackType(buffer);
    return getUnpackedData(buffer, localType, key);
}

vardata fromOpalBuffer(opal_buffer_t *buffer, opal_data_type_t localType) {
    string key = unpackKey(buffer);
    return getUnpackedData(buffer, localType, key);
}

void packDataToBuffer(vector<vardata> inputData, opal_buffer_t *buffer) {
    for(vector<vardata>::iterator it = inputData.begin(); it != inputData.end(); ++it) {
        it->packTo(buffer);
    }
}

vector<vardata> unpackDataFromBuffer(opal_buffer_t *buffer) {
    if (NULL == buffer) {
        throw invalidBuffer();
    }

    vector<vardata> retValue;
    while( buffer->unpack_ptr < buffer->base_ptr + buffer->bytes_used ) {
        retValue.push_back(fromOpalBuffer(buffer));
    }

    return retValue;
}

void packDataToOpalList(vector<vardata> inputData, opal_list_t* opalList) {
    for(vector<vardata>::iterator it = inputData.begin(); it != inputData.end(); ++it) {
        opal_list_append(opalList, (opal_list_item_t*) it->loadToOrcmValue());
    }
}
