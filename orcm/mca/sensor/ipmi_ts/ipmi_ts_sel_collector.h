/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_IPMI_SEL_TS_COLLECTOR_H
#define ORCM_IPMI_SEL_TS_COLLECTOR_H

#include "ipmi_ts_sel_callback_defs.h"

#include <stdint.h>
#include <stdlib.h>
#include <string>

#define RESPONSE_BUFFER_SIZE    24
#define REQUEST_BUFFER_SIZE     6

#ifdef GTEST_MOCK_TESTING
#define PRIVATE public
#define PROTECTED public
#else
#define PRIVATE private
#define PROTECTED protected
#endif

class ipmi_ts_persist_sel_record_id;
class ipmi_ts_sel_record;

class ipmi_ts_sel_collector
{
    public:
        ipmi_ts_sel_collector(const char* hostname, sel_error_callback_fn_t callback, void* user_object);
        ipmi_ts_sel_collector(const ipmi_ts_sel_collector& rhs);
        ~ipmi_ts_sel_collector();

        bool is_bad() const;

        bool load_last_record_id(const char* filename);
        bool scan_new_records(sel_ras_event_fn_t callback);

    PROTECTED:
        virtual void conditionally_send_ras_event() const;

    PRIVATE:
        unsigned char            current_sel_response_[RESPONSE_BUFFER_SIZE];
        unsigned char            current_sel_request_[REQUEST_BUFFER_SIZE];
        sel_ras_event_fn_t       ras_callback_;
        bool                     bad_instance_;
        uint16_t                 last_record_id_;
        uint16_t                 next_record_id_;
        ipmi_ts_persist_sel_record_id*   persist_record_;
        std::string              hostname_;
        sel_error_callback_fn_t  error_callback_;
        size_t                   response_buffer_size_;
        ipmi_ts_sel_record*      current_record_;
        bool                     read_first_record_;
        void*                    user_object_;

        bool get_next_ipmi_sel_record(uint16_t id);
        void report_error(int level, const char* msg) const;
        void build_current_request(uint16_t id);
        bool execute_ipmi_get_sel_entry(uint16_t id);
        bool test_for_ipmi_cmd_error(uint16_t id, int cmd_result, unsigned char ipmi_condition);
        void report_ipmi_cmd_failure(uint16_t id);
};

#endif // ORCM_IPMI_SEL_COLLECTOR_H
