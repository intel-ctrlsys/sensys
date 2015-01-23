/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"

#include "orcm/util/attr.h"

char* orcm_attr_key_print(orte_attribute_key_t key)
{
    if (ORCM_ATTR_KEY_BASE < key &&
        key < ORCM_ATTR_KEY_MAX) {
        /* belongs to ORCM, so we handle it */
        switch(key) {
        case ORCM_PWRMGMT_POWER_MODE_KEY:
        return "PWRMGMT_POWER_MODE";
        case ORCM_PWRMGMT_POWER_BUDGET_KEY:
        return "PWRMGMT_POWER_BUDGET";
        case ORCM_PWRMGMT_POWER_WINDOW_KEY:
        return "PWRMGMT_POWER_WINDOW";
        case ORCM_PWRMGMT_CAP_OVERAGE_LIMIT_KEY:
        return "PWRMGMT_CAP_OVERAGE_LIMIT";
        case ORCM_PWRMGMT_CAP_UNDERAGE_LIMIT_KEY:
        return "PWRMGMT_CAP_UNDERAGE_LIMIT";
        case ORCM_PWRMGMT_CAP_OVERAGE_TIME_LIMIT_KEY:
        return "PWRMGMT_CAP_OVERAGE_TIME_LIMIT";
        case ORCM_PWRMGMT_CAP_UNDERAGE_TIME_LIMIT_KEY:
        return "PWRMGMT_CAP_UNDERAGE_TIME_LIMIT";
        case ORCM_PWRMGMT_SUPPORTED_MODES_KEY:
        return "PWRMGMT_SUPPORTED_MODES";
        case ORCM_PWRMGMT_SELECTED_COMPONENT_KEY:
        return "PWRMGMT_SELECTED_COMPONENT";
        case ORCM_PWRMGMT_FREQ_STRICT_KEY:
        return "PWRMGMT_FREQ_STRICT";

        default:
            return "UNKNOWN-KEY";
        }
    }

    /* get here if nobody know what to do */
    return "UNKNOWN-KEY";
}
