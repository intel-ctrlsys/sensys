/*
 * Copyright (c) 2014-2015 Intel, Inc.  All rights reserved. 
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
#include "orcm/mca/sst/emulator/sst_emulator.h"

static int component_open(void);
static int component_close(void);
static int component_query(mca_base_module_t **module, int *priority);
static int component_register(void);

orcm_sst_emulator_component_t mca_sst_emulator_component = {
    {
        {
            ORCM_SST_BASE_VERSION_1_0_0,
            /* Component name and version */
            .mca_component_name = "emulator",
            MCA_BASE_MAKE_VERSION(component, ORCM_MAJOR_VERSION, ORCM_MINOR_VERSION,
                                  ORCM_RELEASE_VERSION),
        
            /* Component open and close functions */
            .mca_open_component = component_open,
            .mca_close_component = component_close,
            .mca_query_component = component_query,
            .mca_register_component_params = component_register

        },
        .base_data = {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
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
    if (NULL != mca_sst_emulator_component.target) {
        *module = (mca_base_module_t*)&orcm_sst_emulator_module;
        *priority = 100;
        return ORCM_SUCCESS;
    }

    *module = NULL;
    *priority = 0;
    return ORCM_ERROR;
}

static int component_register(void)
{
    mca_sst_emulator_component.target = NULL;
    (void) mca_base_component_var_register(&mca_sst_emulator_component.super.base_version,
                                           "target",
                                           "Emulation target expressed as row:rack:node",
                                           MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_sst_emulator_component.target);
    return ORTE_SUCCESS;
}
