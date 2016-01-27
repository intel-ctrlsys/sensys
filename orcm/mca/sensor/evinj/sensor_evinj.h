/*
 * Copyright (c) 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2015-2016 Intel, Inc. All rights reserved
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
 * Process Resource Utilization sensor
 */
#ifndef ORCM_SENSOR_EViNJ_H
#define ORCM_SENSOR_EViNJ_H

#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"
#include "opal/util/alfg.h"

BEGIN_C_DECLS

struct orcm_sensor_evinj_component_t {
    orcm_sensor_base_component_t super;
    float prob;
    opal_rng_buff_t rng_buff;
    char *vector_file;
    bool collect_metrics;
    void* runtime_metrics;
    int64_t diagnostics;
};
typedef struct orcm_sensor_evinj_component_t orcm_sensor_evinj_component_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_evinj_component_t mca_sensor_evinj_component;
extern orcm_sensor_base_module_t orcm_sensor_evinj_module;

END_C_DECLS

#endif
