/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCA_ANALYTICS_H
#define MCA_ANALYTICS_H

/*
 * includes
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/mca.h"

#include "orcm/mca/analytics/analytics_types.h"

BEGIN_C_DECLS

/* Module functions */

/* initialize the selected module */
typedef int (*orcm_analytics_base_module_init_fn_t)(void);

/* finalize the selected module */
typedef void (*orcm_analytics_base_module_finalize_fn_t)(void);

/*
 * Ver 1.0
 */
typedef struct {
    orcm_analytics_base_module_init_fn_t        init;
    orcm_analytics_base_module_finalize_fn_t    finalize;
} orcm_analytics_base_module_t;

/*
 * the standard component data structure
 */
typedef struct {
    mca_base_component_t base_version;
    mca_base_component_data_t base_data;
} orcm_analytics_base_component_t;

/* define an API module */
typedef void (*orcm_analytics_API_module_foo_fn_t)(void);

typedef struct {
    orcm_analytics_API_module_foo_fn_t foo;
} orcm_analytics_API_module_t;

/*
 * Macro for use in components that are of type analytics v1.0.0
 */
#define ORCM_ANALYTICS_BASE_VERSION_1_0_0 \
  /* analytics v1.0 is chained to MCA v2.0 */ \
  MCA_BASE_VERSION_2_0_0, \
  /* analytics v1.0 */ \
  "analytics", 1, 0, 0

/* Global structure for accessing name server functions
 */
/* holds API function pointers */
ORCM_DECLSPEC extern orcm_analytics_API_module_t orcm_analytics;

END_C_DECLS

#endif
