/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sensor_snmp.h"

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_snmp_module = {
    snmp_init_relay,
    snmp_finalize_relay,
    snmp_start_relay,
    snmp_stop_relay,
    snmp_sample_relay,
    snmp_log_relay,
    NULL,
    NULL,
    snmp_set_sample_rate_relay,
    snmp_get_sample_rate_relay,
    snmp_enable_sampling_relay,
    snmp_disable_sampling_relay,
    snmp_reset_sampling_relay
};
