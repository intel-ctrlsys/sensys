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

#include "orcm/mca/pvsn/base/base.h"
#include "pvsn_ww.h"

static int init(void);
static void finalize(void);
static int avail(opal_list_t *images);
static int status(char *nodes, opal_list_t *images);
static int provision(char *nodes,
                     char *image,
                     opal_list_t *attributes);

orcm_pvsn_base_module_t orcm_pvsn_wwulf_module = {
    init,
    finalize,
    avail,
    status,
    provision
};

static int init(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
}

static int avail(opal_list_t *images)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:avail",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    return ORCM_ERR_NOT_IMPLEMENTED;
}

static int status(char *nodes, opal_list_t *images)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:status",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    return ORCM_ERR_NOT_IMPLEMENTED;
}

static int provision(char *nodes,
                     char *image,
                     opal_list_t *attributes)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:provision",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    return ORCM_ERR_NOT_IMPLEMENTED;
}
