/*
 * Copyright (c) 2013-2016 Intel Corporation. All rights reserved.
 *
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/**
 * @file
 *
 * FREQ resource manager sensor
 */
#ifndef ORCM_SENSOR_FREQ_H
#define ORCM_SENSOR_FREQ_H

#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"

BEGIN_C_DECLS

typedef struct {
    orcm_sensor_base_component_t super;
    bool test;
    char *policy;
    bool pstate;
    bool use_progress_thread;
    int sample_rate;
    bool collect_metrics;
    void* runtime_metrics;
    int64_t diagnostics;
} orcm_sensor_freq_component_t;

typedef struct {
    opal_event_base_t *ev_base;
    bool ev_active;
    int sample_rate;
} orcm_sensor_freq_t;

typedef struct {
    opal_list_item_t super;
    char *file;     /* sysfs entry file location */
    char *sysname;  /* sysfs entry name */
    unsigned int value;
} pstate_tracker_t;
ORCM_DECLSPEC OBJ_CLASS_DECLARATION(pstate_tracker_t);
extern void freq_ptrk_con(pstate_tracker_t *trk);
extern void freq_ptrk_des(pstate_tracker_t *trk);

ORCM_MODULE_DECLSPEC extern orcm_sensor_freq_component_t mca_sensor_freq_component;
extern orcm_sensor_base_module_t orcm_sensor_freq_module;


END_C_DECLS

#endif
