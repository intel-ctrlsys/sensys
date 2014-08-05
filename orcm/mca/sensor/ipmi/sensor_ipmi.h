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
 * IPMI resource manager sensor 
 */
#ifndef ORCM_SENSOR_IPMI_H
#define ORCM_SENSOR_IPMI_H

#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"
#include "ipmiutil-2.9.3/util/ipmicmd.h"
#include "ipmiutil-2.9.3/util/isensor.h"
#include "sensor_ipmi_decls.h"

BEGIN_C_DECLS

typedef struct {
    orcm_sensor_base_component_t super;
    //char *sensors;
    bool test;
    //ipmi_capsule_t sample;
    char *bmc_username;
    char *bmc_password;
} orcm_sensor_ipmi_component_t;

struct ipmi_properties *first_node;

ORCM_MODULE_DECLSPEC extern orcm_sensor_ipmi_component_t mca_sensor_ipmi_component;
extern orcm_sensor_base_module_t orcm_sensor_ipmi_module;


END_C_DECLS

#endif
