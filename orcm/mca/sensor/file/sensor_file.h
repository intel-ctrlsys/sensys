/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved. 
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
 * File movement sensor 
 */
#ifndef ORCM_SENSOR_FILE_H
#define ORCM_SENSOR_FILE_H

#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"

BEGIN_C_DECLS

struct orcm_sensor_file_component_t {
    orcm_sensor_base_component_t super;
    int sample_rate;
    char *file;
    bool check_size;
    bool check_access;
    bool check_mod;
    int limit;
};
typedef struct orcm_sensor_file_component_t orcm_sensor_file_component_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_file_component_t mca_sensor_file_component;
extern orcm_sensor_base_module_t orcm_sensor_file_module;


END_C_DECLS

#endif
