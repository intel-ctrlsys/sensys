/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved.
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
 * ERRCOUNTS resource manager sensor
 */
#ifndef ORCM_SENSOR_ERRCOUNTS_H
#define ORCM_SENSOR_ERRCOUNTS_H

#include "orcm_config.h"
#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/sensor/sensor.h"

BEGIN_C_DECLS

typedef struct {
    orcm_sensor_base_component_t super;
    bool use_progress_thread;
    int sample_rate;
} orcm_sensor_errcounts_component_t;

typedef struct {
    opal_event_base_t *ev_base;
    bool ev_active;
    int sample_rate;
} orcm_sensor_errcounts_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_errcounts_component_t mca_sensor_errcounts_component;
extern orcm_sensor_base_module_t orcm_sensor_errcounts_module;

extern int errcounts_init_relay(void);
extern void errcounts_finalize_relay(void);
extern void errcounts_start_relay(orte_jobid_t jobid);
extern void errcounts_stop_relay(orte_jobid_t jobid);
extern void errcounts_sample_relay(orcm_sensor_sampler_t *sampler);
extern void errcounts_log_relay(opal_buffer_t *sample);
extern void errcounts_set_sample_rate_relay(int sample_rate);
extern void errcounts_get_sample_rate_relay(int *sample_rate);
extern void errcounts_inventory_collect_relay(opal_buffer_t *inventory_snapshot);
extern void errcounts_inventory_log_relay(char *hostname, opal_buffer_t *inventory_snapshot);
END_C_DECLS

#endif
