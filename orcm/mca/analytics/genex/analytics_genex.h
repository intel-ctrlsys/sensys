/*
 * Copyright (c) 2015-2016 Intel, Inc. All rights reserved.
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

#ifndef MCA_analytics_genex_EXPORT_H
#define MCA_analytics_genex_EXPORT_H

#include "orcm_config.h"
#include "orcm/mca/analytics/analytics.h"

BEGIN_C_DECLS

/*
 * Local Component structures
 */

ORCM_MODULE_DECLSPEC extern orcm_analytics_base_component_t mca_analytics_genex_component;

/*Structure to store data from OFLOW*/
typedef struct {
    char* msg_regex_label;
    char* severity_label;
    char* notifier_label;
    char* db_label;
    char* msg_regex;
    char* severity;
    char* db;
    char* notifier;
} genex_workflow_value_t;

typedef struct {
    const char* severity;
    const char* message;
} genex_value_t;

typedef struct {
    orcm_analytics_base_module_t api;
} mca_analytics_genex_module_t;
ORCM_DECLSPEC extern mca_analytics_genex_module_t orcm_analytics_genex_module;

END_C_DECLS

#endif /* MCA_analytics_genex_EXPORT_H */
