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
#define ORCM_PWRMGMT_POWER_BUDGET_KEY             (ORCM_PWRMGMT_START_KEY + 2) // double - power cap in watts 
#define ORCM_PWRMGMT_POWER_WINDOW_KEY             (ORCM_PWRMGMT_START_KEY + 3) // uint64_t - time window in msec to calculate energy over
#define ORCM_PWRMGMT_CAP_OVERAGE_LIMIT_KEY        (ORCM_PWRMGMT_START_KEY + 4) // double - power in watts that we can exceed the power cap
#define ORCM_PWRMGMT_CAP_UNDERAGE_LIMIT_KEY       (ORCM_PWRMGMT_START_KEY + 5) // double - power in watts that we can go under the power cap
#define ORCM_PWRMGMT_CAP_OVERAGE_TIME_LIMIT_KEY   (ORCM_PWRMGMT_START_KEY + 6) // uint64_t - time in ms that we can exceed the power cap
#define ORCM_PWRMGMT_CAP_UNDERAGE_TIME_LIMIT_KEY  (ORCM_PWRMGMT_START_KEY + 7) // uint64_t - time in ms that we can go under the power cap
#define ORCM_PWRMGMT_SUPPORTED_MODES_KEY          (ORCM_PWRMGMT_START_KEY + 8) // opal_ptr (int) - pointer to an array of supported power modes by current component

/* define a max value for pwrmgmt keys */
#define ORCM_PWRMGMT_KEY_MAX                      (ORCM_PWRMGMT_START_KEY + 50)

/* provide a handler for printing ORCM-level attr keys */
ORCM_DECLSPEC char* orcm_attr_key_print(orte_attribute_key_t key);

#endif
