/*
 * Copyright (c) 2015 Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_SENSOR_IPMI_SEL_H
#define ORCM_SENSOR_IPMI_SEL_H

#include "sel_callback_defs.h"

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

bool process_node_sel_logs(const char* hostname, const char* bmc_ip, const char* username,
                           const char* password, const char* persist_filename,
                           sel_error_callback_fn_t error_callback, sel_ras_event_fn_t ras_callback,
                           void* user_object);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
