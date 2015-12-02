/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ANALYZE_COUNTER_H
#define ANALYZE_COUNTER_H

// C
#include <time.h>
#include <stdint.h>

// C++
#include <string>
#include <deque>
#include <vector>
#include <utility>
#include <map>

#ifdef GTEST_MOCK_TESTING
#define PRIVATE public
#else
#define PRIVATE private
#endif // GTEST_MOCK_TESTING

#ifdef __cplusplus
extern "C" {
#endif
    typedef void (*analyze_counter_event_fn_t)(uint32_t error_count, time_t elapsed, const std::vector<uint32_t>& data,
                                               void* cb_data);
#ifdef __cplusplus
}
#endif

class analyze_counter
{
    public:
        analyze_counter();
        virtual ~analyze_counter();

        void set_window_size(const std::string& dur_str);
        void set_threshold(uint32_t limit);
        void add_value(uint32_t val, time_t tval, analyze_counter_event_fn_t event_cb, void* cb_data);

    PRIVATE:
        time_t window_size_;
        uint32_t threshold_;
        std::map<char,uint32_t> lookup_factor_;
        std::deque<std::pair<uint32_t,time_t> > past_data_;
};

#endif // ANALYZE_COUNTER_H
