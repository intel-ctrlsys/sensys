/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <typeinfo>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <stdint.h>

extern "C" {
    #include "opal/dss/dss.h"
    #include "opal/dss/dss_internal.h"
    #include "orte/mca/errmgr/errmgr.h"
    #include "orcm/runtime/orcm_globals.h"
    #include "orcm/util/utils.h"
}

#ifndef ORCM_VARDATA_H
#define ORCM_VARDATA_H

union supportedDataTypes {
    bool flag;
    uint8_t byte;
    size_t size;
    pid_t pid;
    int integer;
    int8_t int8;
    int16_t int16;
    int32_t int32;
    int64_t int64;
    unsigned int uint;
    uint8_t uint8;
    uint16_t uint16;
    uint32_t uint32;
    uint64_t uint64;
    float fval;
    double dval;
    struct timeval tv;
};

class vardata {
    public:
    vardata(const std::string& value);
    vardata(float value);
    vardata(double value);
    vardata(int32_t value);
    vardata(int64_t value);
    vardata(const struct timeval& tv);

    vardata& setKey(const std::string& k) { key = k; return *this; };
    const std::string& getKey()           { return key;            };
    uint8_t getDataType()                 { return type;           };
    void* getDataPtr()                    { return (void*) &data;  };


    template <typename T> T getValue() { return *((T*) &data); };

    void packTo(opal_buffer_t* buffer);
    orcm_value_t* loadToOrcmValue();
    void appendToOpalList(opal_list_t *opalList);

    private:
    opal_data_type_t type;

    union supportedDataTypes data;

    std::string key;
    std::string strData;
};

class invalidBuffer : public std::runtime_error {
    public:
    invalidBuffer() : std::runtime_error( "Unable to create object from buffer" ) {}
};

class unsupportedDataType : public std::runtime_error {
    public:
    unsupportedDataType() : std::runtime_error( "Data type not supported by the class" ) {}
};

class unableToCreateObject : public std::runtime_error {
    public:
    unableToCreateObject() : std::runtime_error( "Unable to create object (out of memory?)" ) {}
};

class unableToPack : public std::runtime_error {
    public:
    unableToPack() : std::runtime_error( "Unable to pack into buffer" ) {}
};

vardata fromOpalBuffer(opal_buffer_t* buffer, opal_data_type_t localType);
vardata fromOpalBuffer(opal_buffer_t* buffer);
std::vector<vardata> unpackDataFromBuffer(opal_buffer_t *buffer);
void packDataToBuffer(std::vector<vardata> inputData, opal_buffer_t *buffer);
void packDataToOpalList(std::vector<vardata> inputData, opal_list_t* opalList);

#endif
