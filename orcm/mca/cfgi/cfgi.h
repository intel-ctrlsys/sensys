/*
 * Copyright (c) 2014-2015 Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCA_CFGI_H
#define MCA_CFGI_H

/*
 * includes
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/class/opal_list.h"

#include "orte/types.h"

#include "orcm/mca/mca.h"
#include "orcm/mca/cfgi/cfgi_types.h"

BEGIN_C_DECLS

/*
 * Component functions - all MUST be provided!
 */

/* initialize the selected module */
typedef int (*orcm_cfgi_base_module_init_fn_t)(void);

/* finalize the selected module */
typedef void (*orcm_cfgi_base_module_finalize_fn_t)(void);

/* read configuration */
typedef int (*orcm_cfgi_base_module_read_config_fn_t)(opal_list_t *config);

/* define a system from the configuration */
typedef int (*orcm_cfgi_base_module_define_sys_fn_t)(opal_list_t *config,
                                                     orcm_node_t **mynode,
                                                     orte_vpid_t *num_procs,
                                                     opal_buffer_t *buf);

/*
 * Ver 1.0
 */
struct orcm_cfgi_base_module_1_0_0_t {
    orcm_cfgi_base_module_init_fn_t            init;
    orcm_cfgi_base_module_finalize_fn_t        finalize;
    orcm_cfgi_base_module_read_config_fn_t     read_config;
    orcm_cfgi_base_module_define_sys_fn_t      define_system;
};

typedef struct orcm_cfgi_base_module_1_0_0_t orcm_cfgi_base_module_1_0_0_t;
typedef orcm_cfgi_base_module_1_0_0_t orcm_cfgi_base_module_t;

/* Public API */
struct orcm_cfgi_API_module_1_0_0_t {
    orcm_cfgi_base_module_read_config_fn_t     read_config;
    orcm_cfgi_base_module_define_sys_fn_t      define_system;
};
typedef struct orcm_cfgi_API_module_1_0_0_t orcm_cfgi_API_module_1_0_0_t;
typedef orcm_cfgi_API_module_1_0_0_t orcm_cfgi_API_module_t;

/*
 * the standard component data structure
 */
struct orcm_cfgi_base_component_1_0_0_t {
    mca_base_component_t base_version;
    mca_base_component_data_t base_data;
};
typedef struct orcm_cfgi_base_component_1_0_0_t orcm_cfgi_base_component_1_0_0_t;
typedef orcm_cfgi_base_component_1_0_0_t orcm_cfgi_base_component_t;


/*
 * Macro for use in components that are of type cfgi v1.0.0
 */
#define ORCM_CFGI_BASE_VERSION_1_0_0 \
    ORCM_MCA_BASE_VERSION_2_1_0("cfgi", 1, 0, 0)

/* Global structure for accessing name server functions
 */
ORCM_DECLSPEC extern orcm_cfgi_API_module_t orcm_cfgi;  /* holds public API function pointers */

END_C_DECLS

#endif
