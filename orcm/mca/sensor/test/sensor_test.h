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
 * TEST resource manager sensor
 */
#ifndef ORCM_SENSOR_TEST_H
#define ORCM_SENSOR_TEST_H

#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"

BEGIN_C_DECLS

typedef struct {
    orcm_sensor_base_component_t super;
    bool test;
} orcm_sensor_test_component_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_test_component_t mca_sensor_test_component;
extern orcm_sensor_base_module_t orcm_sensor_test_module;


END_C_DECLS

#endif
