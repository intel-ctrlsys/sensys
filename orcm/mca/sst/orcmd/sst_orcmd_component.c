/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/util/output.h"
#include "opal/mca/base/mca_base_var.h"

#include "orte/runtime/orte_globals.h"

#include "orcm/mca/sst/sst.h"
#include "orcm/mca/sst/orcmd/sst_orcmd.h"

static int component_open(void);
static int component_close(void);
static int component_query(mca_base_module_t **module, int *priority);
static int component_register(void);

orcm_sst_orcmd_component_t mca_sst_orcmd_component = {
    {
        {
            ORCM_SST_BASE_VERSION_1_0_0,

            "orcmd",
            ORCM_MAJOR_VERSION,
            ORCM_MINOR_VERSION,
            ORCM_RELEASE_VERSION,
            component_open,
            component_close,
            component_query,
            component_register
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        }
    }
};

static int component_open(void)
{
     return ORCM_SUCCESS;
}

static int component_close(void)
{
    return ORCM_SUCCESS;
}

static int component_query(mca_base_module_t **module, int *priority)
{
    if (ORTE_PROC_IS_CM && ORTE_PROC_IS_DAEMON) {
        *module = (mca_base_module_t*)&orcm_sst_orcmd_module;
        *priority = 100;
        return ORCM_SUCCESS;
    }

    *module = NULL;
    *priority = 0;
    return ORCM_ERROR;
}

static int component_register(void)
{
    mca_sst_orcmd_component.scheduler_reqd = true;
    (void) mca_base_component_var_register(&mca_sst_orcmd_component.super.base_version,
                                           "scheduler_reqd",
                                           "Whether or not a scheduler is required - allows testing in non-scheduled environments",
                                           MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_sst_orcmd_component.scheduler_reqd);
    return ORTE_SUCCESS;
}
