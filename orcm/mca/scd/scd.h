/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCA_SCD_H
#define MCA_SCD_H

/*
 * includes
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/mca.h"
#include "opal/mca/event/event.h"
#include "opal/util/output.h"

#include "orte/runtime/orte_globals.h"
#include "orte/util/name_fns.h"
#include "orte/util/proc_info.h"

#include "orcm/mca/scd/scd_types.h"

BEGIN_C_DECLS

#define ORCM_ACTIVATE_SCD_STATE(a, b)                                  \
    do {                                                                 \
        opal_output_verbose(1, orcm_scd_base_framework.framework_output, \
                            "%s ACTIVATE SESSION %d STATE %s AT %s:%d",  \
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),          \
                            (a)->id, orcm_scd_session_state_to_str((b)), \
                            __FILE__, __LINE__);                         \
        orcm_scd.activate_scd_session_state((a), (b));                 \
    } while(0);

/*
 * Component functions - all MUST be provided!
 */

/* initialize the selected module */
typedef int (*orcm_scd_base_module_init_fn_t)(void);

/* finalize the selected module */
typedef void (*orcm_scd_base_module_finalize_fn_t)(void);

/*
 * Ver 1.0
 */
typedef struct {
    orcm_scd_base_module_init_fn_t        init;
    orcm_scd_base_module_finalize_fn_t    finalize;
} orcm_scd_base_module_t;

/*
 * the standard component data structure
 */
typedef struct {
    mca_base_component_t base_version;
    mca_base_component_data_t base_data;
} orcm_scd_base_component_t;

/* define an API module */
typedef void (*orcm_scd_API_module_activate_scd_session_state_fn_t)(orcm_session_t *s,
                                                                    orcm_scd_session_state_t state);
typedef struct {
    orcm_scd_API_module_activate_scd_session_state_fn_t activate_scd_session_state;
} orcm_scd_API_module_t;

/*
 * Macro for use in components that are of type scd v1.0.0
 */
#define ORCM_SCD_BASE_VERSION_1_0_0 \
  /* scd v1.0 is chained to MCA v2.0 */ \
  MCA_BASE_VERSION_2_0_0, \
  /* scd v1.0 */ \
  "scd", 1, 0, 0

/* Global structure for accessing name server functions
 */
/* holds API function pointers */
ORCM_DECLSPEC extern orcm_scd_API_module_t orcm_scd;

END_C_DECLS

#endif
