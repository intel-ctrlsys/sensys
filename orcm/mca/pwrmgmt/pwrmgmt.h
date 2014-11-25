/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 * @file:
 *
 */

#ifndef MCA_PWRMGMT_H
#define MCA_PWRMGMT_H

/*
 * includes
 */

#include "opal/mca/mca.h"

#include "orte/types.h"
#include "orte/util/attr.h"


#include "orcm_config.h"
#include "orcm/types.h"
#include "orcm/util/attr.h"
#include "orcm/mca/scd/scd_types.h"
#include "orcm/mca/pwrmgmt/pwrmgmt_types.h"

BEGIN_C_DECLS

/* 
 * Power Management Attributes
 * these are defined in orcm/util/attr.h add new attributes there 
 * ORCM_PWRMGMT_POWER_MODE_KEY               // orcm_pwrmgmt_mode - enum power mode
 * ORCM_PWRMGMT_POWER_BUDGET_KEY             // double - power cap in watts 
 * ORCM_PWRMGMT_POWER_WINDOW_KEY             // uint64_t - time window in msec to calculate energy over
 * ORCM_PWRMGMT_CAP_OVERAGE_LIMIT_KEY        // double - power in watts that we can exceed the power cap
 * ORCM_PWRMGMT_CAP_UNDERAGE_LIMIT_KEY       // double - power in watts that we can go under the power cap
 * ORCM_PWRMGMT_CAP_OVERAGE_TIME_LIMIT_KEY   // uint64_t - time in ms that we can exceed the power cap
 * ORCM_PWRMGMT_CAP_UNDERAGE_TIME_LIMIT_KEY  // uint64_t - time in ms that we can go under the power cap
 * ORCM_PWRMGMT_SUPPORTED_MODES_KEY          // opal_ptr (int) - pointer to an array of supported power modes by current component
 */

/*
 * Component functions - all MUST be provided!
 */

/**
 * initialize power management component
 *
 * What ever needs to be done to initialize the selected module
 */
typedef int (*orcm_pwrmgmt_base_module_init_fn_t)(void);
    
/**
 * finalize the power management component
 *
 * What ever needs to be done to finalize the selected module
 */
typedef void (*orcm_pwrmgmt_base_module_finalize_fn_t)(void);

/**
 * start power management system
 *
 * What ever needs to be done to start up the selected module
 *
 * @param[in] session - The session ID
 */
typedef void (*orcm_pwrmgmt_base_module_start_fn_t)(orcm_session_id_t session);

/**
 * stop power management system
 *
 * What ever needs to be done to stop the selected module
 *
 * @param[in] session - The session ID
 */
typedef void (*orcm_pwrmgmt_base_module_stop_fn_t)(orcm_session_id_t session);

/**
 * notify the current module of an allocation request
 *
 * @param[in] alloc - The allocation structure sent by the scheduler
 *
 */
typedef void (*orcm_pwrmgmt_base_module_alloc_notify_fn_t)(orcm_alloc_t* alloc);

/**
 * Set power management attributes values
 *
 * @param[in] session - session id for the allocation
 * @param[in] attr - opal list of attributes and their respective values
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORTE_ERR_NOT_SUPPORTED The requested attribute(s) is not supported
 *                      this is likely due to using a component that 
 *                      does not support certain attributes
 * @retval ORTE_ERROR   An unspecified error occurred
 */
typedef int (*orcm_pwrmgmt_base_module_set_attributes_fn_t)(orcm_session_id_t session, opal_list_t* attr);
  
/**
 * Get power management attribute values
 *
 * @param[in] session - session id for the allocation
 * @param[in] attr - opal list of attributes to fill in their value(s)
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORTE_ERR_NOT_SUPPORTED The requested attribute(s) is not supported
 *                      this is likely due to using a component that 
 *                      does not support certain attributes
 * @retval ORTE_ERROR   An unspecified error occurred
 */
typedef int (*orcm_pwrmgmt_base_module_get_attributes_fn_t)(orcm_session_id_t session, opal_list_t* attr);
  
/**
 * Get the current power usage in watts
 *
 * Returns the current power usage in watts measured over the current power window
 *
 * @param[in] session - session id for the allocation
 * @param[out] power - double containing the current power usage by the allocation
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORTE_ERROR   An unspecified error occurred
 */
typedef int (*orcm_pwrmgmt_base_module_get_current_power_fn_t)(orcm_session_id_t session, double* power);

/* API module */
/*
 * Ver 1.0
 */

/* provide a public API version */
typedef struct orcm_pwrmgmt_base_API_module_1_0_0_t {
    orcm_pwrmgmt_base_module_init_fn_t               init;
    orcm_pwrmgmt_base_module_finalize_fn_t           finalize;
    orcm_pwrmgmt_base_module_start_fn_t              start;
    orcm_pwrmgmt_base_module_stop_fn_t               stop;
    orcm_pwrmgmt_base_module_alloc_notify_fn_t       alloc_notify;
    orcm_pwrmgmt_base_module_set_attributes_fn_t     set_attributes;
    orcm_pwrmgmt_base_module_get_attributes_fn_t     get_attributes;
    orcm_pwrmgmt_base_module_get_current_power_fn_t  get_current_power;
} orcm_pwrmgmt_base_API_module_1_0_0_t;

typedef orcm_pwrmgmt_base_API_module_1_0_0_t orcm_pwrmgmt_base_API_module_t;

/*
 * the standard component data structure
 */
struct orcm_pwrmgmt_base_component_1_0_0_t {
    mca_base_component_t       base_version;
    mca_base_component_data_t  base_data;
    int                        num_modes;
    int                        modes;
};

typedef struct orcm_pwrmgmt_base_component_1_0_0_t orcm_pwrmgmt_base_component_1_0_0_t;
typedef orcm_pwrmgmt_base_component_1_0_0_t orcm_pwrmgmt_base_component_t;

/*
 * Macro for use in components that are of type pwrmgmt v1.0.0
 */
#define ORCM_PWRMGMT_BASE_VERSION_1_0_0 \
  /* pwrmgmt v1.0 is chained to MCA v2.0 */ \
  MCA_BASE_VERSION_2_0_0, \
  /* pwrmgmt v1.0 */ \
  "pwrmgmt", 1, 0, 0

/* Global structure for accessing pwrmgmt functions
 */
ORCM_DECLSPEC extern orcm_pwrmgmt_base_API_module_t orcm_pwrmgmt;  /* holds API function pointers */

END_C_DECLS

#endif /* MCA_PWRMGMT_H */
