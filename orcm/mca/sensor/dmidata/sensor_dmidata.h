/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
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
 * DMIDATA resource manager sensor 
 */
#ifndef ORCM_SENSOR_DMIDATA_H
#define ORCM_SENSOR_DMIDATA_H

#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"
#include "sensor_dmidata_decls.h"
BEGIN_C_DECLS

typedef struct {
    orcm_sensor_base_component_t super;
    bool test;
} orcm_sensor_dmidata_component_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_dmidata_component_t mca_sensor_dmidata_component;
extern orcm_sensor_base_module_t orcm_sensor_dmidata_module;


END_C_DECLS

#endif
