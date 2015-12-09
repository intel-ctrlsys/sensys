/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2014-2015 Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/class/opal_list.h"

#include "orcm/runtime/orcm_globals.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orcm/mca/evgen/base/base.h"


// Convert from ORCM_RAS_SEVERITY to ORTE_NOTIFIER
static orte_notifier_severity_t lookup_orte_severity[ORCM_RAS_SEVERITY_UNKNOWN] = {
    ORTE_NOTIFIER_EMERG,  // ORCM_RAS_SEVERITY_EMERG
    ORTE_NOTIFIER_ALERT,  // ORCM_RAS_SEVERITY_FATAL
    ORTE_NOTIFIER_ALERT,  // ORCM_RAS_SEVERITY_ALERT
    ORTE_NOTIFIER_CRIT,   // ORCM_RAS_SEVERITY_CRIT
    ORTE_NOTIFIER_ERROR,  // ORCM_RAS_SEVERITY_ERROR
    ORTE_NOTIFIER_WARN,   // ORCM_RAS_SEVERITY_WARNING
    ORTE_NOTIFIER_NOTICE, // ORCM_RAS_SEVERITY_NOTICE
    ORTE_NOTIFIER_INFO,   // ORCM_RAS_SEVERITY_INFO
    ORTE_NOTIFIER_DEBUG,  // ORCM_RAS_SEVERITY_TRACE
    ORTE_NOTIFIER_DEBUG   // ORCM_RAS_SEVERITY_DEBUG
};

void orcm_evgen_base_event(int sd, short args, void *cbdata)
{
    opal_output_verbose(5, orcm_evgen_base_framework.framework_output,
                        "%s evgen:base: evgen event called",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    orcm_evgen_active_module_t* active;
    orcm_ras_event_t *cd = (orcm_ras_event_t*)cbdata;

    OPAL_LIST_FOREACH(active, &orcm_evgen_base.actives, orcm_evgen_active_module_t) {
        active->module->generate(cd);
    }

    if (NULL != cd) {
        if (NULL != cd->cbfunc) {
            cd->cbfunc(cd->cbdata);
        }
        OBJ_RELEASE(cd);
    }
}

orte_notifier_severity_t orcm_evgen_base_convert_ras_severity_to_orte_notifier(int severity)
{
    if(severity < 0) {
        return lookup_orte_severity[0];
    } else if(severity >= ORCM_RAS_SEVERITY_UNKNOWN) {
        return lookup_orte_severity[ORCM_RAS_SEVERITY_DEBUG];
    } else {
        return lookup_orte_severity[severity];
    }
}
