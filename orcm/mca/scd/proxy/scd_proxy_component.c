/*
# Copyright (c) 2014-2015 Intel, Inc.  All rights reserved. 
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
#include "scd_proxy.h"

/*
 * Public string for version number
 */
const char *orcm_scd_proxy_component_version_string = 
    "ORCM SCD proxy MCA component version " ORCM_VERSION;

/*
 * Local functionality
 */
static int scd_proxy_open(void);
static int scd_proxy_close(void);
static int scd_proxy_component_query(mca_base_module_t **module, int *priority);

/*
 * Instantiate the public struct with all of our public information
 * and pointer to our public functions in it
 */
orcm_scd_base_component_t mca_scd_proxy_component = {
    {
        ORCM_SCD_BASE_VERSION_1_0_0,
        /* Component name and version */
        .mca_component_name = "proxy",
        MCA_BASE_MAKE_VERSION(component, ORCM_MAJOR_VERSION, ORCM_MINOR_VERSION,
                              ORCM_RELEASE_VERSION),
        
        /* Component open and close functions */
        .mca_open_component = scd_proxy_open,
        .mca_close_component = scd_proxy_close,
        .mca_query_component = scd_proxy_component_query,
        NULL
    },
    .base_data = {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
};

static int scd_proxy_open(void) 
{
    return ORTE_SUCCESS;
}

static int scd_proxy_close(void)
{
    return ORTE_SUCCESS;
}

static int scd_proxy_component_query(mca_base_module_t **module, int *priority)
{
    if (ORCM_PROC_IS_TOOL) {
        /* must be picked by tools */
        *priority = 100;
        *module = (mca_base_module_t *)&orcm_scd_proxy_module;
        return ORTE_SUCCESS;
    }

    /* reject for anything other than a tool */
    *priority = 0;
    *module = NULL;
    return ORCM_ERROR;
}
