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

#include "orcm/mca/diag/base/base.h"
#include "diag_pwr.h"

static int init(void);
static void finalize(void);
static void calibrate(void);

orcm_diag_base_module_t orcm_diag_pwr_module = {
    init,
    finalize,
    calibrate
};

static int init(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_diag_base_framework.framework_output,
                         "%s diag:pwr:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_diag_base_framework.framework_output,
                         "%s diag:pwr:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
}

static void calibrate(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_diag_base_framework.framework_output,
                         "%s diag:pwr:calibrate",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
}
