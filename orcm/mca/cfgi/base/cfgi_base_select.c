/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */


#include "orcm_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"

#include "orte/util/show_help.h"

#include "orcm/mca/cfgi/base/base.h"


/**
 * Function for selecting one component from all those that are
 * available.
 */
int orcm_cfgi_base_select(void)
{
    mca_base_component_list_item_t *cli;
    orcm_cfgi_base_component_t *component;
    bool added;
    int pri;
    orcm_cfgi_base_module_t *mod;
    orcm_cfgi_base_active_t *active, *cmp;

    /* Query all available components and ask if they are available */
    OPAL_LIST_FOREACH(cli, &orcm_cfgi_base_framework.framework_components, mca_base_component_list_item_t) {
        component = (orcm_cfgi_base_component_t *) cli->cli_component;

        opal_output_verbose(5, orcm_cfgi_base_framework.framework_output,
                            "mca:cfgi:select: checking available component %s",
                            component->base_version.mca_component_name);

        /* If there's no query function, skip it */
        if (NULL == component->base_version.mca_query_component) {
            opal_output_verbose(5, orcm_cfgi_base_framework.framework_output,
                                "mca:oob:select: Skipping component [%s]. It does not implement a query function",
                                component->base_version.mca_component_name );
            continue;
        }

        /* Query the component */
        opal_output_verbose(5, orcm_cfgi_base_framework.framework_output,
                            "mca:cfgi:select: Querying component [%s]",
                            component->base_version.mca_component_name);

        if (ORCM_SUCCESS != component->base_version.mca_query_component((mca_base_module_t**)&mod, &pri)) {
            opal_output_verbose(5, orcm_cfgi_base_framework.framework_output,
                                "mca:oob:select: Skipping component [%s]",
                                component->base_version.mca_component_name );
            continue;
        }

        /* if the module fails to init, then skip it */
        if (NULL == mod->init || ORCM_SUCCESS != mod->init()) {
            opal_output_verbose(5, orcm_cfgi_base_framework.framework_output,
                                "mca:oob:select: Skipping component [%s] - failed to init",
                                component->base_version.mca_component_name );
            continue;
        }

        /* record it, but maintain priority order */
        added = false;
        OPAL_LIST_FOREACH(cmp, &orcm_cfgi_base.actives, orcm_cfgi_base_active_t) {
            if (cmp->priority > pri) {
                continue;
            }
            opal_output_verbose(5, orcm_cfgi_base_framework.framework_output,
                                "mca:cfgi:select: Inserting module");
            active = OBJ_NEW(orcm_cfgi_base_active_t);
            active->mod = (orcm_cfgi_base_module_t*)mod;
            active->priority = pri;
            opal_list_insert_pos(&orcm_cfgi_base.actives,
                                 &cmp->super, &active->super);
            added = true;
            break;
        }
        if (!added) {
            /* add to end */
            opal_output_verbose(5, orcm_cfgi_base_framework.framework_output,
                                "mca:cfgi:select: Adding module to end");
            active = OBJ_NEW(orcm_cfgi_base_active_t);
            active->mod = (orcm_cfgi_base_module_t*)mod;
            active->priority = pri;
            opal_list_append(&orcm_cfgi_base.actives, &active->super);
        }
    }

    if (0 == opal_list_get_size(&orcm_cfgi_base.actives)) {
        /* no support available means we really cannot run */
        opal_output_verbose(5, orcm_cfgi_base_framework.framework_output,
                            "mca:cfgi:select: Init failed to return any available configuration modules");
        orte_show_help("help-orcm-cfgi.txt", "no-modules", true);
        return ORCM_ERR_SILENT;
    }

    return ORCM_SUCCESS;
}
