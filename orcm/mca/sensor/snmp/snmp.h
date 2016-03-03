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
        void packPluginName(opal_buffer_t* buffer);
        void collect_sample(bool perthread = false);
        void ev_pause();
        void ev_resume();
        void ev_create_thread();
        void ev_destroy_thread();
        void load_mca_variables();
        std::vector<vardata> getOIDsVardataVector(snmpCollector sc);

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

class incorrectConfig : public std::runtime_error {
    public:
    incorrectConfig() : std::runtime_error( "Incorrect configuration parameter" ) {}
};

class corruptedInventoryBuffer : public std::runtime_error {
    public:
    corruptedInventoryBuffer() : std::runtime_error( "Inventory buffer is corrupted") {}
};

#endif /* SNMP_H */
