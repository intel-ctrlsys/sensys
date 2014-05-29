/*
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */


#include "orcm_config.h"
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "orcm/constants.h"

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal/class/opal_pointer_array.h"

#include "orcm/mca/diag/base/base.h"


static bool selected = false;

/**
 * Function for weeding out diag components that don't want to run.
 *
 * Call the init function on all available components to find out if
 * they want to run.  Select all components that don't fail.  Failing
 * components will be closed and unloaded.  The selected modules will
 * be returned to the caller in a opal_list_t.
 */
int orcm_diag_base_select(void)
{
    mca_base_component_list_item_t *cli = NULL;
    mca_base_component_t *component = NULL;
    mca_base_module_t *module = NULL;
    orcm_diag_base_module_t *nmodule;
    orcm_diag_active_module_t *newmodule, *mod;
    int rc, priority;
    bool inserted;

    if (selected) {
        /* ensure we don't do this twice */
        return ORTE_SUCCESS;
    }
    selected = true;

    /* Query all available components and ask if they have a module */
    OPAL_LIST_FOREACH(cli, &orcm_diag_base_framework.framework_components, mca_base_component_list_item_t) {
        component = (mca_base_component_t *) cli->cli_component;

        opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "mca:diag:select: checking available component %s",
                            component->mca_component_name);

        /* If there's no query function, skip it */
        if (NULL == component->mca_query_component) {
            opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                                "mca:diag:select: Skipping component [%s]. It does not implement a query function",
                                component->mca_component_name );
            continue;
        }

        /* Query the component */
        opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "mca:diag:select: Querying component [%s]",
                            component->mca_component_name);
        rc = component->mca_query_component(&module, &priority);

        /* If no module was returned, then skip component */
        if (ORTE_SUCCESS != rc || NULL == module) {
            opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                                "mca:diag:select: Skipping component [%s]. Query failed to return a module",
                                component->mca_component_name );
            continue;
        }
        nmodule = (orcm_diag_base_module_t*) module;

        /* If we got a module, init it */
        if (NULL != nmodule->init) {
            if (ORCM_SUCCESS != nmodule->init()) {
                opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                                    "mca:diag:select: Skipping component [%s]. Init returned error",
                                    component->mca_component_name );
                continue;
            }
        }

        /* add to the list of selected modules */
        newmodule = OBJ_NEW(orcm_diag_active_module_t);
        newmodule->pri = priority;
        newmodule->module = nmodule;
        newmodule->component = component;

        /* maintain priority order */
        inserted = false;
        OPAL_LIST_FOREACH(mod, &orcm_diag_base.modules, orcm_diag_active_module_t) {
            if (priority > mod->pri) {
                opal_list_insert_pos(&orcm_diag_base.modules,
                                     (opal_list_item_t*)mod, &newmodule->super);
                inserted = true;
                break;
            }
        }
        if (!inserted) {
            /* must be lowest priority - add to end */
            opal_list_append(&orcm_diag_base.modules, &newmodule->super);
        }
    }

    if (4 < opal_output_get_verbosity(orcm_diag_base_framework.framework_output)) {
        opal_output(0, "%s: Final diag priorities", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        /* show the prioritized list */
        OPAL_LIST_FOREACH(mod, &orcm_diag_base.modules, orcm_diag_active_module_t) {
            opal_output(0, "\tDiagnostic: %s Priority: %d", mod->component->mca_component_name, mod->pri);
        }
    }

    return ORTE_SUCCESS;;
}
