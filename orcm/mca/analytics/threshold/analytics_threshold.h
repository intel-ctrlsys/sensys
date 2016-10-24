/*
 * Copyright (c) 2014      Intel Corporation. All rights reserved.
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
 */

#ifndef MCA_analytics_threshold_EXPORT_H
#define MCA_analytics_threshold_EXPORT_H

#include "orcm_config.h"

#include "orcm/mca/analytics/analytics.h"
#include "orte/mca/notifier/base/base.h"

BEGIN_C_DECLS

/*
 * Local Component structures
 */

typedef struct
{
    opal_object_t super;
    double hi;
    int hi_sev;
    char* hi_action;
    double low;
    int low_sev;
    char* low_action;
} orcm_mca_analytics_threshold_policy_t;

OBJ_CLASS_DECLARATION(orcm_mca_analytics_threshold_policy_t);

ORCM_MODULE_DECLSPEC extern orcm_analytics_base_component_t mca_analytics_threshold_component;

typedef struct {
    orcm_analytics_base_module_t api;
} mca_analytics_threshold_module_t;
ORCM_DECLSPEC extern mca_analytics_threshold_module_t orcm_analytics_threshold_module;

END_C_DECLS

#endif /* MCA_analytics_threshold_EXPORT_H */
