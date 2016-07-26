/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "dataContainer.h"
#include <typeinfo>

#define SUPPORT_FOR(TYPE) \
    template void dataContainer::put<TYPE>(const string& k, const TYPE& v, const string& u); \
    template TYPE dataContainer::getValue<TYPE>(const string& key); \
    template TYPE dataContainer::getValue<TYPE>(const dataContainer::iterator& it); \
    template bool dataContainer::matchType<TYPE>(const string& key);

using namespace std;

dataContainer::dataContainer() {}

// List of supported data types
SUPPORT_FOR(bool)
SUPPORT_FOR(int8_t)
SUPPORT_FOR(int16_t)
SUPPORT_FOR(int32_t)
SUPPORT_FOR(int64_t)
SUPPORT_FOR(uint8_t)
SUPPORT_FOR(uint16_t)
SUPPORT_FOR(uint32_t)
SUPPORT_FOR(uint64_t)
SUPPORT_FOR(float)
SUPPORT_FOR(double)
SUPPORT_FOR(struct timeval)
SUPPORT_FOR(string)

template <typename T> void dataContainer::put(const string& k, const T& v, const string& u) {
    dataHolder data(v);
    data.setUnits(u);
    container.insert(pair< string, dataHolder >(k, data));
}

template<typename T> T dataContainer::getValue(const string& key) {
    checkForKey(key);
    return container[key].getValue<T>();
}

template<typename T> T dataContainer::getValue(const dataContainer::iterator& it) {
    return it->second.getValue<T>();
}

void dataContainer::erase(const string& key) {
    checkForKey(key);
    container.erase(container.find(key));
}

void dataContainer::erase(const dataContainer::iterator& it) {
    container.erase(it);
}

inline void dataContainer::checkForKey(const string& key) {
    if (!containsKey(key)) {
        throw unableToFindKey(key);
    }
}

// dataHolder Methods
dataHolder::dataHolder() {}

template <> dataHolder::dataHolder(const string& value) {
    size_t s = value.length() + 1; // +1 because of NULL terminator
    dataByte* ptr = (dataByte*) value.c_str();
    storedData.insert(storedData.begin(), ptr, ptr+ s);
    dataTypeName = string(typeid(string).name());
}

template<typename T> dataHolder::dataHolder(const T& value) {
    size_t s = sizeof(T);
    dataByte* ptr = (dataByte*) &value;
    storedData.insert(storedData.begin(), ptr, ptr + s);
    dataTypeName = string(typeid(T).name());
}

template <> string dataHolder::getValue() {
    char* s = (char*) storedData.data();
    return string(s);
}

template<typename T> T dataHolder::getValue() {
    return *((T*) storedData.data());
}

template<typename T> bool dataHolder::is() {
    return (0 == dataTypeName.compare(typeid(T).name()));
}
