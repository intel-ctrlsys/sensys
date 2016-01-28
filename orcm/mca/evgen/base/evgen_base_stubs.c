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
