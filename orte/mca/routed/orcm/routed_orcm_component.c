/*
 * Copyright (c) 2013      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"

#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_var.h"

#include "orte/util/proc_info.h"

#include "orte/mca/routed/base/base.h"
#include "routed_orcm.h"

static int orcm_open(void);
static int orcm_query(mca_base_module_t **module, int *priority);
static int orcm_register(void);

/**
 * component definition
 */
orte_routed_orcm_component_t mca_routed_orcm_component = {
    {
        /* First, the mca_base_component_t struct containing meta
        information about the component itself */

        {
        ORTE_ROUTED_BASE_VERSION_2_0_0,

        "orcm", /* MCA component name */
        ORTE_MAJOR_VERSION,  /* MCA component major version */
        ORTE_MINOR_VERSION,  /* MCA component minor version */
        ORTE_RELEASE_VERSION,  /* MCA component release version */
        orcm_open,
        NULL,
        orcm_query,
        orcm_register
        },
        {
        /* This component can be checkpointed */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
        }
    }
};

static int orcm_register(void)
{
    mca_base_component_t *c = &mca_routed_orcm_component.super.base_version;

    mca_routed_orcm_component.radix = 32;
    (void) mca_base_component_var_register(c, "radix",
                                           "Radix to be used for orcm radix tree",
                                           MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_routed_orcm_component.radix);
    return ORTE_SUCCESS;
}

static int orcm_open(void)
{
    return ORTE_SUCCESS;
}

static int orcm_query(mca_base_module_t **module, int *priority)
{
    if (ORTE_PROC_IS_CM) {
        *priority = 1000;
        *module = (mca_base_module_t *) &orte_routed_orcm_module;
        return ORTE_SUCCESS;
    }

    *priority = 0;
    *module = NULL;
    return ORTE_ERROR;
}
