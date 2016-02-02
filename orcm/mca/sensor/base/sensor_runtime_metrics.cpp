/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */
#include "sensor_runtime_metrics.h"

using namespace std;

// class RuntimeMetrics
RuntimeMetrics::RuntimeMetrics(const char* datagroup, bool global_collect_metrics, bool plugin_collect_metrics) :
    datagroup_(datagroup), initalPluginState_(true), currentPluginState_(true)
{
    initalPluginState_ = global_collect_metrics;
    initalPluginState_ = plugin_collect_metrics; // overrides global.

    ResetCollectionState(datagroup);
}


bool RuntimeMetrics::DoCollectMetrics(const char* sensor_label)
{
    (void)sensor_label; // Not implemented; reserved for future implementation
    return currentPluginState_;
}

int RuntimeMetrics::SetCollectionState(bool newState, const char* sensor_spec)
{
    if(IsForMyDatagroup(sensor_spec)) {
        currentPluginState_ = newState;
    }
    return 0;
}

int RuntimeMetrics::ResetCollectionState(const char* sensor_spec)
{
    if(IsForMyDatagroup(sensor_spec)) {
        currentPluginState_ = initalPluginState_;
    }
    return 0;
}

bool RuntimeMetrics::IsForMyDatagroup(const char* sensor_spec)
{
    string spec = sensor_spec;
    size_t pos = spec.find(':');
    string datagroup = spec.substr(0, pos);
    string label;
    if(pos != string::npos) {
        label = spec.substr(pos + 1);
    }
    (void)label; // Not currently used'; reserved for future implementation
    if("all" == spec ||
       (datagroup.size() == datagroup_.size() && datagroup == datagroup_)) {
        return true;
    }
    return false;
}

// C Interface
extern "C" {
    void* orcm_sensor_base_runtime_metrics_create(const char* datagroup, bool global_collect_metrics,
                                                  bool plugin_collect_metrics)
    {
        return static_cast<void*>(new RuntimeMetrics(datagroup, global_collect_metrics,
                                                     plugin_collect_metrics));
    }

    void orcm_sensor_base_runtime_metrics_destroy(void* runtime_metrics)
    {
        delete static_cast<RuntimeMetrics*>(runtime_metrics);
    }

    bool orcm_sensor_base_runtime_metrics_do_collect(void* runtime_metrics, const char* sensor_spec)
    {
        RuntimeMetrics* collection = static_cast<RuntimeMetrics*>(runtime_metrics);
        return collection->DoCollectMetrics(sensor_spec);
    }

    int orcm_sensor_base_runtime_metrics_reset(void* runtime_metrics, const char* sensor_spec)
    {
        RuntimeMetrics* collection = static_cast<RuntimeMetrics*>(runtime_metrics);
        return collection->ResetCollectionState(sensor_spec);
    }

    int orcm_sensor_base_runtime_metrics_set(void* runtime_metrics, bool newState, const char* sensor_spec)
    {
        RuntimeMetrics* collection = static_cast<RuntimeMetrics*>(runtime_metrics);
        return collection->SetCollectionState(newState, sensor_spec);
    }
} // extern "C"
