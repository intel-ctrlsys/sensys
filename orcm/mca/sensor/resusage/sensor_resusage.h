/*
 * Copyright (c) 2009-2011 Cisco Systems, Inc.  All rights reserved. 
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
#ifndef ORCM_SENSOR_RESUSAGE_H
#define ORCM_SENSOR_RESUSAGE_H

#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"

BEGIN_C_DECLS

struct orcm_sensor_resusage_component_t {
    orcm_sensor_base_component_t super;
    int sample_rate;
    float node_memory_limit;
    float proc_memory_limit;
    bool log_node_stats;
    bool log_process_stats;
};
typedef struct orcm_sensor_resusage_component_t orcm_sensor_resusage_component_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_resusage_component_t mca_sensor_resusage_component;
extern orcm_sensor_base_module_t orcm_sensor_resusage_module;


END_C_DECLS

#endif
