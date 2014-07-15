/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/**
 * @file
 *
 * Resource Allocation (ORCM)
 */
#ifndef ORTE_RAS_ORCM_H
#define ORTE_RAS_ORCM_H

#include "orte_config.h"
#include "orte/mca/ras/ras.h"
#include "orte/mca/ras/base/base.h"

BEGIN_C_DECLS

typedef struct {
    orte_ras_base_component_t super;
} orte_ras_orcm_component_t;
ORTE_DECLSPEC extern orte_ras_orcm_component_t mca_ras_orcm_component;

ORTE_DECLSPEC extern orte_ras_base_module_t orte_ras_orcm_module;

END_C_DECLS

#endif
