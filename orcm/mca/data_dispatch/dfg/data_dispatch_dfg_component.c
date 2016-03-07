/*
 * Copyright (c) 2014-2016 Intel, Inc.  All rights reserved.
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

#include "orcm/mca/data_dispatch/data_dispatch.h"
#include "orcm/mca/data_dispatch/dfg/data_dispatch_dfg.h"

static int component_open(void);
static int component_close(void);
static int component_query(mca_base_module_t **module, int *priority);

orcm_data_dispatch_base_component_t mca_data_dispatch_dfg_component = {
    {
        ORCM_DATA_DISPATCH_BASE_VERSION_1_0_0,
        /* Component name and version */
        .mca_component_name = "dfg",
        MCA_BASE_MAKE_VERSION(component, ORCM_MAJOR_VERSION, ORCM_MINOR_VERSION,
                              ORCM_RELEASE_VERSION),

        /* Component open and close functions */
        .mca_open_component = component_open,
        .mca_close_component = component_close,
        .mca_query_component = component_query,
        NULL
    },
    .base_data = {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
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
    *module = (mca_base_module_t*)&orcm_data_dispatch_dfg_module;
    *priority = 20;
    return ORCM_SUCCESS;
}
