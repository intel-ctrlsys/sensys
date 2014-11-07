/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCA_DIAG_H
#define MCA_DIAG_H

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

#define DIAG_MEM_PATTERN_TST      (1 << 0)
#define DIAG_MEM_STRESS_TST       (1 << 1)
#define DIAG_MEM_ERR              (1 << 2)
#define DIAG_MEM_NOTRUN           (1 << 31)

#define DIAG_CPU_FP_TST           (1 << 0)
#define DIAG_CPU_PRIME_TST        (1 << 1)
#define DIAG_CPU_STRESS_TST       (1 << 2)
#define DIAG_CPU_NOTRUN           (1 << 31)

#define DIAG_ETH_REG_TST          (1 << 0)
#define DIAG_ETH_EEPROM_TST       (1 << 1)
#define DIAG_ETH_INT_TST          (1 << 2)
#define DIAG_ETH_LOOPBACK_TST     (1 << 3)
#define DIAG_ETH_LINK_TST         (1 << 4)
#define DIAG_ETH_NOTRUN           (1 << 31)

BEGIN_C_DECLS

/*
 * Component functions - all MUST be provided!
 */

/* initialize the selected module */
typedef int (*orcm_diag_base_module_init_fn_t)(void);

/* finalize the selected module */
typedef void (*orcm_diag_base_module_finalize_fn_t)(void);

/* calibrate */
typedef void (*orcm_diag_base_module_calibrate_fn_t)(void);

/* diagnostics read known to be good values */
typedef int (*orcm_diag_base_module_diag_read_fn_t)(opal_list_t *config);

/* diagnostics check */
typedef int (*orcm_diag_base_module_diag_check_fn_t)(char *resource, opal_list_t *config);

/*
 * Ver 1.0
 */
typedef struct {
    orcm_diag_base_module_init_fn_t        init;
    orcm_diag_base_module_finalize_fn_t    finalize;
    orcm_diag_base_module_calibrate_fn_t   calibrate;
    orcm_diag_base_module_diag_read_fn_t   diag_read;
    orcm_diag_base_module_diag_check_fn_t  diag_check;
} orcm_diag_base_module_t;

/*
 * the standard component data structure
 */
typedef struct {
    mca_base_component_t base_version;
    mca_base_component_data_t base_data;
} orcm_diag_base_component_t;

/* define an API module */
typedef void (*orcm_diag_API_module_calibrate_fn_t)(void);
typedef int  (*orcm_diag_API_module_diag_read_fn_t)(opal_list_t *config);
typedef int  (*orcm_diag_API_module_diag_check_fn_t)(char *resource, opal_list_t *config);

typedef struct {
    orcm_diag_API_module_calibrate_fn_t  calibrate;
    orcm_diag_API_module_diag_read_fn_t  diag_read;
    orcm_diag_API_module_diag_check_fn_t diag_check;
} orcm_diag_API_module_t;

/*
 * Macro for use in components that are of type diag v1.0.0
 */
#define ORCM_DIAG_BASE_VERSION_1_0_0 \
  /* diag v1.0 is chained to MCA v2.0 */ \
  MCA_BASE_VERSION_2_0_0, \
  /* diag v1.0 */ \
  "diag", 1, 0, 0

/* Global structure for accessing name server functions
 */
ORCM_DECLSPEC extern orcm_diag_API_module_t orcm_diag;  /* holds API function pointers */

END_C_DECLS

#endif
