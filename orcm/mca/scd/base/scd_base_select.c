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

#include "orcm/mca/scd/base/base.h"

/*
 * Function for selecting one component from all those that are available.
 */

int orcm_scd_base_select(void)
{
    orcm_scd_base_component_t *best_component = NULL;
    orcm_scd_base_module_t *best_module = NULL;
    
    /*
     * Select the best component
     */
    if(OPAL_SUCCESS != mca_base_select("scd",
                                       orcm_scd_base_framework.framework_output,
                                       &orcm_scd_base_framework.framework_components,
                                       (mca_base_module_t **) &best_module,
                                       (mca_base_component_t **) &best_component)) {
        /* This will only happen if no component was selected */
        return ORCM_ERR_NOT_FOUND;
    }
    
    /* Save the winner */
    orcm_scd_base.module = best_module;
    
    if (NULL != orcm_scd_base.module->init) {
        return orcm_scd_base.module->init();
    }
    return ORCM_SUCCESS;
}
