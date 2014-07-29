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

#include "opal/dss/dss.h"
#include "opal/threads/threads.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/runtime/orcm_progress.h"

#include "orcm/mca/analytics/base/base.h"
#include "orcm/mca/analytics/base/analytics_private.h"

static int parse_attributes(opal_value_array_t *attr_array, char *attr_string);
static void* progress_thread_engine(opal_object_t *obj);

static int workflow_id = 0;

static int parse_attributes(opal_value_array_t *attr_array, char *attr_string) {
    return ORCM_SUCCESS;
}

static void* progress_thread_engine(opal_object_t *obj)
{
    opal_thread_t *thread = (opal_thread_t *)obj;
    orcm_workflow_t *wf = (orcm_workflow_t *)thread->t_arg;
    
    while (wf->ev_active) {
        opal_event_loop(wf->ev_base, OPAL_EVLOOP_ONCE);
    }
    return OPAL_THREAD_CANCELLED;
}

int orcm_analytics_base_workflow_create(opal_buffer_t* buffer, int *wfid)
{
    int num_steps, i, cnt, rc;
    char *mod_attrs = NULL;
    orcm_workflow_step_t *wf_step = NULL;
    orcm_workflow_t *wf;
    char *threadname = NULL;
    
    /* unpack the number of steps */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &num_steps,
                                              &cnt, OPAL_INT))) {
        goto error;
    }
    
    wf = OBJ_NEW(orcm_workflow_t);
    wf->workflow_id = workflow_id;
    workflow_id++;
    *wfid = wf->workflow_id;
    
    /* create new event base for workflow */
    asprintf(&threadname, "wfid%i", wf->workflow_id);
    wf->ev_active = true;
    if (NULL ==
        (wf->ev_base = orcm_start_progress_thread(threadname,
                                                  progress_thread_engine,
                                                  (void *)wf))) {
        wf->ev_active = false;
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    /* create each step (module) of the workflow */
    for (i = 0; i < num_steps; i++) {
        wf_step = OBJ_NEW(orcm_workflow_step_t);
        /* unpack the requested module name */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &wf_step->analytic,
                                                  &cnt, OPAL_STRING))) {
            OBJ_RELEASE(wf_step);
            goto error;
        }
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &mod_attrs,
                                                  &cnt, OPAL_STRING))) {
            OBJ_RELEASE(wf_step);
            goto error;
        }
        
        if (ORCM_SUCCESS !=
            (rc = parse_attributes(&wf_step->attributes, mod_attrs))) {
            OBJ_RELEASE(wf_step);
            goto error;
        }
        
        if (ORCM_SUCCESS !=
            (rc = orcm_analytics_base_select_workflow_step(wf_step))) {
            OBJ_RELEASE(wf_step);
            free(mod_attrs);
            /* skip this module ? */
            /* FIXME log something */
            continue;
        }
        opal_list_append(&wf->steps, &wf_step->super);
        free(mod_attrs);
    }
    
    /* add workflow to the master list of workflows */
    opal_list_append(&orcm_analytics_base.workflows, &wf->super);
    
    return ORCM_SUCCESS;

error:
    ORTE_ERROR_LOG(rc);
    if (mod_attrs) {
        free(mod_attrs);
    }
    if (buffer) {
        OBJ_RELEASE(buffer);
    }
    if (wf) {
        OBJ_RELEASE(wf);
    }
    return rc;
}

int orcm_analytics_base_workflow_delete(int workflow_id)
{
    orcm_workflow_t *wf;
    orcm_workflow_t *next;
    char *threadname = NULL;
    
    OPAL_LIST_FOREACH_SAFE(wf, next, &orcm_analytics_base.workflows, orcm_workflow_t) {
        if (workflow_id == wf->workflow_id) {
            /* stop the event thread */
            asprintf(&threadname, "wfid%i", wf->workflow_id);
            if (wf->ev_active) {
                wf->ev_active = false;
                orcm_stop_progress_thread(threadname, true);
            }
            /* remove workflow from the master list */
            opal_list_remove_item(&orcm_analytics_base.workflows, &wf->super);
            OBJ_RELEASE(wf);
        }
    }
    return ORCM_SUCCESS;
}

int orcm_analytics_base_send_data(opal_value_t *data)
{
    orcm_workflow_t *wf;
    
    OPAL_LIST_FOREACH(wf, &orcm_analytics_base.workflows, orcm_workflow_t) {
        /* OBJ_RETAIN(data), ORCM_ACTIVATE_WORKFLOW_STEP first step */
    }
    return ORCM_SUCCESS;
}
