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

#include "orcm/mca/rm/base/base.h"
#include "rm_basic.h"

static int init(void);
static void finalize(void);


orcm_rm_base_module_t orcm_rm_basic_module = {
    init,
    finalize
};

static int init(void)
{
    int i, rc;
    int num_states;

    OPAL_OUTPUT_VERBOSE((5, orcm_rm_base_framework.framework_output,
                         "%s rm:basic:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* start the receive */
    if (ORCM_SUCCESS != (rc = orcm_rm_base_comm_start())) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_rm_base_framework.framework_output,
                         "%s rm:basic:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    orcm_rm_base_comm_stop();
}
