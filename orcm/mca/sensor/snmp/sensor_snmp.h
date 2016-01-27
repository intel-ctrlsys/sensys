/*
 * Copyright (c) 2015-2016 Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/**
 * @file
 *
 * SNMP devices sensor
 */
#ifndef ORCM_SENSOR_SNMP_H
#define ORCM_SENSOR_SNMP_H

#include "orcm_config.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orcm/mca/sensor/sensor.h"

BEGIN_C_DECLS

typedef struct {
    orcm_sensor_base_component_t super;
    bool use_progress_thread;
    int sample_rate;
    char* config_file;
    bool collect_metrics;
} orcm_sensor_snmp_component_t;

typedef struct {
    opal_event_base_t *ev_base;
    bool ev_active;
    int sample_rate;
} orcm_sensor_snmp_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_snmp_component_t mca_sensor_snmp_component;
extern orcm_sensor_base_module_t orcm_sensor_snmp_module;

extern int snmp_init_relay(void);
extern void snmp_finalize_relay(void);
extern void snmp_start_relay(orte_jobid_t jobid);
extern void snmp_stop_relay(orte_jobid_t jobid);
extern void snmp_sample_relay(orcm_sensor_sampler_t *sampler);
extern void snmp_log_relay(opal_buffer_t *sample);
extern void snmp_set_sample_rate_relay(int sample_rate);
extern void snmp_get_sample_rate_relay(int *sample_rate);
END_C_DECLS

#endif
