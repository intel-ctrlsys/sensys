/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"

#include "opal/mca/base/base.h"
#include "opal/util/net.h"
#include "opal/opal_socket_errno.h"

#include "orte/util/name_fns.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/orte_globals.h"

#include "orte/mca/ras/base/ras_private.h"
#include "ras_orcm.h"


/*
 * Local functions
 */
static int ras_orcm_register(void);
static int ras_orcm_open(void);
static int ras_orcm_close(void);
static int orte_ras_orcm_component_query(mca_base_module_t **module, int *priority);


orte_ras_orcm_component_t mca_ras_orcm_component = {
    {
        /* First, the mca_base_component_t struct containing meta
           information about the component itself */

        {
            ORTE_RAS_BASE_VERSION_2_0_0,

            /* Component name and version */
            "orcm",
            ORTE_MAJOR_VERSION,
            ORTE_MINOR_VERSION,
            ORTE_RELEASE_VERSION,

            /* Component open and close functions */
            ras_orcm_open,
            ras_orcm_close,
            orte_ras_orcm_component_query,
            ras_orcm_register
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        }
    }
};


static int ras_orcm_register(void)
{
    return ORTE_SUCCESS;
}

static int ras_orcm_open(void)
{
    return ORTE_SUCCESS;
}

static int ras_orcm_close(void)
{
    return ORTE_SUCCESS;
}

static int orte_ras_orcm_component_query(mca_base_module_t **module, int *priority)
{
    /*
     * If I am not in a orcm allocation
     * then disqualify myself
     */
    if (NULL == getenv("ORCM_SESSIONID")) {
        /* disqualify ourselves */
        *priority = 0;
        *module = NULL;
        return ORTE_ERROR;
    }

    OPAL_OUTPUT_VERBOSE((2, orte_ras_base_framework.framework_output,
                         "%s ras:orcm: available for selection",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    /* since only one RM can exist on a cluster, just set
     * my priority to something - the other components won't
     * be responding anyway
     */
    *priority = 50;
    *module = (mca_base_module_t *) &orte_ras_orcm_module;
    return ORTE_SUCCESS;
}
