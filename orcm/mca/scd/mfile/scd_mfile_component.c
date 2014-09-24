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
#include "scd_mfile.h"

/*
 * Public string for version number
 */
const char *orcm_scd_mfile_component_version_string = 
    "ORCM SCD mfile MCA component version " ORCM_VERSION;

/*
 * Local functionality
 */
static int mfile_open(void);
static int mfile_close(void);
static int mfile_component_query(mca_base_module_t **module, int *priority);
static int mfile_component_register(void);

/*
 * Instantiate the public struct with all of our public information
 * and pointer to our public functions in it
 */
mca_scd_mfile_component_t mca_scd_mfile_component = {
    {
        {
            ORCM_SCD_BASE_VERSION_1_0_0,
            /* Component name and version */
            "mfile",
            ORCM_MAJOR_VERSION,
            ORCM_MINOR_VERSION,
            ORCM_RELEASE_VERSION,
        
            /* Component open and close functions */
            mfile_open,
            mfile_close,
            mfile_component_query,
            mfile_component_register
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
    },
};

static int mfile_open(void) 
{
    return ORCM_SUCCESS;
}

static int mfile_close(void)
{
    return ORCM_SUCCESS;
}

static int mfile_component_query(mca_base_module_t **module, int *priority)
{
    if (ORCM_PROC_IS_SCHED && NULL != mca_scd_mfile_component.moab_interface_dir) {
        *priority = 100;
        *module = (mca_base_module_t *)&orcm_scd_mfile_module;
        return ORCM_SUCCESS;
    }

    /* otherwise, I am a tool and should be ignored */
    *priority = 0;
    *module = NULL;
    return ORCM_ERR_TAKE_NEXT_OPTION;
}

static int mfile_component_register(void) {
    mca_base_component_t *c = &mca_scd_mfile_component.super.base_version;

    /* retrieve the name of the database to be used */
    mca_scd_mfile_component.moab_interface_dir = NULL;
    (void) mca_base_component_var_register (c, "interface_dir", "Name of Moab interface directory",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_scd_mfile_component.moab_interface_dir);
    return ORCM_SUCCESS;
}

