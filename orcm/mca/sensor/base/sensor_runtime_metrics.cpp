#include "sensor_runtime_metrics.h"

// class RuntimeMetrics
RuntimeMetrics::RuntimeMetrics(bool global_collect_metrics, bool plugin_collect_metrics) :
    initalPluginState_(true), currentPluginState_(true)
{
    initalPluginState_ = global_collect_metrics;
    initalPluginState_ = plugin_collect_metrics; // overrides global.

    ResetCollectionState();
}


bool RuntimeMetrics::DoCollectMetrics(const char* sensor_label)
{
    (void)sensor_label; // Not implemented.
    return currentPluginState_;
}

void RuntimeMetrics::SetCollectionState(bool newState, const char* sensor_label)
{
    (void)sensor_label; // Not Implemented.
    currentPluginState_ = newState;
}

void RuntimeMetrics::ResetCollectionState(void)
{
    currentPluginState_ = initalPluginState_;
}


// C Interface
extern "C" {
    void* orcm_sensor_base_runtime_metrics_create(bool global_collect_metrics,
                                                  bool plugin_collect_metrics)
    {
        return static_cast<void*>(new RuntimeMetrics(global_collect_metrics, plugin_collect_metrics));
    }

    void orcm_sensor_base_runtime_metrics_destroy(void* runtime_metrics)
    {
        delete static_cast<RuntimeMetrics*>(runtime_metrics);
    }

    bool orcm_sensor_base_runtime_metrics_do_collect(void* runtime_metrics)
    {
        RuntimeMetrics* collection = static_cast<RuntimeMetrics*>(runtime_metrics);
        return collection->DoCollectMetrics();
    }

    void orcm_sensor_base_runtime_metrics_reset(void* runtime_metrics)
    {
        RuntimeMetrics* collection = static_cast<RuntimeMetrics*>(runtime_metrics);
        collection->ResetCollectionState();
    }

    void orcm_sensor_base_runtime_metrics_set(void* runtime_metrics, bool newState)
    {
        RuntimeMetrics* collection = static_cast<RuntimeMetrics*>(runtime_metrics);
        collection->SetCollectionState(newState);
    }
} // extern "C"
