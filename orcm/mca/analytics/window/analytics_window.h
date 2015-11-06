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

#ifndef MCA_analytics_window_EXPORT_H
#define MCA_analytics_window_EXPORT_H

#include "orcm_config.h"

#include "orcm/mca/analytics/analytics.h"

BEGIN_C_DECLS

typedef struct {
    opal_object_t super;
    int win_size;
    char *win_unit;
    uint64_t win_left;
    uint64_t win_right;
    uint64_t num_sample_recv;
    double sum_min_max;
    double sum_square;
    char *compute_type;
    char *win_type;
} win_statistics_t;
OBJ_CLASS_DECLARATION(win_statistics_t);

/*
 * Local Component structures
 */

ORCM_MODULE_DECLSPEC extern orcm_analytics_base_component_t mca_analytics_window_component;

typedef struct {
    orcm_analytics_base_module_t api;
} mca_analytics_window_module_t;
ORCM_DECLSPEC extern mca_analytics_window_module_t orcm_analytics_window_module;

END_C_DECLS

#endif /* MCA_analytics_threshold_EXPORT_H */
