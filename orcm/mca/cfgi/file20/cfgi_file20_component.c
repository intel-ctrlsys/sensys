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

#include "orcm/mca/cfgi/cfgi.h"
#include "orcm/mca/cfgi/file20/cfgi_file20.h"

static int component_open(void);
static int component_close(void);
static int component_query(mca_base_module_t **module, int *priority);

orcm_cfgi_base_component_t mca_cfgi_file20_component = {
    {
        ORCM_CFGI_BASE_VERSION_1_0_0,
        /* Component name and version */
        .mca_component_name = "file20",
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
    /* deprecated version, retain support at low priority */
    *module = (mca_base_module_t*)&orcm_cfgi_file20_module;
    *priority = 5;
    return ORCM_SUCCESS;
}
