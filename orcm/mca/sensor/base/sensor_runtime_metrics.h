/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#ifndef BASE_SENSOR_RUNTIME_METRICS_H
#define BASE_SENSOR_RUNTIME_METRICS_H

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
void* orcm_sensor_base_runtime_metrics_create(const char* datagroup, bool global_collect_metrics,
                                              bool plugin_collect_metrics);
void orcm_sensor_base_runtime_metrics_destroy(void* runtime_metrics);
bool orcm_sensor_base_runtime_metrics_do_collect(void* runtime_metrics, const char* sensor_spec);
int orcm_sensor_base_runtime_metrics_reset(void* runtime_metrics, const char* sensor_spec);
int orcm_sensor_base_runtime_metrics_set(void* runtime_metrics, bool newState, const char* sensor_spec);
#ifdef __cplusplus
}; // extern "C"

#include <string>

class RuntimeMetrics
{
    public:
        RuntimeMetrics(const char* datagroup, bool global_collect_metrics, bool plugin_collect_metrics);

        bool DoCollectMetrics(const char* sensor_label = NULL);

        int SetCollectionState(bool newState, const char* sensor_spec);
        int ResetCollectionState(const char* sensor_spec);

    private:
        bool IsForMyDatagroup(const char* sensor_spec);

    private:
        std::string datagroup_;
        bool initalPluginState_;
        bool currentPluginState_;
}; // class RuntimeMetrics

#endif // __cplusplus

#endif // BASE_SENSOR_RUNTIME_METRICS_H
