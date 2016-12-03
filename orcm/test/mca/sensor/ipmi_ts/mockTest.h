/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MOCK_IPMI_TS_FACTORY_TESTS_H
#define MOCK_IPMI_TS_FACTORY_TESTS_H

#include <iostream>
#include <stdexcept>
#include <dirent.h>
#include "orcm/common/dataContainerHelper.hpp"
#include "orcm/mca/sensor/ipmi_ts/ipmiSensorInterface.h"

typedef ipmiSensorInterface* (*sensorInstance)(std::string);

bool mock_plugin;
bool mock_do_collect;
bool mock_opal_pack;
bool mock_opal_unpack;
bool mock_analytics_base_send_data;
bool mock_serializeMap;
bool mock_deserializeMap;
bool do_collect_expected_value;
bool opal_pack_expected_value;
bool opal_secpack_expected_value;
bool opal_unpack_expected_value;
bool throwOnInit;
bool throwOnSample;
bool dlopenReturnHandler;
bool getPluginNameMock;
bool initPluginMock;
bool emptyContainer;
int n_mocked_plugins;
int n_opal_calls;

extern "C" {
    #include "orte/include/orte/types.h"
    #include "orcm/mca/analytics/analytics_types.h"
    extern bool __real_orcm_sensor_base_runtime_metrics_do_collect(void*, const char*);
    extern void __real__ZN19dataContainerHelper12serializeMapERSt3mapISs13dataContainerSt4lessISsESaISt4pairIKSsS1_EEEPv(dataContainerMap &cntMap, void* buffer);
    extern void __real__ZN19dataContainerHelper14deserializeMapERSt3mapISs13dataContainerSt4lessISsESaISt4pairIKSsS1_EEEPv(dataContainerMap &cntMap, void* buffer);
    extern int __real_opal_dss_pack(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type);
    extern int __real_opal_dss_unpack(opal_buffer_t* buffer, void* dst, int32_t* num_vals, opal_data_type_t type);

    bool __wrap_orcm_sensor_base_runtime_metrics_do_collect(void* runtime_metrics, const char* sensor_spec){
        if (mock_do_collect) {
            return do_collect_expected_value;
        }
        return __real_orcm_sensor_base_runtime_metrics_do_collect(runtime_metrics, sensor_spec);
    }

    int __wrap_opal_dss_pack(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type) {
        if(mock_opal_pack) {
            if(n_opal_calls == 0) {
                n_opal_calls++;
                return opal_pack_expected_value;
            } else
                return opal_secpack_expected_value;
        } else {
            __real_opal_dss_pack(buffer, src, num_vals, type);
        }
    }

    int __wrap_opal_dss_unpack(opal_buffer_t* buffer, void* dst, int32_t* num_vals, opal_data_type_t type) {
        if(mock_opal_unpack) {
            return opal_unpack_expected_value;
        } else {
            __real_opal_dss_unpack(buffer, dst, num_vals, type);
        }
    }

    void __wrap_orcm_analytics_base_send_data(orcm_analytics_value_t *data){
      /*Dummy function*/
    }

    /* C++ dataContainerHelperSerializeMap object symbols */
    void __wrap__ZN19dataContainerHelper12serializeMapERSt3mapISs13dataContainerSt4lessISsESaISt4pairIKSsS1_EEEPv(dataContainerMap &cntMap, void* buffer) {
        if(mock_serializeMap){
            throw ErrOpal("Failing on mockSerializeMap", -1);
        } else {
             __real__ZN19dataContainerHelper12serializeMapERSt3mapISs13dataContainerSt4lessISsESaISt4pairIKSsS1_EEEPv(cntMap, buffer);
        }
    }

    /* C++ dataContainerHelperDeserializeMap object symbols */
    void __wrap__ZN19dataContainerHelper14deserializeMapERSt3mapISs13dataContainerSt4lessISsESaISt4pairIKSsS1_EEEPv(dataContainerMap &cntMap, void* buffer) {
        if(mock_deserializeMap){
            throw ErrOpal("Failing on mockDeSerializeMap", -1);
        } else {
             __real__ZN19dataContainerHelper14deserializeMapERSt3mapISs13dataContainerSt4lessISsESaISt4pairIKSsS1_EEEPv(cntMap, buffer);
        }
    }
}

typedef int (*opal_dss_pack_fn_t)(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type);
typedef int (*opal_dss_unpack_fn_t)(opal_buffer_t* buffer, void* dst, int32_t* num_vals, opal_data_type_t type);

#endif
