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

#ifndef MCA_ANALYTICS_PRIVATE_H
#define MCA_ANALYTICS_PRIVATE_H


BEGIN_C_DECLS

ORCM_DECLSPEC int orcm_analytics_base_workflow_create(opal_buffer_t* buffer, int *wfid);
ORCM_DECLSPEC int orcm_analytics_base_workflow_delete(int workflow_id);
ORCM_DECLSPEC int orcm_analytics_base_comm_start(void);
ORCM_DECLSPEC int orcm_analytics_base_comm_stop(void);
ORCM_DECLSPEC int orcm_analytics_base_select_workflow_step(orcm_workflow_step_t *workflow);

END_C_DECLS
#endif
