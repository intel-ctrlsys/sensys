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

#include "orcm/mca/analytics/base/base.h"

BEGIN_C_DECLS

ORCM_DECLSPEC int orcm_analytics_base_workflow_create(opal_buffer_t* buffer, int *wfid);
ORCM_DECLSPEC int orcm_analytics_base_workflow_delete(int workflow_id);
ORCM_DECLSPEC int orcm_analytics_base_comm_start(void);
ORCM_DECLSPEC int orcm_analytics_base_comm_stop(void);
ORCM_DECLSPEC int orcm_analytics_base_select_workflow_step(orcm_workflow_step_t *workflow);
ORCM_DECLSPEC void orcm_analytics_stop_wokflow(orcm_workflow_t *wf);
ORCM_DECLSPEC void orcm_analytics_base_activate_analytics_workflow_step(orcm_workflow_t *wf,
                                                                        orcm_workflow_step_t *wf_step,
                                                                        opal_value_array_t *data);
ORCM_DECLSPEC void orcm_analytics_base_send_data(opal_value_array_t *data);


#define ANALYTICS_COUNT_DEFAULT 1
#define MAX_ALLOWED_ATTRIBUTES_PER_WORKFLOW_STEP 2

typedef struct {
    /* list of active workflows */
    opal_list_t workflows;
} orcm_analytics_base_wf_t;
ORCM_DECLSPEC extern orcm_analytics_base_wf_t orcm_analytics_base_wf;


#define ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(wf, prev_wf_step, data)                   \
    do {                                                                           \
        opal_list_item_t *list_item = opal_list_get_next(prev_wf_step);            \
        if (NULL == list_item){                                                    \
            opal_output_verbose(1, orcm_analytics_base_framework.framework_output, \
                                "%s TRIED TO ACTIVATE EMPTY WORKFLOW %d AT %s:%d", \
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),                \
                                (wf)->workflow_id,                                 \
                                __FILE__, __LINE__);                               \
            OBJ_RELEASE(data);                                                     \
            break;                                                                 \
        }                                                                          \
        orcm_workflow_step_t *wf_step_item = (orcm_workflow_step_t *)list_item;    \
        if (list_item == opal_list_get_end(&(wf->steps))) {                        \
            opal_output_verbose(1, orcm_analytics_base_framework.framework_output, \
                                "%s END OF WORKFLOW %d AT %s:%d",                  \
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),                \
                                (wf)->workflow_id,                                 \
                                __FILE__, __LINE__);                               \
            OBJ_RELEASE(data);                                                     \
        } else {                                                                   \
            opal_output_verbose(1, orcm_analytics_base_framework.framework_output, \
                                "%s ACTIVATE NEXT WORKFLOW %d MODULE %s AT %s:%d", \
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),                \
                                (wf)->workflow_id, wf_step_item->analytic,         \
                                __FILE__, __LINE__);                               \
            orcm_analytics_base_activate_analytics_workflow_step((wf),             \
                                                            wf_step_item,          \
                                                            (data));               \
        }                                                                          \
    }while(0);

END_C_DECLS
#endif
