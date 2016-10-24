/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
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

#ifdef  __cplusplus
extern "C" {
#endif
    #include "orcm/runtime/orcm_globals.h"
    #include "opal/dss/dss.h"

    extern int __real_orcm_util_append_orcm_value(opal_list_t *list, char *key, void *data,
                                                  opal_data_type_t type, char *units);
#ifdef  __cplusplus
}
#endif

class dssMocks {
    private:
        opal_dss_pack_fn_t packPtr;
        opal_dss_unpack_fn_t unpackPtr;
        size_t packCount;
        size_t packLimit;
        size_t unpackLimit;
        size_t unpackCount;
        size_t appendOrcmValueLimit;
        size_t appendOrcmValueCount;

    public:
        dssMocks();
        ~dssMocks();
        void setupPackMock(int n);
        void setupUnpackMock(int n);
        void setupAppendOrcmValueMock(int n);
        void teardownMocks();

        inline bool mockPack() { return (packLimit > 0 && packLimit == ++packCount); };
        inline bool mockUnpack() { return (unpackLimit > 0 && unpackLimit == ++unpackCount); };
        inline bool mockAppendOrcmValue() { return (appendOrcmValueLimit > 0 &&
                                                    appendOrcmValueLimit == ++appendOrcmValueCount); }

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
typedef void (*serializationMapMethods)(dataContainerMap &cnt, void* buffer);

class dataContainerHelperTests : public testing::Test {
    protected:
        dataContainer* cnt;
        dataContainerMap cntMap;
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
        void serializationExceptionTester(const int rc, const std::string& message, serializationMethods f);
        void serializationMapExceptionTester(const int rc, const std::string& message, serializationMapMethods f);
        void dataContainerToListExceptionTester(const int rc, const std::string& message, opal_list_t *list);
        void verifyItem(orcm_value_t *item, dataContainer::iterator& it);

    public:
        dataContainerHelperTests();
        virtual ~dataContainerHelperTests();
};

class dataContainerHelperFullDataTypeTests : public dataContainerHelperTests {
    public:
        dataContainerHelperFullDataTypeTests();
};

#endif
