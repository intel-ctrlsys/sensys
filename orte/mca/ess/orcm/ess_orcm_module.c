/*
 * Copyright (c) 2013-2014 Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 */

#include "orte_config.h"
#include "orte/constants.h"

#include "orte/util/proc_info.h"

#include "orte/mca/ess/ess.h"
#include "orte/mca/ess/base/base.h"
#include "orte/mca/ess/orcm/ess_orcm.h"

static int rte_init(void);
static int rte_finalize(void);
static void rte_abort(int status, bool report);

orte_ess_base_module_t orte_ess_orcm_module = {
    rte_init,
    rte_finalize,
    rte_abort,
    NULL /* ft_event */
};

static int rte_init(void)
{
    return ORTE_SUCCESS;
}

static int rte_finalize(void)
{
    return ORTE_SUCCESS;
}


void rte_abort(int status, bool report)
{
    /* - Clean out the global structures 
     * (not really necessary, but good practice) */
    orte_proc_info_finalize();
    
    /* Now Exit */
    exit(status);
}

