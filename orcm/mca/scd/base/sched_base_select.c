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

#include "orcm/mca/scd/base/base.h"


/**
 * Function for selecting one component from all those that are
 * available.
 */
int orcm_scd_base_select(void)
{
    int ret, exit_status = ORCM_SUCCESS;
    orcm_scd_base_component_t *best_component = NULL;
    orcm_scd_base_module_t *best_module = NULL;

    /*
     * Select the best component
     */
    if( OPAL_SUCCESS != mca_base_select("scd", orcm_scd_base_framework.framework_output,
                                        &orcm_scd_base_framework.framework_components,
                                        (mca_base_module_t **) &best_module,
                                        (mca_base_component_t **) &best_component) ) {
        /* This will only happen if no component was selected */
        exit_status = ORCM_ERR_NOT_FOUND;
        goto cleanup;
    }

    /* Save the winner */
    orcm_sched = *best_module;

    if (ORCM_SUCCESS != (ret = orcm_sched.init()) ) {
        exit_status = ret;
        goto cleanup;
    }

 cleanup:
    return exit_status;
}
