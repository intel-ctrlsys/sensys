/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_SENSOR_IPMI_TS_H
#define ORCM_SENSOR_IPMI_TS_H

BEGIN_C_DECLS
#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"

typedef struct {
    orcm_sensor_base_component_t super;
    bool test;
    bool dfx;
    char* policy;
    bool use_progress_thread;
    int sample_rate;
    bool collect_metrics;
    void* runtime_metrics;
    int agents;
    int64_t diagnostics;
} orcm_sensor_ipmi_ts_component_t;

typedef struct {
    opal_event_base_t *ev_base;
    bool ev_active;
    int sample_rate;
} orcm_sensor_ipmi_ts_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_ipmi_ts_component_t mca_sensor_ipmi_ts_component;
extern orcm_sensor_base_module_t orcm_sensor_ipmi_ts_module;

END_C_DECLS

#endif
