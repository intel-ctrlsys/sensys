/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */


#include "orcm_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/rm/base/base.h"

static bool selected = false;

/**
 * Function for selecting multiple components from all those that are
 * available.
 */
int orcm_rm_base_select(void)
{
    mca_base_component_list_item_t *cli = NULL;
    mca_base_component_t *component = NULL;
    mca_base_module_t *module = NULL;
    orcm_rm_base_module_t *nmodule;
    orcm_rm_base_active_module_t *newmodule, *mod;
    int rc, priority;
    bool inserted;

    if (selected) {
        /* ensure we don't do this twice */
        return ORCM_SUCCESS;
    }
    selected = true;

    /* Query all available components and ask if they have a module */
    OPAL_LIST_FOREACH(cli, &orcm_rm_base_framework.framework_components, mca_base_component_list_item_t) {
        component = (mca_base_component_t *) cli->cli_component;

        opal_output_verbose(5, orcm_rm_base_framework.framework_output,
                            "mca:rm:select: checking available component %s", component->mca_component_name);

        /* If there's no query function, skip it */
        if (NULL == component->mca_query_component) {
            opal_output_verbose(5, orcm_rm_base_framework.framework_output,
                                "mca:rm:select: Skipping component [%s]. It does not implement a query function",
                                component->mca_component_name);
            continue;
        }

        /* Query the component */
        opal_output_verbose(5, orcm_rm_base_framework.framework_output,
                            "mca:rm:select: Querying component [%s]",
                            component->mca_component_name);
        rc = component->mca_query_component(&module, &priority);

        /* If no module was returned, then skip component */
        if (ORCM_SUCCESS != rc || NULL == module) {
            opal_output_verbose(5, orcm_rm_base_framework.framework_output,
                                "mca:rm:select: Skipping component [%s]. Query failed to return a module",
                                component->mca_component_name);
            continue;
        }

        /* If we got a module, keep it */
        nmodule = (orcm_rm_base_module_t*) module;
        /* give the module a chance to init */
        if (NULL != nmodule->init) {
            if (ORCM_SUCCESS != (rc = nmodule->init())) {
                opal_output_verbose(5, orcm_rm_base_framework.framework_output,
                                    "mca:rm:select: Skipping component [%s]. Init returned error %s",
                                    component->mca_component_name, ORTE_ERROR_NAME(rc));
                continue;
            }
        }

        /* add to the list of selected modules */
        newmodule = OBJ_NEW(orcm_rm_base_active_module_t);
        newmodule->pri = priority;
        newmodule->module = nmodule;
        newmodule->component = component;

        /* maintain priority order */
        inserted = false;
        OPAL_LIST_FOREACH(mod, &orcm_rm_base.active_modules, orcm_rm_base_active_module_t) {
            if (priority > mod->pri) {
                opal_list_insert_pos(&orcm_rm_base.active_modules,
                                     (opal_list_item_t*)mod, &newmodule->super);
                inserted = true;
                break;
            }
        }
        if (!inserted) {
            /* must be lowest priority - add to end */
            opal_list_append(&orcm_rm_base.active_modules, &newmodule->super);
        }
    }

    /* if we didn't get any plugins, that's an error */
    if (0 == opal_list_get_size(&orcm_rm_base.active_modules)) {
        return ORCM_ERR_NOT_FOUND;
    }

    if (4 < opal_output_get_verbosity(orcm_rm_base_framework.framework_output)) {
        opal_output(0, "%s: Final resource manager priorities", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        /* show the prioritized list */
        OPAL_LIST_FOREACH(mod, &orcm_rm_base.active_modules, orcm_rm_base_active_module_t) {
            opal_output(0, "\tResource Manager: %s Priority: %d", mod->component->mca_component_name, mod->pri);
        }
    }

    return ORCM_SUCCESS;;
}
