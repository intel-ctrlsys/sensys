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
 * When the ORCM Scheduler gets an allocation command, it calls alloc_notify in the base framework and
 * passes in the job allocation structure. The base framework then polls all the existing components to see
 * which will handle the requested power management mode. If more than one component can handle the requested 
 * mode, it will choose one based on the component's priority. The selected component's alloc_notify function
 * is called, allowing that component to set any attributes it wishes to be send to the back end nodes.
 *
 * When the allocated orcmds receive the allocation command from the scheduler, they call the base pwrmgmt 
 * framework alloc_notify. The selection of the correct component matches what happened at the scheduler with 
 * the exception of any extra attributes set by the selected component at the scheduler level. If a node cannot 
 * fufill the request for some reason an error is sent back to the scheduler and the session is terminated with 
 * an error. Once the correct component is selected, the orcm_pwrmgmt function pointers are redirected to the 
 * selected component.
 *
 * A "head" power managment node is specified by the alloc_t structure. This is either the "head" aggregator node,
 * or if no aggregators exist in the allocation, the "head" daemon is chosen. All successive power management messages
 * will be sent to this node. It is up to the current component on that node to dissemenate any new or updated settings
 * to the rest of the allocation. 
 *
 * During the session lifetime, attributes such as the power cap or power mode for the session can be changed by 
 * calling the set_attributes function with the requested attributes and values. To query current power management 
 * settings, get_attributes can be called with a list of the attributes to query for. The component will then fill 
 * in the requested values. All defined attributes are listed below.
 *
 * At the end of the session, or when the power mode causes the selected component to change, the currently selected 
 * component's dealloc_notify function is called. The component is responsible for all deinitializaion and cleanup it 
 * needs to do to put the system back into the state it was before the component was selected. When this is complete 
 * the orcm_pwrmgmt function pointers ar pointed back to the base framework stubs. 
 */

BEGIN_C_DECLS

/* 
 * Power Management Attributes
 * these are defined in orcm/util/attr.h add new attributes there 
 * ORCM_PWRMGMT_POWER_MODE_KEY               // orcm_pwrmgmt_mode - enum power mode (defined in pwrmgmt_types.h)
 * ORCM_PWRMGMT_POWER_BUDGET_KEY             // uint32_t - power cap in watts for the session 
 * ORCM_PWRMGMT_POWER_WINDOW_KEY             // uint32_t - time window in msec to calculate energy over
 * ORCM_PWRMGMT_CAP_OVERAGE_LIMIT_KEY        // uint32_t - power in watts that we can exceed the power cap
 * ORCM_PWRMGMT_CAP_UNDERAGE_LIMIT_KEY       // uint32_t - power in watts that we can go under the power cap
 * ORCM_PWRMGMT_CAP_OVERAGE_TIME_LIMIT_KEY   // uint32_t - time in ms that we can exceed the power cap
 * ORCM_PWRMGMT_CAP_UNDERAGE_TIME_LIMIT_KEY  // uint32_t - time in ms that we can go under the power cap
 * ORCM_PWRMGMT_SUPPORTED_MODES_KEY          // opal_list (int) - an array of the possible supported power modes
 * ORCM_PWRMGMT_SELECTED_COMPONENT_KEY       // string - string name of the selected component
 * ORCM_PWRMGMT_MANUAL_FREQUENCY_KEY         // float - requested target for setting a manual frequency
 * ORCM_PWRMGMT_FREQ_STRICT_KEY              // bool - Do we have to set the frequency to the exact requested frequency and fail otherwise?
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
 * select the correct module for a session allocation
 *
 * This is called at the beginning of a session and it chooses the correct
 * module based on the session allocation information for power management.
 * Every module is called and must decide if it can support the requested
 * power management mode. 
 *
 * @param[in] alloc - The allocation structure sent by the scheduler
 *
 */
typedef int (*orcm_pwrmgmt_base_module_component_select_fn_t)(orcm_session_id_t session, opal_list_t* attr);

/**
 * notify the current module of a session allocation
 *
 * This is called after the correct component has been selected. the component
 * can then do any work it needs to do to start running globally across the
 * session allocation. The alloc structure holds the job information. All
 * power management attributes are in the constraints section of the alloc 
 * structure 
 *
 * @param[in] alloc - The allocation structure sent by the scheduler
 *
 */
typedef int (*orcm_pwrmgmt_base_module_alloc_notify_fn_t)(orcm_alloc_t* alloc);

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
 * Reet power management attributes values (reset the current power management state)
 *
 * Give a list of attributes and values to the current component so that it can
 * remove any attributes it may have added during alloc_notify. The goal is to
 * reset the attribute list to its initial state
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
typedef int (*orcm_pwrmgmt_base_module_reset_attributes_fn_t)(orcm_session_id_t session, opal_list_t* attr);
  
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
  
/* API module */
/*
 * Ver 1.0
 */

/* provide a public API version */
typedef struct orcm_pwrmgmt_base_API_module_1_0_0_t {
    orcm_pwrmgmt_base_module_init_fn_t               init;
    orcm_pwrmgmt_base_module_finalize_fn_t           finalize;
    orcm_pwrmgmt_base_module_component_select_fn_t   component_select;
    orcm_pwrmgmt_base_module_alloc_notify_fn_t       alloc_notify;
    orcm_pwrmgmt_base_module_dealloc_notify_fn_t     dealloc_notify;
    orcm_pwrmgmt_base_module_set_attributes_fn_t     set_attributes;
    orcm_pwrmgmt_base_module_reset_attributes_fn_t   reset_attributes;
    orcm_pwrmgmt_base_module_get_attributes_fn_t     get_attributes;
} orcm_pwrmgmt_base_API_module_1_0_0_t;

typedef orcm_pwrmgmt_base_API_module_1_0_0_t orcm_pwrmgmt_base_API_module_t;

/*
 * the standard component data structure
 */
struct orcm_pwrmgmt_base_component_1_0_0_t {
    mca_base_component_t       base_version;
    mca_base_component_data_t  base_data;
    char*                      name;
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

ORCM_DECLSPEC const char *orcm_pwrmgmt_get_mode_string(int mode);
ORCM_DECLSPEC int orcm_pwrmgmt_get_mode_val(const char* mode);

END_C_DECLS

#endif /* MCA_PWRMGMT_H */
