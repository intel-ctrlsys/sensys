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
void orcm_sensor_base_runtime_metrics_track(void* runtime_metrics, const char* sensor_label);
void orcm_sensor_base_runtime_metrics_begin(void* runtime_metrics);
void orcm_sensor_base_runtime_metrics_end(void* runtime_metrics);
int orcm_sensor_base_runtime_metrics_active_label_count(void* runtime_metrics);
#ifdef __cplusplus
}; // extern "C"

#include <string>
#include <map>

#ifdef GTEST_MOCK_TESTING
    #define PRIVATE public
#else
    #define PRIVATE private
#endif

class RuntimeMetrics
{
    private: // typedef aliases
        typedef std::map<std::string,bool> LabelStateMap;
        typedef LabelStateMap::const_iterator LabelStateMapCItr;
        typedef LabelStateMap::iterator LabelStateMapItr;
        typedef std::string String;

    public:
        RuntimeMetrics(const char* datagroup, bool global_collect_metrics, bool plugin_collect_metrics);

        bool DoCollectMetrics(const char* sensor_label = NULL);

        int SetCollectionState(bool newState, const char* sensor_spec);
        int ResetCollectionState(const char* sensor_spec);

        int CountOfCollectedLabels();
        void TrackSensorLabel(const char* label_name);
        void BeginDataCollection();
        void EndDataCollection();

        bool IsTrackingLabel(const char* label) const;

    PRIVATE:
        bool IsForMyDatagroup(const char* sensor_spec) const;
        String GetSensorLabelFromSpec(const char* sensor_spec) const;
        String GetSensorDatagroupFromSpec(const char* sensor_spec) const;
        bool WaitForDataCollection(int seconds);

    PRIVATE:
        String datagroup_;
        bool initalPluginState_;
        bool currentPluginState_;
        LabelStateMap sensorLabelMap_;
        // Thread safe for reading and one thread will call being and end only.
        bool collectionLock_;
}; // class RuntimeMetrics

#undef PRIVATE

#endif // __cplusplus

#endif // BASE_SENSOR_RUNTIME_METRICS_H
