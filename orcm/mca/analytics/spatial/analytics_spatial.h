/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
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

#ifndef MCA_analytics_spatial_EXPORT_H
#define MCA_analytics_spatial_EXPORT_H

#include "orcm/include/orcm_config.h"

#include "orcm/mca/analytics/analytics.h"

BEGIN_C_DECLS

typedef struct {
    opal_object_t super;
    int interval;
    int size;
    char** nodelist;
    int compute_type;
    struct timeval* last_round_time;
    opal_hash_table_t* buckets;
    double result;
    uint64_t num_data_point;
} spatial_statistics_t;
OBJ_CLASS_DECLARATION(spatial_statistics_t);

/*
 * Local Component structures
 */

ORCM_MODULE_DECLSPEC extern orcm_analytics_base_component_t mca_analytics_spatial_component;

typedef struct {
    orcm_analytics_base_module_t api;
} mca_analytics_spatial_module_t;
ORCM_DECLSPEC extern mca_analytics_spatial_module_t orcm_analytics_spatial_module;

END_C_DECLS

#endif /* MCA_analytics_threshold_EXPORT_H */
