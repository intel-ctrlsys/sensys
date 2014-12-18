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
 * manual frequency pwrmgmt component
 */
#ifndef ORCM_PWRMGMT_MANUALFREQ_H
#define ORCM_PWRMGMT_MANUALFREQ_H

#include "orcm_config.h"

#include "orcm/mca/pwrmgmt/pwrmgmt.h"

BEGIN_C_DECLS

typedef struct {
    orcm_pwrmgmt_base_component_t super;
    bool test;
} orcm_pwrmgmt_manualfreq_component_t;

ORCM_MODULE_DECLSPEC extern orcm_pwrmgmt_manualfreq_component_t mca_pwrmgmt_manualfreq_component;
extern orcm_pwrmgmt_base_API_module_t orcm_pwrmgmt_manualfreq_module;


END_C_DECLS

#endif
