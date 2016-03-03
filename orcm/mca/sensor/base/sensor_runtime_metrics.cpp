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

#include "orcm/include/orcm/constants.h"
#include "sensor_runtime_metrics.h"
#include <time.h>

using namespace std;

// class RuntimeMetrics
RuntimeMetrics::RuntimeMetrics(const char* datagroup, bool global_collect_metrics, bool plugin_collect_metrics) :
    datagroup_(datagroup), initalPluginState_(true), currentPluginState_(true), collectionLock_(false)
{
    initalPluginState_ = global_collect_metrics;
    initalPluginState_ = plugin_collect_metrics; // overrides global.

    ResetCollectionState(datagroup);
}

#include <iostream>
#include <iomanip>
bool RuntimeMetrics::DoCollectMetrics(const char* sensor_label)
{
    if(NULL == sensor_label || 0 == sensorLabelMap_.size() || sensorLabelMap_.end() == sensorLabelMap_.find(sensor_label)) {
        return currentPluginState_;
    } else {
        return sensorLabelMap_[sensor_label];
    }
}

int RuntimeMetrics::SetCollectionState(bool newState, const char* sensor_spec)
{
    int rv = ORCM_SUCCESS;
    if(IsForMyDatagroup(sensor_spec)) {
        if(false == WaitForDataCollection(5)) { // wait for at most 5 seconds (OOB sensors are slow)...
            return ORCM_ERR_TIMEOUT;
        }
        String label = GetSensorLabelFromSpec(sensor_spec);
        if(label.empty() || label == "all" || 0 == sensorLabelMap_.size()) {
            currentPluginState_ = newState;
            for(LabelStateMapItr it = sensorLabelMap_.begin(); sensorLabelMap_.end() != it; ++it) {
                sensorLabelMap_[it->first] = newState;
            }
        } else {
            LabelStateMapItr item = sensorLabelMap_.find(label);
            if(sensorLabelMap_.end() != item) {
                sensorLabelMap_[label] = newState;
            } else {
                rv = ORCM_ERR_NOT_FOUND;
            }
        }
    }
    return rv;
}

int RuntimeMetrics::ResetCollectionState(const char* sensor_spec)
{
    return SetCollectionState(initalPluginState_, sensor_spec);
}

void RuntimeMetrics::TrackSensorLabel(const char* label_name)
{
    if(sensorLabelMap_.end() == sensorLabelMap_.find(label_name)) {
        sensorLabelMap_[label_name] = initalPluginState_;
    }
}

void RuntimeMetrics::BeginDataCollection()
{
    if(false == collectionLock_) {
        collectionLock_ = true;
    }
}

void RuntimeMetrics::EndDataCollection()
{
    if(true == collectionLock_) {
        collectionLock_ = false;
    }
}

bool RuntimeMetrics::IsTrackingLabel(const char* label) const
{
    if(sensorLabelMap_.end() != sensorLabelMap_.find(string(label))) {
        return true;
    } else {
        return false;
    }
}

int RuntimeMetrics::CountOfCollectedLabels()
{
    int count = 0;
    for(LabelStateMapItr it = sensorLabelMap_.begin(); sensorLabelMap_.end() != it; ++it) {
        if(true == sensorLabelMap_[it->first]) {
            ++count;
        }
    }
    return count;
}

bool RuntimeMetrics::IsForMyDatagroup(const char* sensor_spec) const
{
    String datagroup = GetSensorDatagroupFromSpec(sensor_spec);
    if("all" == datagroup || (datagroup.size() == datagroup_.size() && datagroup == datagroup_)) {
        return true;
    }
    return false;
}

RuntimeMetrics::String RuntimeMetrics::GetSensorLabelFromSpec(const char* sensor_spec) const
{
    String spec = sensor_spec;
    size_t pos = spec.find(':');
    String datagroup = spec.substr(0, pos);
    String label;
    if(pos != String::npos) {
        label = spec.substr(pos + 1);
    }
    return label;
}

RuntimeMetrics::String RuntimeMetrics::GetSensorDatagroupFromSpec(const char* sensor_spec) const
{
    String spec = sensor_spec;
    size_t pos = spec.find(':');
    String datagroup = spec.substr(0, pos);
    return datagroup;
}

bool RuntimeMetrics::WaitForDataCollection(int seconds)
{
    bool val = collectionLock_; // make local copy
    if(false == val) {
        return true;
    }
    time_t start = time(NULL);
    struct timespec period;
    period.tv_sec = 0;
    period.tv_nsec = 100000000; // 100 ms or 0.1 sec
    while (true == val && (time(NULL) - start) <= seconds) {
        nanosleep(&period, NULL);
        val = collectionLock_; // update local copy
    }
    return (!val);
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

    void orcm_sensor_base_runtime_metrics_track(void* runtime_metrics, const char* sensor_label)
    {
        RuntimeMetrics* collection = static_cast<RuntimeMetrics*>(runtime_metrics);
        collection->TrackSensorLabel(sensor_label);
    }

    void orcm_sensor_base_runtime_metrics_begin(void* runtime_metrics)
    {
        RuntimeMetrics* collection = static_cast<RuntimeMetrics*>(runtime_metrics);
        collection->BeginDataCollection();
    }

    void orcm_sensor_base_runtime_metrics_end(void* runtime_metrics)
    {
        RuntimeMetrics* collection = static_cast<RuntimeMetrics*>(runtime_metrics);
        collection->EndDataCollection();
    }

    int orcm_sensor_base_runtime_metrics_active_label_count(void* runtime_metrics)
    {
        RuntimeMetrics* collection = static_cast<RuntimeMetrics*>(runtime_metrics);
        return collection->CountOfCollectedLabels();
    }
} // extern "C"
