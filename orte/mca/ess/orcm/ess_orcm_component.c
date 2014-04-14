/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 * These symbols are in a file by themselves to provide nice linker
 * semantics.  Since linkers generally pull in symbols by object
 * files, keeping these symbols as the only symbols in this file
 * prevents utility programs such as "ompi_info" from having to import
 * entire components just to query their version and parameters.
 */

#include "orte_config.h"
#include "orte/constants.h"

#include "orte/util/proc_info.h"

#include "orte/mca/ess/ess.h"
#include "orte/mca/ess/orcm/ess_orcm.h"

extern orte_ess_base_module_t orte_ess_orcm_module;

static int orcm_component_open(void);
static int orcm_component_query(mca_base_module_t **module, int *priority);
static int orcm_component_close(void);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */
orte_ess_base_component_t mca_ess_orcm_component = {
    {
        ORTE_ESS_BASE_VERSION_3_0_0,

        /* Component name and version */
        "orcm",
        ORTE_MAJOR_VERSION,
        ORTE_MINOR_VERSION,
        ORTE_RELEASE_VERSION,

        /* Component open and close functions */
        orcm_component_open,
        orcm_component_close,
        orcm_component_query,
        NULL
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    }
};


static int orcm_component_open(void)
{
    return ORTE_SUCCESS;
}


static int orcm_component_query(mca_base_module_t **module, int *priority)
{
    if (ORTE_PROC_IS_CM) {
        *priority = 1000;
        *module = (mca_base_module_t *)&orte_ess_orcm_module;
        return ORTE_SUCCESS;
    }
    
    /* Sadly, no */
    *priority = -1;
    *module = NULL;
    return ORTE_ERROR;
}


static int orcm_component_close(void)
{
    return ORTE_SUCCESS;
}
