/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef HOST_ANALYZE_COUNTERS_H
#define HOST_ANALYZE_COUNTERS_H

// C
#include <time.h>
#include <stdint.h>

// C++
#include <string>
#include <vector>
#include <map>

#ifdef GTEST_MOCK_TESTING
#define PRIVATE public
#else
#define PRIVATE private
#endif // GTEST_MOCK_TESTING

#ifdef __cplusplus
extern "C" {
#endif
    typedef void (*host_analyze_counters_event_fn_t)(const std::string& host, const std::string& label, uint32_t error_count,
                                                     time_t elapsed, const std::vector<uint32_t>& data, void* cb_data);
#ifdef __cplusplus
}
#endif

class analyze_counter;

class host_analyze_counters
{
    typedef std::map<std::string,analyze_counter*> label_map;
    typedef std::map<std::string,label_map> host_map;
    typedef label_map::iterator label_iterator;
    typedef host_map::iterator host_iterator;

    public: // internal but public for relay functionality
        class current_state {
            public:
                current_state() : this_ptr(NULL), callback(NULL), host(""), label(""), user_data(NULL) {}

                host_analyze_counters* this_ptr;
                host_analyze_counters_event_fn_t callback;
                std::string host;
                std::string label;
                void* user_data;
        };
    public:
        host_analyze_counters(void* user_data = NULL);
        virtual ~host_analyze_counters();

        void set_window_size(const std::string& dur_str);
        void set_threshold(uint32_t limit);
        void set_value_label_mask(const std::string& label_mask);
        void add_value(const std::string& host, const std::string& label, uint32_t val, time_t tval,
                       host_analyze_counters_event_fn_t event_cb, void* cb_data);
        bool is_wanted_label(const std::string& label);
        void* get_user_data() const { return user_data_; }
        uint32_t get_threshold() const { return threshold_; }

    public: // Static Callbacks
        static void analyze_counter_relay(uint32_t error_count, time_t elapsed, const std::vector<uint32_t>& data,
                                          void* cb_data);
        void analyze_counter_callback(uint32_t error_count, time_t elapsed, const std::vector<uint32_t>& data,
                                      void* cb_data);

    PRIVATE:
        host_map data_db_;
        std::string window_size_;
        uint32_t threshold_;
        std::string label_mask_;
        std::vector<std::string> mask_parts_;
        void* user_data_;
};

#endif // HOST_ANALYZE_COUNTERS_H
