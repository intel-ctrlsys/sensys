/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_ATTRS_H
#define ORCM_ATTRS_H

#include "orcm_config.h"
#include "orte/types.h"
#include "orte/util/attr.h"

/* define the mininum value of the ORCM keys just in
 * case someone someday puts a layer underneath us */
#define ORCM_ATTR_KEY_BASE        ORTE_ATTR_KEY_MAX


/* define a max value for ORCM keys */
#define ORCM_ATTR_KEY_MAX         ORCM_ATTR_KEY_BASE+1000

/* 
 * Power Management Attributes
 */
#define ORCM_PWRMGMT_START_KEY   ORCM_ATTR_KEY_MAX

#define ORCM_PWRMGMT_POWER_MODE_KEY               (ORCM_PWRMGMT_START_KEY + 1) // orcm_pwrmgmt_mode - enum power mode
#define ORCM_PWRMGMT_POWER_BUDGET_KEY             (ORCM_PWRMGMT_START_KEY + 2) // int32_t - power cap in watts 
#define ORCM_PWRMGMT_POWER_WINDOW_KEY             (ORCM_PWRMGMT_START_KEY + 3) // int32_t - time window in msec to calculate energy over
#define ORCM_PWRMGMT_CAP_OVERAGE_LIMIT_KEY        (ORCM_PWRMGMT_START_KEY + 4) // int32_t - power in watts that we can exceed the power cap
#define ORCM_PWRMGMT_CAP_UNDERAGE_LIMIT_KEY       (ORCM_PWRMGMT_START_KEY + 5) // int32_t - power in watts that we can go under the power cap
#define ORCM_PWRMGMT_CAP_OVERAGE_TIME_LIMIT_KEY   (ORCM_PWRMGMT_START_KEY + 6) // int32_t - time in ms that we can exceed the power cap
#define ORCM_PWRMGMT_CAP_UNDERAGE_TIME_LIMIT_KEY  (ORCM_PWRMGMT_START_KEY + 7) // int32_t - time in ms that we can go under the power cap
#define ORCM_PWRMGMT_SUPPORTED_MODES_KEY          (ORCM_PWRMGMT_START_KEY + 8) // opal_list (int) - an array of possible supported power modes
#define ORCM_PWRMGMT_MANUAL_FREQUENCY_KEY         (ORCM_PWRMGMT_START_KEY + 9)  // float - requested target for setting a manual frequency
#define ORCM_PWRMGMT_SELECTED_COMPONENT_KEY       (ORCM_PWRMGMT_START_KEY + 10) // string - The current selected component
#define ORCM_PWRMGMT_FREQ_STRICT_KEY              (ORCM_PWRMGMT_START_KEY + 11) // bool - Do we have to set the frequency to the exact requested frequency and fail otherwise?

/* define a max value for pwrmgmt keys */
#define ORCM_PWRMGMT_KEY_MAX                      (ORCM_PWRMGMT_START_KEY + 50)

/* provide a handler for printing ORCM-level attr keys */
ORCM_DECLSPEC char* orcm_attr_key_print(orte_attribute_key_t key);

#endif
