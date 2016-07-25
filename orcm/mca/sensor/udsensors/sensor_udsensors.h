/*
 * Copyright (c) 2016 Intel, Inc. All rights reserved.
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
 * UDSENSORS User-Defined sensors
 */
#ifndef ORCM_SENSOR_UDSENSORS_H
#define ORCM_SENSOR_UDSENSORS_H

BEGIN_C_DECLS
#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"

typedef struct {
    orcm_sensor_base_component_t super;
    bool test;
    char* policy;
    bool use_progress_thread;
    int sample_rate;
    bool collect_metrics;
    void* runtime_metrics;
    int64_t diagnostics;
    char* udpath;
} orcm_sensor_udsensors_component_t;

typedef struct {
    opal_event_base_t *ev_base;
    bool ev_active;
    int sample_rate;
} orcm_sensor_udsensors_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_udsensors_component_t mca_sensor_udsensors_component;
extern orcm_sensor_base_module_t orcm_sensor_udsensors_module;

END_C_DECLS

#endif
