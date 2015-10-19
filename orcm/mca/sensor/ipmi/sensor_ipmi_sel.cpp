/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sensor_ipmi_sel.h"

#include "ipmi_sel_collector.h"
#include "ipmi_credentials.h"

extern "C" {

    bool process_node_sel_logs(const char* hostname, const char* bmc_ip, const char* username,
                               const char* password, const char* persist_filename,
                               sel_error_callback_fn_t error_callback, sel_ras_event_fn_t ras_callback,
                               void* user_object)
    {
        ipmi_credentials creds(bmc_ip, username, password);
        ipmi_sel_collector scanner(hostname, creds, error_callback, user_object);

        if(false == scanner.is_bad()) {
            scanner.load_last_record_id(persist_filename);
            return scanner.scan_new_records(ras_callback);
        }

        return false;
    }

} // extern "C"
