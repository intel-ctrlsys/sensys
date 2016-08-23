/*
 * Copyright (c) 2015-2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SNMP_H
#define SNMP_H

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/snmp/sensor_snmp.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/snmp/sensor_snmp.h"
#include "orcm/util/vardata.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/sensor/snmp/sensor_snmp.h"
#include "orcm/util/vardata.h"

#include <time.h>
#include <string>
#include <map>
#include <vector>
#include <stdexcept>

extern "C" {
    #include "orcm/runtime/orcm_globals.h"
}

class snmpCollector;
class RuntimeMetrics;

#ifdef GTEST_MOCK_TESTING
    #define PRIVATE public
#else
    #define PRIVATE private
#endif

class snmp_impl
{
    public:
        snmp_impl();
        ~snmp_impl();

        int init(void);
        void finalize(void);
        void start(orte_jobid_t job);
        void stop(orte_jobid_t job);
        void sample(orcm_sensor_sampler_t *sampler);
        void log(opal_buffer_t *buf);
        void set_sample_rate(int sample_rate);
        void get_sample_rate(int *sample_rate);
        int enable_sampling(const char* sensor_spec);
        int disable_sampling(const char* sensor_spec);
        int reset_sampling(const char* sensor_spec);
        void inventory_collect(opal_buffer_t *inventory_snapshot);
        void inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);

    PRIVATE: // Static Callback Relays
        static void perthread_snmp_sample_relay(int fd, short args, void *cbdata);
        static void my_inventory_log_cleanup(int dbhandle, int status,
                        opal_list_t *kvs, opal_list_t *output, void *cbdata);

    PRIVATE: // In-Object Callback Methods
        void perthread_snmp_sample();
        void collectAndPackDataSamples(opal_buffer_t *buffer);
        void packSamplesIntoBuffer(opal_buffer_t *buffer, const std::vector<vardata>& dataSamples);
        std::vector<vardata> unpackSamplesFromBuffer(opal_buffer_t *buffer);
        void packPluginName(opal_buffer_t* buffer);
        void collect_sample(bool perthread = false);
        void ev_pause();
        void ev_resume();
        void ev_create_thread();
        void ev_destroy_thread();
        void load_mca_variables();
        std::vector<vardata> getOIDsVardataVector(snmpCollector sc);
        void generate_test_vector();
        void generate_test_inv_vector(opal_buffer_t *inventory_snapshot);
        std::vector<vardata> generate_data();
        void printInitErrorMsg(const char *extraMsg);
        void allocateAnalyticsObjects(opal_list_t **key, opal_list_t **non_compute);
        void releaseAnalyticsObjects(opal_list_t **key, opal_list_t **non_compute,
                                     orcm_analytics_value_t **analytics_vals);
        void checkAnalyticsVals(orcm_analytics_value_t *analytics_vals);
        void setAnalyticsKeys(opal_list_t *key);
        void prepareDataForAnalytics(vardata& ctime,
                                     opal_list_t *key,
                                     opal_list_t *non_compute,
                                     opal_buffer_t *buf,
                                     orcm_analytics_value_t **analytics_vals);
        bool haveDataInBuffer(opal_buffer_t *buffer);

PRIVATE: // Fields (i.e. state)
        std::vector<snmpCollector> collectorObj_;
        std::string hostname_;
        std::string config_file_;
        bool ev_paused_;
        opal_event_base_t* ev_base_;
        orcm_sensor_sampler_t* snmp_sampler_;
        RuntimeMetrics* runtime_metrics_;
        int64_t diagnostics_;
        static const std::string plugin_name_;
};
#undef PRIVATE

class unableToAllocateObj : public std::runtime_error {
    public:
    unableToAllocateObj() : std::runtime_error( "Unable to allocate object in memory" ) {}
};

class corruptedInventoryBuffer : public std::runtime_error {
    public:
    corruptedInventoryBuffer() : std::runtime_error( "Inventory buffer is corrupted") {}
};

class noSnmpConfigAvailable : public std::runtime_error {
    public:
    noSnmpConfigAvailable() : std::runtime_error( "No snmp configuration available." ) {}
};

class noDataSampled : public std::runtime_error {
    public:
    noDataSampled() : std::runtime_error( "No available data for sampling." ) {}
};

class unableToPackData : public std::runtime_error {
    public:
    unableToPackData() : std::runtime_error( "Unable to pack data samples." ) {}
};

#endif /* SNMP_H */
