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

#include "orcm/mca/sst/base/base.h"


/**
 * Function for selecting one component from all those that are
 * available.
 */
int orcm_sst_base_select(void)
{
    orcm_sst_base_component_t *best_component = NULL;
    orcm_sst_base_module_t *best_module = NULL;

    /*
     * Select the best component
     */
    if( OPAL_SUCCESS != mca_base_select("sst", orcm_sst_base_framework.framework_output,
                                        &orcm_sst_base_framework.framework_components,
                                        (mca_base_module_t **) &best_module,
                                        (mca_base_component_t **) &best_component) ) {
        /* This will only happen if no component was selected */
        return ORCM_ERR_NOT_FOUND;
    }

    /* Save the winner */
    orcm_sst = *best_module;

    if (NULL != orcm_sst.init) {
        return orcm_sst.init();
    }
    return ORCM_SUCCESS;
}
