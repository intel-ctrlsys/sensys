/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "dataContainer.hpp"
#include <typeinfo>

#define SUPPORT_FOR(TYPE) \
    template void dataContainer::put<TYPE>(const string& k, const TYPE& v, const string& u); \
    template TYPE dataContainer::getValue<TYPE>(const string& key); \
    template TYPE dataContainer::getValue<TYPE>(const dataContainer::iterator& it) const; \
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

void dataContainer::erase(const string& key) {
    checkForKey(key);
    container.erase(container.find(key));
}

void dataContainer::erase(const dataContainer::iterator& it) {
    container.erase(it);
}

dataHolder::dataHolder() {}
