/*
 * Copyright (c) 2010      Cisco Systems, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orte_config.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/errmgr/base/base.h"
#include "orte/mca/errmgr/base/errmgr_private.h"
#include "errmgr_orcmsd_hnp.h"

/*
 * Public string for version number
 */
const char *orte_errmgr_orcmsd_hnp_component_version_string = 
    "ORTE ERRMGR orcmsd_hnp MCA component version " ORTE_VERSION;

/*
 * Local functionality
 */
static int orcmsd_hnp_register(void);
static int orcmsd_hnp_open(void);
static int orcmsd_hnp_close(void);
static int orcmsd_hnp_component_query(mca_base_module_t **module, int *priority);

/*
 * Instantiate the public struct with all of our public information
 * and pointer to our public functions in it
 */
orte_errmgr_base_component_t mca_errmgr_orcmsd_hnp_component = {
    /* Handle the general mca_component_t struct containing 
     *  meta information about the component orcmsd_hnp
     */
    {
        ORTE_ERRMGR_BASE_VERSION_3_0_0,
        /* Component name and version */
        "orcmsd_hnp",
        ORTE_MAJOR_VERSION,
        ORTE_MINOR_VERSION,
        ORTE_RELEASE_VERSION,
        
        /* Component open and close functions */
        orcmsd_hnp_open,
        orcmsd_hnp_close,
        orcmsd_hnp_component_query,
        orcmsd_hnp_register
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
};

static int my_priority;

static int orcmsd_hnp_register(void)
{
    mca_base_component_t *c = &mca_errmgr_orcmsd_hnp_component.base_version;

    my_priority = 1000;
    (void) mca_base_component_var_register(c, "priority",
                                           "Priority of the orcmsd_hnp errmgr component",
                                           MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY, &my_priority);

    return ORTE_SUCCESS;
}

static int orcmsd_hnp_open(void) 
{
    return ORTE_SUCCESS;
}

static int orcmsd_hnp_close(void)
{
    return ORTE_SUCCESS;
}

static int orcmsd_hnp_component_query(mca_base_module_t **module, int *priority)
{
    if (ORTE_PROC_IS_CM && ORTE_PROC_IS_HNP) {
        /* set our priority high as we are the default for orcmsd */
        *priority = 1001;
        *module = (mca_base_module_t *)&orte_errmgr_orcmsd_hnp_module;
        return ORTE_SUCCESS;        
    }

    *module = NULL;
    *priority = -1;
    return ORTE_ERROR;
}
