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

        inline std::string getDataTypeName() {
            return dataTypeName;
        };

        inline std::string getUnits() {
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
        template<typename T> T getValue(const dataContainer::iterator& it);
        void erase(const dataContainer::iterator& it);
        void erase(const std::string& key);

        inline std::string getUnits(const std::string& key) {
            return container[key].getUnits();
        };

        inline std::string getDataTypeName(const std::string& key) {
            return container[key].getDataTypeName();
        };

        inline size_t getDataSize(const std::string& key) {
            return container[key].size();
        };

        template<typename T> inline bool matchType(const std::string& key) {
            return container[key].is<T>();
        };

        inline bool containsKey(const std::string& key) {
            return !(container.find(key) == container.end());
        };

        inline size_t count() {
            return container.size();
        };

        inline iterator begin() {
            return container.begin();
        }

        inline iterator end() {
            return container.end();
        }

        inline std::string getKey(iterator it) {
            return it->first;
        }

    protected:
        inline void checkForKey(const std::string& key);
        std::map<std::string, dataHolder> container;
};

// Exceptions
class unableToFindKey : public std::runtime_error {
    public:
        unableToFindKey(const std::string &k) : std::runtime_error(
            std::string("Unable to find key in container (") + k + std::string(")")) {}
};


#endif
