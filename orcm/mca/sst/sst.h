/*
 * Copyright (c) 2014-2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCA_SST_H
#define MCA_SST_H

/*
 * includes
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "orcm/mca/mca.h"



BEGIN_C_DECLS

/*
 * Component functions - all MUST be provided!
 */

/* initialize the selected module */
typedef int (*orcm_sst_base_module_init_fn_t)(void);

/* finalize the selected module */
typedef void (*orcm_sst_base_module_finalize_fn_t)(void);

/*
 * Ver 1.0
 */
struct orcm_sst_base_module_1_0_0_t {
    orcm_sst_base_module_init_fn_t                  init;
    orcm_sst_base_module_finalize_fn_t              finalize;
};

typedef struct orcm_sst_base_module_1_0_0_t orcm_sst_base_module_1_0_0_t;
typedef orcm_sst_base_module_1_0_0_t orcm_sst_base_module_t;

/*
 * the standard component data structure
 */
struct orcm_sst_base_component_1_0_0_t {
    mca_base_component_t base_version;
    mca_base_component_data_t base_data;
};
typedef struct orcm_sst_base_component_1_0_0_t orcm_sst_base_component_1_0_0_t;
typedef orcm_sst_base_component_1_0_0_t orcm_sst_base_component_t;



/*
 * Macro for use in components that are of type sst v1.0.0
 */
#define ORCM_SST_BASE_VERSION_1_0_0 \
  /* sst v1.0 is chained to MCA v2.0 */ \
    ORCM_MCA_BASE_VERSION_2_1_0("sst", 1, 0, 0)


/* Global structure for accessing name server functions
 */
ORCM_DECLSPEC extern orcm_sst_base_module_t orcm_sst;  /* holds selected module's function pointers */

END_C_DECLS

#endif
