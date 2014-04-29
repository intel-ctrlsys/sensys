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
#include "scd_basic.h"

/*
 * Public string for version number
 */
const char *orcm_scd_basic_component_version_string = 
    "ORCM SCD basic MCA component version " ORCM_VERSION;

/*
 * Local functionality
 */
static int scd_basic_open(void);
static int scd_basic_close(void);
static int scd_basic_component_query(mca_base_module_t **module, int *priority);

/*
 * Instantiate the public struct with all of our public information
 * and pointer to our public functions in it
 */
orcm_scd_base_component_t mca_scd_basic_component = {
    {
        ORCM_SCD_BASE_VERSION_1_0_0,
        /* Component name and version */
        "basic",
        ORCM_MAJOR_VERSION,
        ORCM_MINOR_VERSION,
        ORCM_RELEASE_VERSION,
        
        /* Component open and close functions */
        scd_basic_open,
        scd_basic_close,
        scd_basic_component_query,
        NULL
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
};

static int scd_basic_open(void) 
{
    return ORCM_SUCCESS;
}

static int scd_basic_close(void)
{
    return ORCM_SUCCESS;
}

static int scd_basic_component_query(mca_base_module_t **module, int *priority)
{
    if (ORCM_PROC_IS_SCHED) {
        *priority = 5;
        *module = (mca_base_module_t *)&orcm_scd_basic_module;
        return ORCM_SUCCESS;
    }

    /* otherwise, I am a tool and should be ignored */
    *priority = 0;
    *module = NULL;
    return ORCM_ERR_TAKE_NEXT_OPTION;
}
