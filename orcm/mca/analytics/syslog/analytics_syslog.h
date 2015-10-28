/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved.
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

#ifndef MCA_analytics_syslog_EXPORT_H
#define MCA_analytics_syslog_EXPORT_H

#include "orcm_config.h"

#include "orcm/mca/analytics/analytics.h"

BEGIN_C_DECLS

/*
 * Local Component structures
 */

ORCM_MODULE_DECLSPEC extern orcm_analytics_base_component_t mca_analytics_syslog_component;

/*Structure to store data from OFLOW*/
typedef struct {
    char* msg_regex_label;
    char* severity_label;
    char* facility_label;
    char* msg_regex;
    char* severity;
    char* facility;
} syslog_workflow_value_t;

typedef struct {
    int severity;
    int facility;
    char* date;
    char* message;
} syslog_value_t;

typedef struct {
    orcm_analytics_base_module_t api;
} mca_analytics_syslog_module_t;
ORCM_DECLSPEC extern mca_analytics_syslog_module_t orcm_analytics_syslog_module;

END_C_DECLS

#endif /* MCA_analytics_threshold_EXPORT_H */
