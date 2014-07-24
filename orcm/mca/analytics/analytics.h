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

/* Maybe following goes in activate_analytics_workflow_step() */
/* If tap_output in attributes, send data
 * If opal_list_get_next(steps) == opal_list_get_end(steps)
 *  OBJ_RELEASE(data)
 * else
 */
#define ORCM_ACTIVATE_WORKFLOW_STEP(a, b)                                      \
    do {                                                                       \
        opal_output_verbose(1, orcm_analytics_base_framework.framework_output, \
                            "%s ACTIVATE WORKFLOW %d STATE %s AT %s:%d",       \
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),                \
                            (a)->workflow_id, orcm_analytics_step_to_str((b)), \
                            __FILE__, __LINE__);                               \
        orcm_analytics.activate_analytics_workflow_step((a), (b));             \
    } while(0);

struct orcm_analytics_base_module_t;

/* Module functions */

/* initialize the module */
typedef int (*orcm_analytics_base_module_init_fn_t)(struct orcm_analytics_base_module_t *mod);

/* finalize the selected module */
typedef void (*orcm_analytics_base_module_finalize_fn_t)(struct orcm_analytics_base_module_t *mod);

/*
 * Ver 1.0
 */
typedef struct {
    orcm_analytics_base_module_init_fn_t        init;
    orcm_analytics_base_module_finalize_fn_t    finalize;
} orcm_analytics_base_module_t;

/*
 * the component data structure
 */
/* function to determine if this component is available for use.
 * Note that we do not use the standard component open
 * function as we do not want/need return of a module.
 */
typedef bool (*mca_analytics_base_component_avail_fn_t)(void);

/* create and return an analytics module */
typedef struct orcm_analytics_base_module_t* (*mca_analytics_base_component_create_hdl_fn_t)(void);

/* provide a chance for the component to finalize */
typedef void (*mca_analytics_base_component_finalize_fn_t)(void);

typedef struct {
    mca_base_component_t                         base_version;
    mca_base_component_data_t                    base_data;
    int                                          priority;
    mca_analytics_base_component_avail_fn_t      available;
    mca_analytics_base_component_create_hdl_fn_t create_handle;
    mca_analytics_base_component_finalize_fn_t   finalize;
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
