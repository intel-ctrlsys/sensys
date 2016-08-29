/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef DATA_CONTAINER_H
#define DATA_CONTAINER_H

#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <stdint.h>
#include <sys/time.h>
#include <typeinfo>

using namespace std;
// Exceptions
class unableToFindKey : public std::runtime_error {
    public:
        unableToFindKey(const std::string &k) : std::runtime_error(
            std::string("Unable to find key in container (") + k + std::string(")")) {}
};

typedef uint8_t dataByte;

class dataHolder {
    public:
        dataHolder();
        template<typename T> dataHolder(const T& value);

        template<typename T> T getValue();
        template<typename T> bool is();

        template<typename T> inline T* ptr() {
            return reinterpret_cast<T*>(storedData.data());
        };

        inline size_t size() {
            return storedData.size()*sizeof(dataByte);
        };

        inline const std::string& getDataTypeName() const {
            return dataTypeName;
        };

        inline const std::string& getUnits() const {
            return units;
        };

        inline void setUnits(const std::string& u) {
            units = u;
        };

    protected:
        std::string units;
        std::string dataTypeName;
        std::vector<dataByte> storedData;
};

class dataContainer {
    public:
        typedef std::map<std::string, dataHolder>::iterator iterator;

        dataContainer();

        template<typename T> void put(const std::string& k, const T &v, const std::string &u);
        template<typename T> T getValue(const std::string& key);
        template<typename T> T getValue(const dataContainer::iterator& it) const;
        void erase(const dataContainer::iterator& it);
        void erase(const std::string& key);

        inline bool containsKey(const std::string& key) {
            return !(container.find(key) == container.end());
        };

        inline const std::string& getUnits(const std::string& key) {
            checkForKey(key);
            return container[key].getUnits();
        };

        inline const std::string& getDataTypeName(const std::string& key) {
            checkForKey(key);
            return container[key].getDataTypeName();
        };

        inline size_t getDataSize(const std::string& key) {
            checkForKey(key);
            return container[key].size();
        };

        template<typename T> inline bool matchType(const std::string& key) {
            checkForKey(key);
            return container[key].is<T>();
        };

        inline size_t count() {
            return container.size();
        };

        inline iterator begin() {
            return container.begin();
        };

        inline iterator end() {
            return container.end();
        };

        inline const std::string& getKey(const iterator& it) const {
            return it->first;
        };

        inline const std::string& getUnits(const iterator& it) const {
            return it->second.getUnits();
        };

        inline const std::string& getDataTypeName(const iterator& it) const {
            return it->second.getDataTypeName();
        };

        inline void* getRawDataPtr(const iterator& it) const {
            return it->second.ptr<void>();
        };

        template<typename T> inline bool matchType(const iterator& it) const {
            return it->second.is<T>();
        };
    protected:
        inline void checkForKey(const std::string& key) {
            if (!containsKey(key)) {
                throw unableToFindKey(key);
            }
        };

        std::map<std::string, dataHolder> container;
};

typedef std::map<std::string, dataContainer> dataContainerMap;

// dataContainer Templates

template <typename T> inline void dataContainer::put(const string& k, const T& v, const string& u) {
    dataHolder data(v);
    data.setUnits(u);
    container.insert(pair< string, dataHolder >(k, data));
}

template<typename T> inline T dataContainer::getValue(const string& key) {
    checkForKey(key);
    return container[key].getValue<T>();
}

template<typename T> inline T dataContainer::getValue(const dataContainer::iterator& it) const {
    return it->second.getValue<T>();
}

template <> inline dataHolder::dataHolder(const string& value) {
    size_t s = value.length() + 1; // +1 because of NULL terminator
    dataByte* ptr = (dataByte*) value.c_str();
    storedData.insert(storedData.begin(), ptr, ptr + s);
    dataTypeName = string(typeid(string).name());
}

template<typename T> inline dataHolder::dataHolder(const T& value) {
    size_t s = sizeof(T);
    dataByte* ptr = (dataByte*) &value;
    storedData.insert(storedData.begin(), ptr, ptr + s);
    dataTypeName = string(typeid(T).name());
}

template <> inline string dataHolder::getValue() {
    char* s = (char*) storedData.data();
    return string(s);
}

template<typename T> inline T dataHolder::getValue() {
    return *((T*) storedData.data());
}

template<typename T> inline bool dataHolder::is() {
    return (0 == dataTypeName.compare(typeid(T).name()));
}

#endif
