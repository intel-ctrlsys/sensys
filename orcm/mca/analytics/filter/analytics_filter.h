/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
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
 */

#ifndef MCA_analytics_filter_EXPORT_H
#define MCA_analytics_filter_EXPORT_H

#include "orcm_config.h"

#include "orcm/mca/analytics/analytics.h"

BEGIN_C_DECLS

/*
 * Local Component structures
 */

ORCM_MODULE_DECLSPEC extern orcm_analytics_base_component_t mca_analytics_filter_component;

/*Structure to store data from OFLOW*/
typedef struct {
	char* nodeid_label;
	char* sensor_label;
	char* coreid_label;
	char** nodeid;
	char** sensorname;
	char** coreid;
} filter_workflow_value;

typedef struct {
    orcm_analytics_base_module_t api;
} mca_analytics_filter_module_t;
ORCM_DECLSPEC extern mca_analytics_filter_module_t orcm_analytics_filter_module;

END_C_DECLS

#endif /* MCA_analytics_threshold_EXPORT_H */
