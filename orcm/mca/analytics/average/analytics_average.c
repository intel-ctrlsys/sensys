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

#include <stdio.h>
#include <ctype.h>

#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/analytics/base/base.h"
#include "analytics_average.h"

static int init(struct orcm_analytics_base_module_t *imod);
static void finalize(struct orcm_analytics_base_module_t *imod);

mca_analytics_average_module_t orcm_analytics_average_module = {
    {
        init,
        finalize
    }
};

static int init(struct orcm_analytics_base_module_t *imod)
{
    return ORCM_SUCCESS;
}

static void finalize(struct orcm_analytics_base_module_t *imod)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                         "%s analytics:average:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
}
