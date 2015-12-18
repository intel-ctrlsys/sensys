/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SNMP_H
#define SNMP_H

#ifdef GTEST_MOCK_TESTING
    #define PRIVATE public
#else
    #define PRIVATE private
#endif

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/snmp/sensor_snmp.h"

#include <time.h>
#include <string>
#include <map>
#include <vector>
#include <stdexcept>

extern "C" {
    #include "orcm/runtime/orcm_globals.h"
}

class snmpCollector;

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

    PRIVATE: // Static Callback Relays
        static void perthread_snmp_sample_relay(int fd, short args, void *cbdata);

    PRIVATE: // In-Object Callback Methods
        void perthread_snmp_sample();
        void collectAndPackDataSamples(opal_buffer_t *buffer);
        void packPluginName(opal_buffer_t* buffer);
        void collect_sample(bool perthread = false);
        void ev_pause();
        void ev_resume();
        void ev_create_thread();
        void ev_destroy_thread();

    PRIVATE: // Fields (i.e. state)
        std::vector<snmpCollector> collectorObj_;
        std::string hostname_;
        bool ev_paused_;
        opal_event_base_t* ev_base_;
        orcm_sensor_sampler_t* snmp_sampler_;

        static const std::string plugin_name_;
};

class unableToAllocateObj : public std::runtime_error {
    public:
    unableToAllocateObj() : std::runtime_error( "Unable to allocate object in memory" ) {}
};

class incorrectConfig : public std::runtime_error {
    public:
    incorrectConfig() : std::runtime_error( "Incorrect configuration parameter" ) {}
};

#endif /* SNMP_H */
