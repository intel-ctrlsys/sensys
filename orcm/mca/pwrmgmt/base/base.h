/*
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2013      Los Alamos National Security, LLC.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_PWRMGMT_BASE_H
#define MCA_PWRMGMT_BASE_H

/*
 * includes
 */
#include "orcm_config.h"

#include "opal/class/opal_list.h"
#include "opal/mca/base/base.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */

#include "opal/class/opal_pointer_array.h"
#include "opal/mca/event/event.h"
#include "opal/threads/threads.h"

#include "orte/runtime/orte_globals.h"

#include "orcm/mca/pwrmgmt/pwrmgmt.h"

#define ORCM_PWRMGMT_BASE_MAX_FREQ_CMD  (uint8_t) 51    
#define ORCM_PWRMGMT_BASE_SET_IDLE_CMD  (uint8_t) 52
#define ORCM_PWRMGMT_BASE_RESET_DEFAULTS_CMD  (uint8_t) 53

BEGIN_C_DECLS

/*
 * MCA Framework
 */
/* Some utility functions */

/**
 * set the cpus to the max supported frequency
 *
 * Sets the cpus on the specified nodes to the maximum supported frequency
 *
 * @param[in] daemons - array of proc_name_t structures of the target nodes
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORTE_ERR_NOT_SUPPORTED The operation is not supported.
 *                      this is likely due to performance governors 
 *                      being disabled or not having sufficient
 *                      permissions to complete the operation
 * @retval ORTE_ERROR   An unspecified error occurred
 */
typedef int (*orcm_pwrmgmt_base_module_set_max_freq_fn_t)(opal_pointer_array_t *daemons);

/**
 * set the cpus to idle frequency
 *
 * Sets the cpus on the specified nodes to the minimum supported frequency
 *
 * @param[in] daemons - array of proc_name_t structures of the target nodes
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORTE_ERR_NOT_SUPPORTED The operation is not supported.
 *                      this is likely due to performance governors 
 *                      being disabled or not having sufficient
 *                      permissions to complete the operation
 *
 * @retval ORTE_ERROR   An unspecified error occurred
 */
typedef int (*orcm_pwrmgmt_base_module_set_to_idle_fn_t)(opal_pointer_array_t *daemons);

/**
 * reset the frequencies to default
 *
 * Sets the cpus on the specified nodes to the default (initial) frequency settings
 *
 * @param[in] daemons - array of proc_name_t structures of the target nodes
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORTE_ERR_NOT_SUPPORTED The operation is not supported.
 *                      this is likely due to performance governors 
 *                      being disabled or not having sufficient
 *                      permissions to complete the operation
 *
 * @retval ORTE_ERROR   An unspecified error occurred
 */
typedef int (*orcm_pwrmgmt_base_module_reset_freq_defaults_fn_t)(opal_pointer_array_t *daemons);

typedef struct {
    opal_pointer_array_t modules;
    orcm_pwrmgmt_base_module_set_max_freq_fn_t         set_max_freq;
    orcm_pwrmgmt_base_module_set_to_idle_fn_t          set_to_idle;
    orcm_pwrmgmt_base_module_reset_freq_defaults_fn_t  reset_freq_defaults;
} orcm_pwrmgmt_base_t;

typedef struct {
    opal_object_t super;
    int priority;
    orcm_pwrmgmt_base_component_t *component;
    orcm_pwrmgmt_base_API_module_t *module;
} orcm_pwrmgmt_active_module_t;
OBJ_CLASS_DECLARATION(orcm_pwrmgmt_active_module_t);

ORCM_DECLSPEC extern mca_base_framework_t orcm_pwrmgmt_base_framework;
ORCM_DECLSPEC extern orcm_pwrmgmt_base_t orcm_pwrmgmt_base;
ORCM_DECLSPEC extern orcm_pwrmgmt_base_API_module_t orcm_pwrmgmt_stubs;
ORCM_DECLSPEC int orcm_pwrmgmt_base_init(void);
ORCM_DECLSPEC void orcm_pwrmgmt_base_finalize(void);
ORCM_DECLSPEC int orcm_pwrmgmt_base_component_select(orcm_session_id_t session, opal_list_t* attr);
ORCM_DECLSPEC int orcm_pwrmgmt_base_alloc_notify(orcm_alloc_t* alloc);
ORCM_DECLSPEC void orcm_pwrmgmt_base_dealloc_notify(orcm_alloc_t* alloc);
ORCM_DECLSPEC int orcm_pwrmgmt_base_set_attributes(orcm_session_id_t session, opal_list_t* attr);
ORCM_DECLSPEC int orcm_pwrmgmt_base_reset_attributes(orcm_session_id_t session, opal_list_t* attr);
ORCM_DECLSPEC int orcm_pwrmgmt_base_get_attributes(orcm_session_id_t session, opal_list_t* attr);
ORCM_DECLSPEC int orcm_pwrmgmt_base_get_current_power(orcm_session_id_t session, double* power);
ORCM_DECLSPEC int orcm_pwrmgmt_base_select(void);

END_C_DECLS
#endif
