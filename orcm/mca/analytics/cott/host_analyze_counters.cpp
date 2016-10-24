/*
 * Copyright (c) 2015      Intel Corporation. All rights reserved.
 *
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "host_analyze_counters.h"
#include "analyze_counter.h"

// C
#include <stdlib.h>

// C++
#include <utility>

#define SAFE_DELETE(x) if(NULL != x) delete x; x=NULL

using namespace std;

host_analyze_counters::host_analyze_counters(void* user_data)
    : window_size_("5s"), threshold_(1), user_data_(user_data)
{
}

host_analyze_counters::~host_analyze_counters()
{
    for(host_iterator it = data_db_.begin(); it != data_db_.end(); ++it) {
        for(label_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            SAFE_DELETE(it2->second);
        }
    }
    data_db_.clear();
}

void host_analyze_counters::set_window_size(const std::string& dur_str)
{
    window_size_ = dur_str;
    for(host_iterator it = data_db_.begin(); it != data_db_.end(); ++it) {
        for(label_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            it2->second->set_window_size(window_size_);
        }
    }
}

void host_analyze_counters::set_threshold(uint32_t limit)
{
    threshold_ = limit;
    for(host_iterator it = data_db_.begin(); it != data_db_.end(); ++it) {
        for(label_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            it2->second->set_threshold(threshold_);
        }
    }
}

void host_analyze_counters::set_value_label_mask(const std::string& label_mask)
{
    label_mask_ = label_mask;

    size_t pos = 0;
    size_t next;

    // Get parts to match from mask...
    string part;
    mask_parts_.clear();
    while(pos < label_mask_.size() && string::npos != (next = label_mask_.find('*', pos))) {
        part = label_mask_.substr(pos, next - pos);
        if(false == part.empty()) {
            mask_parts_.push_back(part);
        }
        pos = next + 1;
    }
    if(0 != pos) {
        part = label_mask_.substr(pos);
        if(false == part.empty()) {
            mask_parts_.push_back(part);
        }
    }
}


void host_analyze_counters::add_value(const std::string& host, const std::string& label, uint32_t val,
                                      time_t tval, host_analyze_counters_event_fn_t event_cb, void* cb_data)
{
    if(true == host.empty() || true == label.empty()) {
        return;
    }
    analyze_counter* counter = NULL;
    if(data_db_.end() != data_db_.find(host)) {
        if(data_db_[host].end() != data_db_[host].find(label)) {
            counter = data_db_[host][label];
        } else {
            counter = new analyze_counter();
            counter->set_window_size(window_size_);
            counter->set_threshold(threshold_);
            data_db_[host][label] = counter;
        }
    } else {
        counter = new analyze_counter();
        counter->set_window_size(window_size_);
        counter->set_threshold(threshold_);
        data_db_[host][label] = counter;
    }
    current_state* state = new current_state;
    state->this_ptr = this;
    state->callback = event_cb;
    state->host = host;
    state->label = label;
    state->user_data = cb_data;
    counter->add_value(val, tval, analyze_counter_relay, (void*)state);
}

bool host_analyze_counters::is_wanted_label(const std::string& label)
{
    if(true == label.empty()) {
        return false;
    }
    if(string::npos == label_mask_.find_first_not_of("*")) {
        return true;
    }

    if(0 == mask_parts_.size()) {
        // No wildcards, just compare...
        return label == label_mask_;
    } else {
        // Scan label for parts...
        size_t pos = 0;
        size_t next;
        for(size_t i = 0; i < mask_parts_.size(); ++i) {
            next = label.find(mask_parts_[i], pos);
            if(string::npos == next) {
                return false;
            }
            pos = next + mask_parts_[i].size();
        }
        return true;
    }
}


// analyze_counter callback relay
void host_analyze_counters::analyze_counter_relay(uint32_t error_count, time_t elapsed, const std::vector<uint32_t>& data,
                                                  void* cb_data)
{
    current_state* state = (current_state*)cb_data;
    state->this_ptr->analyze_counter_callback(error_count, elapsed, data, cb_data);
}

void host_analyze_counters::analyze_counter_callback(uint32_t error_count, time_t elapsed, const std::vector<uint32_t>& data,
                                                     void* cb_data)
{
    current_state* state = (current_state*)cb_data;
    if(NULL != state->callback) {
        state->callback(state->host, state->label, error_count, elapsed, data, state->user_data);
    }
    SAFE_DELETE(state);
}
