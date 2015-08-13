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

#include "orcm/mca/mca.h"

#include "orcm/mca/analytics/analytics_types.h"
#include "opal/class/opal_hash_table.h"

BEGIN_C_DECLS

/* Module functions */

/* initialize the module */
typedef int (*orcm_analytics_base_module_init_fn_t)(struct orcm_analytics_base_module_t *mod);

/* finalize the selected module */
typedef void (*orcm_analytics_base_module_finalize_fn_t)(struct orcm_analytics_base_module_t *mod);

/* do the real work in the selected module */
typedef void (*orcm_analytics_base_module_analyze_fn_t)(int fd, short args, void* cb);


typedef struct {
    orcm_analytics_base_module_init_fn_t        init;
    orcm_analytics_base_module_finalize_fn_t    finalize;
    orcm_analytics_base_module_analyze_fn_t     analyze;
    opal_hash_table_t*                          orcm_mca_analytics_hash_table;
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


typedef int (*orcm_analytics_API_module_array_create_fn_t) (opal_value_array_t **analytics_sample_array,
                                                            int ncores);
typedef int (*orcm_analytics_API_module_array_append_fn_t) (opal_value_array_t *analytics_sample_array,
                                                            int index, char *plugin_name,
                                                            char *host_name, orcm_metric_value_t *sample);
typedef void (*orcm_analytics_API_module_array_cleanup_fn_t) (opal_value_array_t *analytics_sample_array);
typedef void (*orcm_analytics_API_module_send_data_fn_t)(opal_value_array_t *data);


typedef struct {
    orcm_analytics_API_module_array_create_fn_t      array_create;
    orcm_analytics_API_module_array_append_fn_t      array_append;
    orcm_analytics_API_module_array_cleanup_fn_t     array_cleanup;
    orcm_analytics_API_module_send_data_fn_t         array_send;
} orcm_analytics_API_module_t;

/*
 * Macro for use in components that are of type analytics v1.0.0
 */
#define ORCM_ANALYTICS_BASE_VERSION_1_0_0 \
    ORCM_MCA_BASE_VERSION_2_1_0("analytics", 1, 0, 0)

/* Global structure for accessing name server functions
 */
/* holds Global API function pointers */
ORCM_DECLSPEC extern orcm_analytics_API_module_t orcm_analytics;

END_C_DECLS

#endif
