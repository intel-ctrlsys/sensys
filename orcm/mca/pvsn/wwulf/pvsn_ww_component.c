/*
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "opal/util/output.h"

#include "opal/mca/base/mca_base_var.h"

#include "orcm/runtime/orcm_globals.h"
#include "pvsn_ww.h"

/*
 * Public string for version number
 */
const char *orcm_pvsn_wwulf_component_version_string = 
    "ORCM PVSN wwulf MCA component version " ORCM_VERSION;

/*
 * Local functionality
 */
static int pvsn_ww_open(void);
static int pvsn_ww_close(void);
static int pvsn_ww_component_query(mca_base_module_t **module, int *priority);

/*
 * Instantiate the public struct with all of our public information
 * and pointer to our public functions in it
 */
orcm_pvsn_base_component_t mca_pvsn_wwulf_component = {
    {
        ORCM_PVSN_BASE_VERSION_1_0_0,
        /* Component name and version */
        "wwulf",
        ORCM_MAJOR_VERSION,
        ORCM_MINOR_VERSION,
        ORCM_RELEASE_VERSION,
        
        /* Component open and close functions */
        pvsn_ww_open,
        pvsn_ww_close,
        pvsn_ww_component_query,
        NULL
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
};

static int pvsn_ww_open(void) 
{
    return ORCM_SUCCESS;
}

static int pvsn_ww_close(void)
{
    return ORCM_SUCCESS;
}

static int pvsn_ww_component_query(mca_base_module_t **module, int *priority)
{
    /* only schedulers will open this framework */
    *priority = 5;
    *module = (mca_base_module_t *)&orcm_pvsn_wwulf_module;
    return ORCM_SUCCESS;
}
