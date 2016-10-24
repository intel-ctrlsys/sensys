/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef DATA_CONTAINER_TESTS_H
#define DATA_CONTAINER_TESTS_H

#include "gtest/gtest.h"
#include "orcm/common/dataContainer.hpp"

class utDataContainer : public testing::Test {
    protected:
        dataContainer *cnt;

        utDataContainer() {
            cnt = new dataContainer;
        }

        virtual ~utDataContainer() {
            delete cnt;
        }
};

class utDataContainerIteration : public utDataContainer {
    protected:
        static const int numberOfElements = 5;
        dataContainer::iterator it;
        std::string probeValue;
        std::string probeUnits;

        utDataContainerIteration() : utDataContainer() {
            probeValue = std::string("intValue_3");
            probeUnits = std::string("ints");
            cnt->put("intValue_1", 1, "ints");
            cnt->put("intValue_2", 2, "ints");
            cnt->put("intValue_3", 3, "ints");
            cnt->put("intValue_4", 4, "ints");
            cnt->put("intValue_5", 5, "ints");

            it = cnt->begin();
            it++;
            it++;
        }
};

template<typename T> T putAndGet(dataContainer* cnt, const char key[], const char units[], T data);

#endif
