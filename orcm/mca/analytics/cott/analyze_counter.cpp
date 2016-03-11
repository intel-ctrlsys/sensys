/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "analyze_counter.h"

// C
#include <stdlib.h>
#include <errno.h>

using namespace std;

analyze_counter::analyze_counter()
    : window_size_(1), threshold_(1)
{
    lookup_factor_['s'] = 1;
    lookup_factor_['m'] = 60;
    lookup_factor_['h'] = 3600;
    lookup_factor_['d'] = 86400;
}

analyze_counter::~analyze_counter()
{
}

void analyze_counter::set_window_size(const std::string& dur_str)
{
    if(false == dur_str.empty()) {
        size_t start = dur_str.find_first_not_of(" \t\n\r\f");
        if(string::npos != start) {
            size_t last = dur_str.find_last_not_of(" \t\n\r\f");
            string str = dur_str.substr(start, last - start + 1);
            char type = str[str.size()-1];
            if('s' != type && 'm' != type && 'h' != type && 'd' != type) {
                type = 's';
            } else {
                str.erase(str.size()-1);
            }
            if(false == str.empty() && string::npos == str.find_first_not_of("0123456789")) {
                uint32_t factor = lookup_factor_[type];
                time_t val = (time_t)strtoull(str.c_str(), NULL, 10) * factor;
                if(1 <= val) {
                    window_size_ = val;
                }
            }
        }
    }
}

void analyze_counter::set_threshold(uint32_t limit)
{
    if(0 != limit) {
        threshold_ = limit;
    }
}

void analyze_counter::add_value(uint32_t val, time_t tval, analyze_counter_event_fn_t event_cb, void* cb_data)
{
    // Remove expired entries.
    if(0 != past_data_.size()) {
        time_t tdiff = tval - past_data_.front().second;
        while(tdiff > window_size_) {
            past_data_.pop_front();
            if(0 < past_data_.size()) {
                tdiff = tval - past_data_.front().second;
            } else {
                tdiff = 0;
            }
        }
    }

    // Is the new value less than the previous values (degenerate case)?
    if(0 != past_data_.size() && (val < past_data_.front().first || val < past_data_.back().first)) {
        // Set 'val' as the new starting point.
        past_data_.clear();
    } else {
        // Do I fire the event?
        uint32_t error_count = 0;
        if(0 != past_data_.size()) {
            error_count = val - past_data_.front().first;
        }
        if(error_count >= threshold_) {
            // Yes, fire the callback
            vector<uint32_t> raw_data;
            for(size_t i = 0; i < past_data_.size(); ++i) {
                raw_data.push_back(past_data_[i].first);
            }
            raw_data.push_back(val);
            event_cb(error_count, tval - past_data_.front().second, raw_data, cb_data);

            // Clear all old events
            past_data_.clear();
        }
    }
    // Add new entry.
    pair<uint32_t,time_t> new_val(val, tval);
    past_data_.push_back(new_val);
}
