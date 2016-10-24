/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <string.h>

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"

#include "orte/util/proc_info.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/parser/base/base.h"

static bool selected = false;

/*
 * Function for selecting one component from all those that are
 * available.
 */
int orcm_parser_base_select(void)
{
    mca_base_component_list_item_t *cli = NULL;
    mca_base_component_t *component = NULL;
    mca_base_module_t *module = NULL;
    orcm_parser_base_module_t *nmodule;
    orcm_parser_active_module_t *newmodule, *mod;
    int rc, priority;
    bool inserted;

    if (true == selected) {
        /* ensure we don't do this twice */
        return ORCM_SUCCESS;
    }
    selected = true;

    /* Query all available components and ask if they have a module */
    OPAL_LIST_FOREACH(cli, &orcm_parser_base_framework.framework_components, mca_base_component_list_item_t) {

        component = (mca_base_component_t *) cli->cli_component;
        if (NULL == component) {
            continue;
        }

        opal_output_verbose(5, orcm_parser_base_framework.framework_output,
                            "mca:parser:select: checking available component %s", component->mca_component_name);

        /* If there's no query function, skip it */
        if (NULL == component->mca_query_component) {
            opal_output_verbose(5, orcm_parser_base_framework.framework_output,
                                "mca:parser:select: Skipping component [%s]. It does not implement a query function",
                                component->mca_component_name );
            continue;
        }

        /* Query the component */
        opal_output_verbose(5, orcm_parser_base_framework.framework_output,
                            "mca:parser:select: Querying component [%s]",
                            component->mca_component_name);
        rc = component->mca_query_component(&module, &priority);

        /* If no module was returned, then skip component */
        if (ORCM_SUCCESS != rc || NULL == module) {
            opal_output_verbose(5, orcm_parser_base_framework.framework_output,
                                "mca:parser:select: Skipping component [%s]. Query failed to return a module",
                                component->mca_component_name );
            continue;
        }
        nmodule = (orcm_parser_base_module_t*) module;

        /* init the module */
        if (NULL != nmodule->init) {
            nmodule->init();
        }

        /* If we got a module, keep it */
        /* add to the list of active modules */
        newmodule = OBJ_NEW(orcm_parser_active_module_t);
        if (NULL == newmodule) {
            if (NULL != nmodule->finalize) {
                nmodule->finalize();
            }
            continue;
        }
        newmodule->priority = priority;
        newmodule->module = nmodule;
        newmodule->component = component;

        /* maintain priority order */
        inserted = false;
        OPAL_LIST_FOREACH(mod, &orcm_parser_base.actives, orcm_parser_active_module_t) {
            if (priority > mod->priority) {
                opal_list_insert_pos(&orcm_parser_base.actives,
                                     (opal_list_item_t*)mod, &newmodule->super);
                inserted = true;
                break;
            }
        }
        if (!inserted) {
            /* must be lowest priority - add to end */
            opal_list_append(&orcm_parser_base.actives, &newmodule->super);
        }
    }
    return ORCM_SUCCESS;
}

void orcm_parser_base_reset_selected(void)
{
    selected = false;
}
