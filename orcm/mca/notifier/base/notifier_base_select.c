/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
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

#include "opal/mca/mca.h"
#include "opal/util/argv.h"
#include "opal/mca/base/base.h"
#include "opal/util/output.h"

#include "orcm/mca/notifier/base/base.h"

/* Global variables */
/*
 * orcm_notifier_base_XXX_selected is set to true if at least 1 module has
 * been selected for the notifier XXX API interface.
 */
bool orcm_notifier_base_log_selected = false;
bool orcm_notifier_base_help_selected = false;
bool orcm_notifier_base_log_peer_selected = false;
bool orcm_notifier_base_log_event_selected = false;

static bool orcm_notifier_add_module(mca_base_component_t *component,
                                     orcm_notifier_base_module_t *module,
                                     int priority,
                                     char *include_list,
                                     opal_list_t *selected_modules);

static void onbsp_construct(orcm_notifier_base_selected_pair_t *obj)
{
    obj->onbsp_component = NULL;
    obj->onbsp_module = NULL;
    obj->onbsp_priority = -1;
}

static void onbsp_destruct(orcm_notifier_base_selected_pair_t *obj)
{
    onbsp_construct(obj);
}

OBJ_CLASS_INSTANCE(orcm_notifier_base_selected_pair_t,
                   opal_list_item_t,
                   onbsp_construct,
                   onbsp_destruct);


int orcm_notifier_base_select(void)
{
    mca_base_component_list_item_t *cli = NULL;
    mca_base_component_t *component = NULL;
    mca_base_module_t *module = NULL;
    int ret, priority, exit_status = ORCM_SUCCESS;
    opal_list_item_t *item;
    orcm_notifier_base_module_t *nmodule;
    bool module_needed;


    /* Query all available components and ask if they have a module */
    for (item = opal_list_get_first(&orcm_notifier_base_components_available);
         opal_list_get_end(&orcm_notifier_base_components_available) != item;
         item = opal_list_get_next(item)) {
        cli = (mca_base_component_list_item_t *) item;
        component = (mca_base_component_t *) cli->cli_component;

        /* If there's no query function, skip it */
        if (NULL == component->mca_query_component) {
            opal_output_verbose(5, orcm_notifier_base_framework.framework_output,
                                "mca:notify:select: Skipping component [%s]. It does not implement a query function",
                                component->mca_component_name );
            continue;
        }

        /* Query the component */
        opal_output_verbose(5, orcm_notifier_base_framework.framework_output,
                            "mca:notify:select: Querying component [%s]",
                            component->mca_component_name);
        ret = component->mca_query_component(&module, &priority);

        /* If no module was returned, then skip component */
        if (ORCM_SUCCESS != ret || NULL == module) {
            opal_output_verbose(5, orcm_notifier_base_framework.framework_output,
                                "mca:notify:select: Skipping component [%s]. Query failed to return a module",
                                component->mca_component_name );
            continue;
        }

        /* If we got a module, initialize it */
        nmodule = (orcm_notifier_base_module_t*) module;
        if (NULL != nmodule->init) {
            /* If the module doesn't want to be used, skip it */
            if (ORCM_SUCCESS != (ret = nmodule->init()) ) {
                if (ORCM_ERR_NOT_SUPPORTED != ret &&
                    ORCM_ERR_NOT_IMPLEMENTED != ret) {
                    exit_status = ret;
                    goto cleanup;
                }

                if (NULL != nmodule->finalize) {
                    nmodule->finalize();
                }
                continue;
            }
        }

        /*
         * OK, one module has been selected for the notifier framework, and
         * successfully initialized.
         * Now we have to include it in the per interface selected modules
         * lists if needed.
         */
        ret = orcm_notifier_add_module(component,
                                       nmodule,
                                       priority,
                                       orcm_notifier_base.imodules_log,
                                       &orcm_notifier_log_selected_modules);

        orcm_notifier_base_log_selected |= ret;
        /*
         * This variable is set to check if the module is needed by at least
         * one interface.
         */
        module_needed = ret;

        ret = orcm_notifier_add_module(component,
                                       nmodule,
                                       priority,
                                       orcm_notifier_base.imodules_help,
                                       &orcm_notifier_help_selected_modules);
        orcm_notifier_base_help_selected |= ret;
        module_needed |= ret;

        ret = orcm_notifier_add_module(component,
                                       nmodule,
                                       priority,
                                       orcm_notifier_base.imodules_log_peer,
                                       &orcm_notifier_log_peer_selected_modules);
        orcm_notifier_base_log_peer_selected |= ret;
        module_needed |= ret;

        ret = orcm_notifier_add_module(component,
                                       nmodule,
                                       priority,
                                       orcm_notifier_base.imodules_log_event,
                                       &orcm_notifier_log_event_selected_modules);
        orcm_notifier_base_log_event_selected |= ret;
        module_needed |= ret;

        /*
         * If the module is needed by at least one interface:
         * Unconditionally update the global list that will be used during
         * the close step. Else finalize it.
         */
        if (module_needed) {
            orcm_notifier_add_module(component,
                                     nmodule,
                                     priority,
                                     NULL,
                                     &orcm_notifier_base_selected_modules);
        } else {
            if (NULL != nmodule->finalize) {
                nmodule->finalize();
            }
        }
    }

    if (orcm_notifier_base_log_event_selected) {
        /*
         * This has to be done whatever the selected module. That's why it's
         * done here.
         */
        orcm_notifier_base_events_init();
    }

 cleanup:
    return exit_status;
}

/**
 * Check if a component name belongs to an include list and add it to the
 * list of selected modules.
 *
 * @param component        (IN)  component to be included
 * @param module           (IN)  module to be included
 * @param priority         (IN)  module priority
 * @param include_list     (IN)  list of module names to go through
 * @param selected_modules (OUT) list of selected modules to be updated
 * @return                       true/false depending on whether the module
 *                               has been added or not
 */
static bool orcm_notifier_add_module(mca_base_component_t *component,
                                     orcm_notifier_base_module_t *module,
                                     int priority,
                                     char *includes,
                                     opal_list_t *selected_modules)
{
    orcm_notifier_base_selected_pair_t *pair, *pair2;
    char *module_name;
    opal_list_item_t *item;
    int i;
    char **include_list=NULL;

    if (NULL == includes) {
        return false;
    }

    module_name = component->mca_component_name;
    include_list = opal_argv_split(includes, ',');

    /* If this component is not in the include list, skip it */
    for (i = 0; NULL != include_list[i]; i++) {
        if (!strcmp(include_list[i], module_name)) {
            break;
        }
    }
    if (NULL == include_list[i]) {
        opal_argv_free(include_list);
        return false;
    }
    opal_argv_free(include_list);

    /* Make an item for the list */
    pair = OBJ_NEW(orcm_notifier_base_selected_pair_t);
    pair->onbsp_component = (orcm_notifier_base_component_t*) component;
    pair->onbsp_module = module;
    pair->onbsp_priority = priority;

    /* Put it in the list in priority order */
    for (item = opal_list_get_first(selected_modules);
         opal_list_get_end(selected_modules) != item;
         item = opal_list_get_next(item)) {
        pair2 = (orcm_notifier_base_selected_pair_t*) item;
        if (priority > pair2->onbsp_priority) {
            opal_list_insert_pos(selected_modules, item, &(pair->super));
            break;
        }
    }
    if (opal_list_get_end(selected_modules) == item) {
        opal_list_append(selected_modules, &(pair->super));
    }

    return true;
}
