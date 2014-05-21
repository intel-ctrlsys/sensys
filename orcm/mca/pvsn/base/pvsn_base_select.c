/*
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

#include "orcm/mca/pvsn/base/base.h"


static bool selected = false;

/**
 * Call the init function on all available components to find out if
 * they want to run.  Select the highest priority component that doesn't
 * fail. Failing components will be closed and unloaded.
 */
int orcm_pvsn_base_select(void)
{
    mca_base_component_list_item_t *cli = NULL;
    mca_base_component_t *component = NULL;
    mca_base_module_t *module = NULL;
    orcm_pvsn_base_module_t *nmodule, *best_module = NULL;
    int rc, priority, pri = -1;

    if (selected) {
        /* ensure we don't do this twice */
        return ORTE_SUCCESS;
    }
    selected = true;

    /* Query all available components and ask if they have a module */
    OPAL_LIST_FOREACH(cli, &orcm_pvsn_base_framework.framework_components, mca_base_component_list_item_t) {
        component = (mca_base_component_t *) cli->cli_component;

        opal_output_verbose(5, orcm_pvsn_base_framework.framework_output,
                            "mca:pvsn:select: checking available component %s",
                            component->mca_component_name);

        /* If there's no query function, skip it */
        if (NULL == component->mca_query_component) {
            opal_output_verbose(5, orcm_pvsn_base_framework.framework_output,
                                "mca:pvsn:select: Skipping component [%s]. It does not implement a query function",
                                component->mca_component_name );
            continue;
        }

        /* Query the component */
        opal_output_verbose(5, orcm_pvsn_base_framework.framework_output,
                            "mca:pvsn:select: Querying component [%s]",
                            component->mca_component_name);
        rc = component->mca_query_component(&module, &priority);

        /* If no module was returned, then skip component */
        if (ORTE_SUCCESS != rc || NULL == module) {
            opal_output_verbose(5, orcm_pvsn_base_framework.framework_output,
                                "mca:pvsn:select: Skipping component [%s]. Query failed to return a module",
                                component->mca_component_name );
            continue;
        }
        nmodule = (orcm_pvsn_base_module_t*) module;

        /* see if it can init */
        if (NULL == nmodule->init) {
            continue;
        }

        if (ORCM_SUCCESS != nmodule->init()) {
            if (NULL != nmodule->finalize) {
                nmodule->finalize();
            }
            continue;
        }

        /* keep the highest priority one */
        if (pri < priority) {
            best_module = nmodule;
            pri = priority;
        }
    }

    if (NULL == best_module) {
        return ORCM_ERR_NOT_FOUND;
    }

    orcm_pvsn = *best_module;
    return ORCM_SUCCESS;;
}
