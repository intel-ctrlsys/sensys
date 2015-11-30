/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ERRCOUNTS_H
#define ERRCOUNTS_H

#ifdef GTEST_MOCK_TESTING
    #define PRIVATE public
#else
    #define PRIVATE private
#endif

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/errcounts/sensor_errcounts.h"

// C
#include <time.h>

// C++
#include <string>
#include <map>
#include <vector>

extern "C" {
    // ORCM
    #include "orcm/runtime/orcm_globals.h"
}
class edac_collector;

class errcounts_impl
{
    public:
        errcounts_impl();
        ~errcounts_impl();

        int init(void);
        void finalize(void);
        void start(orte_jobid_t job);
        void stop(orte_jobid_t job);
        void sample(orcm_sensor_sampler_t *sampler);
        void log(opal_buffer_t *buf);
        void inventory_collect(opal_buffer_t *inventory_snapshot);
        void inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);
        void set_sample_rate(int sample_rate);
        void get_sample_rate(int *sample_rate);

    PRIVATE: // Static Callback Relays
        static void error_callback_relay(const char* pathname, int error_number, void* user_data);
        static void data_callback_relay(const char* label, int error_count, void* user_data);
        static void inventory_callback_relay(const char* label, const char* name, void* user_data);
        static void perthread_errcounts_sample_relay(int fd, short args, void *cbdata);
        static void my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata);

    PRIVATE: // In-Object Callback Methods
        void error_callback(const char* pathname, int error_number);
        void data_callback(const char* label, int error_count);
        void inventory_callback(const char* label, const char* name);
        void perthread_errcounts_sample();

        /* Helper Methods */
        void collect_sample(bool perthread = false);
        void ev_pause();
        void ev_resume();
        void ev_create_thread();
        void ev_destroy_thread();
        bool pack_string(opal_buffer_t* buffer, const std::string& str) const;
        bool pack_int32(opal_buffer_t* buffer, int32_t value) const;
        bool pack_timestamp(opal_buffer_t* buffer) const;
        bool pack_data_sample(opal_buffer_t* buffer);
        bool pack_inv_sample(opal_buffer_t* buffer);
        bool unpack_string(opal_buffer_t* buffer, std::string& str) const;
        bool unpack_int32(opal_buffer_t* buffer, int32_t& value) const;
        bool unpack_timestamp(opal_buffer_t* buffer, struct timeval& timestamp) const;
        bool unpack_data_sample(opal_buffer_t* buffer);
        bool unpack_inv_sample(opal_buffer_t* buffer);
        orcm_value_t* make_orcm_value_string(const char* name, const char* value);

    PRIVATE: // Fields (i.e. state)
        edac_collector* collector_;
        std::string hostname_;
        bool ev_paused_;
        opal_event_base_t* ev_base_;
        orcm_sensor_sampler_t* errcounts_sampler_;
        std::vector<std::string> data_samples_labels_;
        std::vector<int32_t> data_samples_values_;
        std::map<std::string,std::string> inv_samples_;
        std::vector<std::string> log_samples_labels_;
        std::vector<int32_t> log_samples_values_;
        std::map<std::string,std::string> inv_log_samples_;

        static const std::string plugin_name_;
};

#endif /* ERRCOUNTS_H */
