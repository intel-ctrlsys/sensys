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
#ifndef ORCM_SENSOR_FT_TESTER_H
#define ORCM_SENSOR_FT_TESTER_H

#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"
#include "opal/util/alfg.h"

BEGIN_C_DECLS

struct orcm_sensor_ft_tester_component_t {
    orcm_sensor_base_component_t super;
    float fail_prob;
    float daemon_fail_prob;
    bool multi_fail;
};
typedef struct orcm_sensor_ft_tester_component_t orcm_sensor_ft_tester_component_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_ft_tester_component_t mca_sensor_ft_tester_component;
extern orcm_sensor_base_module_t orcm_sensor_ft_tester_module;

extern opal_rng_buff_t orcm_sensor_ft_rng_buff;

END_C_DECLS

#endif
