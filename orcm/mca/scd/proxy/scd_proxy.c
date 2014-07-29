/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/scd/base/base.h"
#include "scd_proxy.h"

static int init(void);
static void finalize(void);

orcm_scd_base_module_t orcm_scd_proxy_module = {
    init,
    finalize,
    NULL,
    NULL
};


static int init(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:proxy:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:proxy:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
}
