/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved.
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/**
 * @file syslog resource manager sensor
 */
#ifndef ORCM_SENSOR_SYSLOG_H
#define ORCM_SENSOR_SYSLOG_H

#include "orcm_config.h"
#include "orcm/mca/sensor/sensor.h"

BEGIN_C_DECLS

typedef struct {
    orcm_sensor_base_component_t super;
    bool use_progress_thread;
    int sample_rate;
    bool collect_metrics;
    void* runtime_metrics;
    int64_t diagnostics;
    bool test;
} orcm_sensor_syslog_component_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_syslog_component_t mca_sensor_syslog_component;
extern orcm_sensor_base_module_t orcm_sensor_syslog_module;

typedef struct {
    struct timeval tv_curr;
    struct timeval tv_prev;
    unsigned long long interval;
} __time_val;

typedef struct {
    opal_event_base_t *ev_base;
    bool ev_active;
    int sample_rate;
} orcm_sensor_syslog_t;

END_C_DECLS

#endif
