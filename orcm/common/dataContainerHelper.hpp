/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef DATA_CONTAINER_HELPER_H
#define DATA_CONTAINER_HELPER_H

#include <stdexcept>

#include "orcm/common/dataContainer.hpp"
#include "opal/dss/dss_types.h"


static const std::map<std::string, int> initializeDataTypesEquivalence();

class dataContainerHelper {
    private:

        static void packStringLabel(const std::string& label, opal_buffer_t* buffer);
        static void packDataFromContainer(const dataContainer& cnt,
                                          const dataContainer::iterator it,
                                          opal_buffer_t* buffer);
        static void packContainerMapToOpalBuffer(dataContainerMap &cntMap, opal_buffer_t* buffer);
        static void packContainerBufferToOpalBuffer(opal_buffer_t* cntBuffer, opal_buffer_t* buffer);
        static void unpackContainerMapFromOpalBuffer(dataContainerMap &cntMap, opal_buffer_t* buffer);
        static std::string unpackStringLabel(opal_buffer_t* buffer);
        static void pushBufferItemToContainer(dataContainer& cnt, opal_buffer_t* buffer);
        static void pushBufferToContainerMap(dataContainerMap &cntMap, opal_buffer_t* buffer);

        static opal_data_type_t getOpalType(opal_buffer_t* buffer);
        template<typename T> static T extractFromBuffer(opal_buffer_t* buffer, const opal_data_type_t& type);
        static void appendContainerToOpalBuffer(dataContainer& cnt, opal_buffer_t* buffer);
        static void appendContainerMapToOpalBuffer(dataContainerMap& cnt, opal_buffer_t* buffer);
        static void pushBufferToContainer(dataContainer& cnt, opal_buffer_t* buffer);
        static void whenInvalidBufferThrowBadParam(const std::string& msg, const void* buffer);
        static bool isBufferEmpty(opal_buffer_t* buffer);
    public:
        static void serialize(dataContainer& cnt, void* buffer);
        static void deserialize(dataContainer& cnt, void* buffer);
        static void serializeMap(dataContainerMap& cntMap, void* buffer);
        static void deserializeMap(dataContainerMap& cntMap, void* buffer);
};

class ErrOpal : public std::runtime_error {
    private:
        int return_code;
    public:
        ErrOpal(std::string msg, const int& rc) :
	        std::runtime_error(msg), return_code(rc) {};
        int getRC() const { return return_code; };
};

#endif
