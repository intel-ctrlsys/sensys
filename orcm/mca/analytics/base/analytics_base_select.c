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

#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/analytics/base/base.h"
#include "orcm/mca/analytics/base/analytics_private.h"


int orcm_analytics_base_select_workflow_step(orcm_workflow_step_t *workstep)
{
    mca_base_component_list_item_t *cli = NULL;
    orcm_analytics_base_component_t *component = NULL;
    mca_base_component_t *basecomp = NULL;

    /* Find requested component, and ask if it is available */
    OPAL_LIST_FOREACH(cli,
                      &orcm_analytics_base_framework.framework_components,
                      mca_base_component_list_item_t) {
        basecomp = (mca_base_component_t *)cli->cli_component;
        component = (orcm_analytics_base_component_t *)basecomp;
        
        if (0 == strncmp(basecomp->mca_component_name,
                         workstep->analytic,
                         MCA_BASE_MAX_COMPONENT_NAME_LEN + 1)) {
            opal_output_verbose(5, orcm_analytics_base_framework.framework_output,
                                "mca:analytics:select: found requested component %s",
                                basecomp->mca_component_name);
            
            if (!component->available()) {
                ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
                return ORCM_ERR_NOT_AVAILABLE;
            }
            
            /* create a handle to the module, store in workstep */
            if (NULL == (workstep->mod = component->create_handle())) {
                opal_output_verbose(5, orcm_analytics_base_framework.framework_output,
                                    "mca:analytics:select: Skipping component [%s]. It does not implement a query function",
                                    basecomp->mca_component_name );
                ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
        }
    }
    
    return ORCM_SUCCESS;
}
