/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/* Project Headers */
#include "ipmi_ts_sel_collector.h"
#include "ipmi_ts_persist_sel_record_id.h"
#include "ipmi_ts_sel_record.h"

/* C++ Headers */
#include <exception>
#include <sstream>
#include <iomanip>

/* C Headers */
#include <stdio.h>
#include <memory.h>
#include <ipmicmd.h>
#include <pthread.h>

/* Implementation Defines */
#define INVALID_RECORD_ID       ((uint16_t)0)
#define END_OF_RECORDS          ((uint16_t)0xffff)
#define SAFE_DELETE(x)          if(NULL != x) { delete x; x = NULL; }
/* IPMI Debug Define */
#define IPMI_DEBUG ((char)0) /* 0 for no IPMI debug; 1 if you wish to see detailed debug information */

using namespace std;


//////////////////////////////////////////////////////////////////////////////
// Public API
ipmi_ts_sel_collector::ipmi_ts_sel_collector(const char* hostname, sel_error_callback_fn_t callback, void* user_object)
    : ras_callback_(NULL), bad_instance_(false), last_record_id_(INVALID_RECORD_ID), next_record_id_(END_OF_RECORDS), persist_record_(NULL),
      hostname_(hostname), error_callback_(callback), response_buffer_size_(0), current_record_(NULL), read_first_record_(false), user_object_(user_object)
{
    memset((void*)current_sel_response_, 0, sizeof(current_sel_response_));
    memset((void*)current_sel_request_, 0, sizeof(current_sel_request_));
}

ipmi_ts_sel_collector::ipmi_ts_sel_collector(const ipmi_ts_sel_collector&)
    : ras_callback_(NULL), bad_instance_(true), last_record_id_(INVALID_RECORD_ID), next_record_id_(END_OF_RECORDS), persist_record_(NULL),
      hostname_(""), error_callback_(NULL), response_buffer_size_(0), current_record_(NULL), read_first_record_(false), user_object_(NULL)
{
}

ipmi_ts_sel_collector::~ipmi_ts_sel_collector()
{
    if(false == bad_instance_) {
        ipmi_close();
        SAFE_DELETE(persist_record_);
        SAFE_DELETE(current_record_);
    }
}

bool ipmi_ts_sel_collector::is_bad() const
{
    return bad_instance_;
}

bool ipmi_ts_sel_collector::load_last_record_id(const char* filename)
{
    if(false == bad_instance_) {
        SAFE_DELETE(persist_record_);
        persist_record_ = new ipmi_ts_persist_sel_record_id(hostname_.c_str(), error_callback_);
        persist_record_->load_last_record_id(filename);
        last_record_id_ = persist_record_->get_record_id();
        return true;
    }
    return false;
}

bool ipmi_ts_sel_collector::scan_new_records(sel_ras_event_fn_t callback)
{
    if(true == bad_instance_) {
        return false;
    }
    ras_callback_ = callback;

    while(END_OF_RECORDS != next_record_id_ || false == read_first_record_) {
        uint16_t id = (false == read_first_record_)?last_record_id_:next_record_id_;
        if(false == get_next_ipmi_sel_record(id)) {
            return false;
        }
        read_first_record_ = true;
    }
    persist_record_->set_record_id(last_record_id_);
    return true;
}


//////////////////////////////////////////////////////////////////////////////
// Protected virtual API
void ipmi_ts_sel_collector::conditionally_send_ras_event() const
{
    if(NULL != current_record_ && NULL != ras_callback_) {
        ras_callback_(current_record_->get_decoded_string(), hostname_.c_str(), user_object_);
    }
}


//////////////////////////////////////////////////////////////////////////////
// Private Implmentation Methods
void ipmi_ts_sel_collector::build_current_request(uint16_t id)
{
    current_sel_request_[5] = 0xff;
    current_sel_request_[2] = (id & 0x00ff);
    current_sel_request_[3] = (id & 0xff00) >> 8;
}

void ipmi_ts_sel_collector::report_ipmi_cmd_failure(uint16_t id)
{
    stringstream ss;
    ss << "Failed to retrieve IPMI SEL record ID '0x" << hex << setw(4) << setfill('0') << id << "' from host: " << hostname_;
    report_error(0, ss.str().c_str());
}

bool ipmi_ts_sel_collector::test_for_ipmi_cmd_error(uint16_t id, int cmd_result, unsigned char ipmi_condition)
{
    if(0 != cmd_result || 0 != ipmi_condition) {
        bad_instance_ = true;
        next_record_id_ = INVALID_RECORD_ID;
        report_ipmi_cmd_failure(id);
        return false;
    }
    return true;
}

bool ipmi_ts_sel_collector::execute_ipmi_get_sel_entry(uint16_t id)
{
    int sz = RESPONSE_BUFFER_SIZE;
    unsigned char cc = 0;
    int rv = ipmi_cmd(GET_SEL_ENTRY, (uchar*)current_sel_request_, REQUEST_BUFFER_SIZE,
                      (uchar*)current_sel_response_, &sz, &cc, IPMI_DEBUG);
    if(false == test_for_ipmi_cmd_error(id, rv, cc)) {
        return false;
    }
    response_buffer_size_ = (size_t)sz;
    last_record_id_ = id;
    next_record_id_ = (uint16_t)current_sel_response_[0] | ((uint16_t)current_sel_response_[1] << 8);
    return true;
}

bool ipmi_ts_sel_collector::get_next_ipmi_sel_record(uint16_t id)
{
    if(id == END_OF_RECORDS || true == bad_instance_) {
        return false;
    }
    build_current_request(id);
    if(false == execute_ipmi_get_sel_entry(id)) {
        return false;
    }

    SAFE_DELETE(current_record_);
    current_record_ = new ipmi_ts_sel_record((const uint8_t*)current_sel_response_);

    if(true == read_first_record_ || 0 == id) {
        conditionally_send_ras_event();
    }

    return true;
}

void ipmi_ts_sel_collector::report_error(int level, const char* msg) const
{
    if(NULL != error_callback_) {
        error_callback_(level, msg);
    }
}
