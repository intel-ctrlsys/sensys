#ifndef BASE_SENSOR_RUNTIME_METRICS_H
#define BASE_SENSOR_RUNTIME_METRICS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
void* orcm_sensor_base_runtime_metrics_create(bool global_collect_metrics,
                                                            bool plugin_collect_metrics);
void orcm_sensor_base_runtime_metrics_destroy(void* runtime_metrics);
bool orcm_sensor_base_runtime_metrics_do_collect(void* runtime_metrics);
void orcm_sensor_base_runtime_metrics_reset(void* runtime_metrics);
void orcm_sensor_base_runtime_metrics_set(void* runtime_metrics, bool newState);
#ifdef __cplusplus
}; // extern "C"

class RuntimeMetrics
{
    public:
        RuntimeMetrics(bool global_collect_metrics, bool plugin_collect_metrics);

        bool DoCollectMetrics(const char* sensor_label = NULL);

        void SetCollectionState(bool newState, const char* sensor_label = NULL);
        void ResetCollectionState(void);

    private:
        bool initalPluginState_;
        bool currentPluginState_;
}; // class RuntimeMetrics

#endif // __cplusplus

#endif // BASE_SENSOR_RUNTIME_METRICS_H
