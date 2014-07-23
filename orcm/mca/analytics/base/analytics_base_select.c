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

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/analytics/base/base.h"


static int workflow_id = 0;

/**
 * Function for weeding out analytics components that don't want to run.
 *
 * Call the init function on all available components to find out if
 * they want to run.  Select all components that are requested and that don't 
 * fail.  Failing components will be closed and unloaded.  
 * The selected modules will be stored in workflow's opal_list_t.
 */

/* this will look odd, but we need to intialize each module for each step
 * separately because each instatiation of a module will potentially
 * have different attributes that need to be intialized as well as 
 * workflows will be running in simultaneous threads so we need
 * separate modules between threads as well
 */

int orcm_analytics_base_select_workflow(orcm_workflow_t *workflow)
{
    mca_base_component_list_item_t *cli = NULL;
    mca_base_component_t *component = NULL;
    mca_base_module_t *module = NULL;
    orcm_analytics_base_module_t *nmodule;
    int rc, priority;

    workflow->workflow_id = workflow_id;
    workflow_id++;
    
    /* Logic needs to be:
     * For each module in ordered_list_of_modules
     *   make sure the module is selectable
     *   initialize with specified attributes
     *   create a new workflow step
     *   save module in workflow step
     *   append workflow step to step list
     */
    
    /* Query all available components and ask if they have a module */
    OPAL_LIST_FOREACH(cli, &orcm_analytics_base_framework.framework_components, mca_base_component_list_item_t) {
        component = (mca_base_component_t *) cli->cli_component;

        opal_output_verbose(5, orcm_analytics_base_framework.framework_output,
                            "mca:analytics:select: checking available component %s",
                            component->mca_component_name);

        /* If there's no query function, skip it */
        if (NULL == component->mca_query_component) {
            opal_output_verbose(5, orcm_analytics_base_framework.framework_output,
                                "mca:analytics:select: Skipping component [%s]. It does not implement a query function",
                                component->mca_component_name );
            continue;
        }

        /* Query the component */
        opal_output_verbose(5, orcm_analytics_base_framework.framework_output,
                            "mca:analytics:select: Querying component [%s]",
                            component->mca_component_name);
        rc = component->mca_query_component(&module, &priority);

        /* If no module was returned, then skip component */
        if (ORTE_SUCCESS != rc || NULL == module) {
            opal_output_verbose(5, orcm_analytics_base_framework.framework_output,
                                "mca:analytics:select: Skipping component [%s]. Query failed to return a module",
                                component->mca_component_name );
            continue;
        }
        nmodule = (orcm_analytics_base_module_t*) module;

        /* If we got a module, init it */
        /* component_create will init */
     }

    return ORTE_SUCCESS;
}
