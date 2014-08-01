/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_ANALYTICS_BASE_H
#define MCA_ANALYTICS_BASE_H

/*
 * includes
 */
#include "orcm_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/event/event.h"
#include "opal/dss/dss_types.h"
#include "opal/util/output.h"

#include "orcm/mca/analytics/analytics.h"


BEGIN_C_DECLS

/*
 * MCA framework
 */
ORCM_DECLSPEC extern mca_base_framework_t orcm_analytics_base_framework;

/* sensors send data to framework to pass to each workflow */
ORCM_DECLSPEC int orcm_analytics_base_send_data(opal_value_array_t *data);

typedef struct {
    /* list of active workflows */
    opal_list_t workflows;
} orcm_analytics_base_t;
ORCM_DECLSPEC extern orcm_analytics_base_t orcm_analytics_base;

ORCM_DECLSPEC void orcm_analytics_base_activate_analytics_workflow_step(orcm_workflow_t *wf,
                                                                        orcm_workflow_step_t *wf_step,
                                                                        opal_value_array_t *data);

/* base code stubs */

END_C_DECLS
#endif
