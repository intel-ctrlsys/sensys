/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include "opal/mca/mca.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"

#include "orte/types.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/name_fns.h"
#include "orte/util/regex.h"
#include "orte/util/show_help.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/diag/base/base.h"

void orcm_diag_base_calibrate(void)
{
    orcm_diag_active_module_t *mod;

    OPAL_LIST_FOREACH(mod, &orcm_diag_base.modules, orcm_diag_active_module_t) {
        if (NULL != mod->module->calibrate) {
            opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                                "%s diag:base: calibrating component %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                mod->component->mca_component_name);
            mod->module->calibrate();
        }
    }
}
