/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef DATA_CONTAINER_HELPER_TESTS_H
#define DATA_CONTAINER_HELPER_TESTS_H

#include "gtest/gtest.h"

#include "orcm/common/dataContainer.hpp"
#include "opal/dss/dss.h"

class dssMocks {
    private:
        opal_dss_pack_fn_t packPtr;
        opal_dss_unpack_fn_t unpackPtr;
        size_t packCount;
        size_t packLimit;
        size_t unpackLimit;
        size_t unpackCount;

    public:
        dssMocks();
        ~dssMocks();
        void setupPackMock(int n);
        void setupUnpackMock(int n);
        void teardownMocks();

        inline bool mockPack() { return (packLimit > 0 && packLimit == ++packCount); };
        inline bool mockUnpack() { return (unpackLimit > 0 && unpackLimit == ++unpackCount); };

        inline int real_pack(opal_buffer_t *buffer,
                             const void *src,
                             int32_t num_vals,
                             opal_data_type_t type) { return packPtr(buffer, src, num_vals, type); }

        inline int real_unpack(opal_buffer_t *buffer,
                               void *dst,
                               int32_t *num_vals,
                               opal_data_type_t type) { return unpackPtr(buffer, dst, num_vals, type); }
};

typedef void (*serializationMethods)(dataContainer& cnt, void* buffer);

class dataContainerHelperTests : public testing::Test {
    protected:
        dataContainer* cnt;
        opal_buffer_t* buffer;


        void packKeyUnits(opal_buffer_t* buffer, char* key, char* units);
        void unpackKeyUnits(opal_buffer_t* buffer, char** key, char** units);
        void packDummyUnsupportedData(opal_buffer_t* buffer);
        void checkPackedData(const dataContainer& cnt, dataContainer::iterator& it, opal_buffer_t* buffer);
        template<typename T> void unpackAndCheck(const dataContainer& cnt,
                                                 dataContainer::iterator& it,
                                                 opal_buffer_t* buffer,
                                                 opal_data_type_t type);
        template<typename T> void compareItems(const dataContainer& cnt,
                                               dataContainer::iterator& it,
                                               std::string key,
                                               dataContainer& dst);
        void exceptionTester(const int rc, const std::string& message, serializationMethods f);

    public:
        dataContainerHelperTests();
        virtual ~dataContainerHelperTests();
};

class dataContainerHelperFullDataTypeTests : public dataContainerHelperTests {
    public:
        dataContainerHelperFullDataTypeTests();
};

#endif
