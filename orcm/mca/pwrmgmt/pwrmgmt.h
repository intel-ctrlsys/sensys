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

/**
 * ORCM Power Management Framework
 *
 * The power management flow is as follows:
 *
 * The ORCM Scheduler (or the top level ORCMD if integrated with a third-party system) opens the base
 * pwrmgmt framework. When it gets an allocation command, it calls alloc_notify in the base framework and
 * passes in the job allocation structure. The base framework then polls all the existing components to see
 * which will handle the requested power management mode. If more than one component can handle the requested 
 * mode, it will chose one based on the components priority. The orcm_pwrmgmt function pointers are then pointed 
 * at the selected component and its alloc_notify is called.
 *
 * The component is then responsible for all initialization and startup it has to do to start running across
 * the whole session allocation. This can include calling alloc_notify on the backend daemons, setting up 
 * communications hierarchies, database handles, etc...
 *
 * During the session lifetime, the power management mode is constant and should not be changed. However, 
 * other attributes such as the power cap for the session can be changed by calling the set_attributes 
 * function with the requested attributes and values. To query current power management settings, get_attributes
 * can be called with a list of the attributes to query for. The component will then fill in the requested
 * values. All defined attributes are listed below.
 *
 * At the end of the session, the component's dealloc_notify function is called. The component is responsible
 * for all deinitializaion and cleanup it needs to do to put the system back into the state it was before
 * the component was selected. When this is complete the orcm_pwrmgmt function pointers ar pointed back
 * to the base framework stubs. 
 */

BEGIN_C_DECLS

/* 
 * Power Management Attributes
 * these are defined in orcm/util/attr.h add new attributes there 
 * ORCM_PWRMGMT_POWER_MODE_KEY               // orcm_pwrmgmt_mode - enum power mode (defined in pwrmgmt_types.h)
 * ORCM_PWRMGMT_POWER_BUDGET_KEY             // double - power cap in watts for the session 
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
 * notify the current module of a session allocation
 *
 * This is called at the beginning of a session and it chooses the correct
 * module based on the session allocation information for power management.
 * It then calls the same function for the selected component. the component
 * can then do any work it needs to do to start running globally across the
 * session allocation. The alloc structure holds the job information. All
 * power management attributes are in the constraints section of the alloc 
 * structure 
 *
 * @param[in] alloc - The allocation structure sent by the scheduler
 *
 */
typedef void (*orcm_pwrmgmt_base_module_alloc_notify_fn_t)(orcm_alloc_t* alloc);

/**
 * notify the current module at the end of a session
 *
 * This is called at the end of a session to allow the currrent component
 * to do any cleanup/reset that it needs to do before being de-selected
 *
 * @param[in] alloc - The allocation structure sent by the scheduler
 */
typedef void (*orcm_pwrmgmt_base_module_dealloc_notify_fn_t)(orcm_alloc_t* alloc);

/**
 * Set power management attributes values (set the current power management state)
 *
 * Give a list of attributes and values to the current component so that it can
 * set the current power management state to match. The list of attributes can be
 * a single attribute or multiple. The defined attributes are listed at the top of 
 * this file.
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
 * Get power management attribute values (get the current power management state)
 *
 * Give a list of attributes to the current component so that it may fill in
 * the values according to the current power management state. The defined
 * attributes are listed at the top of this file.
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
    orcm_pwrmgmt_base_module_alloc_notify_fn_t       alloc_notify;
    orcm_pwrmgmt_base_module_dealloc_notify_fn_t     dealloc_notify;
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
