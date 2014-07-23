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
#include "orcm/types.h"

#include "opal/util/output.h"

#include "orcm/mca/analytics/base/base.h"
#include "orcm/mca/analytics/base/analytics_private.h"

int orcm_analytics_base_workflow_create()
{
    /* create event base */
    /* for each module in the workflow
     *     create module handle
     *     initialize with attributes
     *     add to list
     */
    return ORCM_SUCCESS;
}

int orcm_analytics_base_workflow_delete(int workflow_id)
{
    orcm_workflow_t *wf;
    
    OPAL_LIST_FOREACH(wf, &orcm_analytics_base.workflows, orcm_workflow_t) {
        if (workflow_id == wf->workflow_id) {
            /* delete the workflow */
        }
    }
    return ORCM_SUCCESS;
}
