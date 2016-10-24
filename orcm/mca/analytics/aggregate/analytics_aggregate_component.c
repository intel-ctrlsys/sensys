/*
 * Copyright (c) 2014      Intel Corporation.  All rights reserved. 
 *
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "opal/util/output.h"

#include "opal/mca/base/mca_base_var.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/runtime/orcm_globals.h"
#include "analytics_aggregate.h"

/*
 * Public string for version number
 */
const char *orcm_analytics_aggregate_component_version_string =
    "ORCM ANALYTICS aggregate MCA component version " ORCM_VERSION;

/*
 * Local functionality
 */
static bool component_avail(void);
static orcm_analytics_base_module_t *component_create(void);

/*
 * Instantiate the public struct with all of our public information
 * and pointer to our public functions in it
 */
orcm_analytics_base_component_t mca_analytics_aggregate_component = {
    {
        ORCM_ANALYTICS_BASE_VERSION_1_0_0,
        /* Component name and version */
        .mca_component_name = "aggregate",
        MCA_BASE_MAKE_VERSION(component, ORCM_MAJOR_VERSION, ORCM_MINOR_VERSION,
                              ORCM_RELEASE_VERSION),
        
        /* Component open and close functions */
        .mca_open_component = NULL,
        .mca_close_component = NULL,
        .mca_query_component = NULL,
    },
    .base_data = {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
    .priority = 1,
    .available = component_avail,
    .create_handle = component_create,
    .finalize = NULL
};

static bool component_avail(void)
{
    /* we are always available */
    return true;
}

static orcm_analytics_base_module_t *component_create(void)
{
    mca_analytics_aggregate_module_t *mod;
    
    mod = (mca_analytics_aggregate_module_t*)malloc(sizeof(mca_analytics_aggregate_module_t));
    if (NULL == mod) {
        ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
        return NULL;
    }
    /* copy the APIs across */
    memcpy(mod, &orcm_analytics_aggregate_module.api, sizeof(orcm_analytics_base_module_t));
    /* let the module init itself */
    if (ORCM_SUCCESS != mod->api.init((orcm_analytics_base_module_t*)mod)) {
        /* release the module and return the error */
        free(mod);
        return NULL;
    }
    return (orcm_analytics_base_module_t*)mod;
}
