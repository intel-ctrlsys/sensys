/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/mca/event/event.h"
#include "opal/dss/dss.h"
#include "opal/util/output.h"
#include "opal/runtime/opal_progress_threads.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/analytics/base/base.h"
#include "orcm/mca/analytics/base/analytics_private.h"

/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/analytics/base/static-components.h"

static int orcm_analytics_base_register(mca_base_register_flag_t flags);
static void orcm_analytics_stop_wokflow_step(orcm_workflow_step_t *wf_step);
static void orcm_analytics_stop_wokflow_thread(orcm_workflow_t *wf);
static int orcm_analytics_base_close(void);
static int orcm_analytics_base_open(mca_base_open_flag_t flags);

/*
 * Global variables
 */
orcm_analytics_API_module_t orcm_analytics = {
        orcm_analytics_base_send_data
};
orcm_analytics_base_t orcm_analytics_base;

static int orcm_analytics_base_register(mca_base_register_flag_t flags)
{
    orcm_analytics_base.store_raw_data = true;
    orcm_analytics_base.store_event_data = true;

    (void)mca_base_var_register("orcm", "analytics", "base", "store_raw_data",
                                "store raw data",
                                MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                OPAL_INFO_LVL_9,
                                MCA_BASE_VAR_SCOPE_READONLY,
                                &orcm_analytics_base.store_raw_data);
    (void)mca_base_var_register("orcm", "analytics", "base", "store_event_data",
                                "store event data",
                                MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                OPAL_INFO_LVL_9,
                                MCA_BASE_VAR_SCOPE_READONLY,
                                &orcm_analytics_base.store_event_data);

    return ORCM_SUCCESS;

}

static void orcm_analytics_stop_wokflow_step(orcm_workflow_step_t *wf_step)
{
    orcm_analytics_base_module_t *module = NULL;

    module = (orcm_analytics_base_module_t *)wf_step->mod;
    if(NULL != module) {
        module->finalize(wf_step->mod);
    }
}

static void orcm_analytics_stop_wokflow_thread(orcm_workflow_t *wf)
{
    char *threadname = NULL;

    if (NULL != wf && false != wf->ev_active) {
        wf->ev_active = false;
        /* Stop each Workflow thread */
        asprintf(&threadname, "wfid%i", wf->workflow_id);
        opal_progress_thread_finalize(threadname);
    }
    free(threadname);
}

void orcm_analytics_stop_wokflow(orcm_workflow_t *wf)
{
    orcm_workflow_step_t *wf_step = NULL;

    OPAL_LIST_FOREACH (wf_step, &wf->steps, orcm_workflow_step_t) {
        orcm_analytics_stop_wokflow_step(wf_step);
    }
    orcm_analytics_stop_wokflow_thread(wf);

}

static int orcm_analytics_base_close(void)
{
    orcm_workflow_t *wf = NULL;

    OPAL_LIST_FOREACH (wf, &orcm_analytics_base.workflows, orcm_workflow_t) {
        orcm_analytics_stop_wokflow(wf);
    }
    orcm_analytics_base_comm_stop();

    /* Destroy the base objects */
    OPAL_LIST_DESTRUCT(&orcm_analytics_base.workflows);

    /* close the database handle */
    orcm_analytics_base_close_db();

    return mca_base_framework_components_close(&orcm_analytics_base_framework,
                                               NULL);
}

/*
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_analytics_base_open(mca_base_open_flag_t flags)
{
    int rc;

    /* setup the base objects */
    OBJ_CONSTRUCT(&orcm_analytics_base.workflows, opal_list_t);

    rc = mca_base_framework_components_open(&orcm_analytics_base_framework, flags);
    if (OPAL_SUCCESS != rc) {
        return rc;
    }
    
    /* start the receive */
    if (ORCM_SUCCESS != (rc = orcm_analytics_base_comm_start())) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    /* open the database handle */
    orcm_analytics_base_open_db();

    return rc;
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, analytics, "ORCM Analytics", orcm_analytics_base_register,
                           orcm_analytics_base_open, orcm_analytics_base_close,
                           mca_analytics_base_static_components, 0);

/****    INSTANCE CLASSES    ****/
static void wkstep_con(orcm_workflow_step_t *p)
{
    OBJ_CONSTRUCT(&p->attributes, opal_list_t);
    p->analytic = NULL;
    p->mod = NULL;
}
static void wkstep_des(orcm_workflow_step_t *p)
{
    if (NULL == p) {
       return;
    }
    OPAL_LIST_DESTRUCT(&p->attributes);
    free(p->analytic);
}
OBJ_CLASS_INSTANCE(orcm_workflow_step_t,
                   opal_list_item_t,
                   wkstep_con, wkstep_des);

static void wk_con(orcm_workflow_t *p)
{
    p->name = NULL;
    OBJ_CONSTRUCT(&p->steps, opal_list_t);
    p->ev_base = NULL;
}
static void wk_des(orcm_workflow_t *p)
{
    if (NULL == p) {
        return;
    }
    if (NULL != p->ev_base) {
        orcm_analytics_stop_wokflow_thread(p);
    }
    free(p->name);
    OPAL_LIST_DESTRUCT(&p->steps);
}
OBJ_CLASS_INSTANCE(orcm_workflow_t,
                   opal_list_item_t,
                   wk_con, wk_des);

static void analytics_value_con(orcm_analytics_value_t *p)
{
    p->key = NULL;
    p->non_compute_data = NULL;
    p->compute_data = NULL;
}
static void analytics_value_des(orcm_analytics_value_t *p)
{
    if (NULL == p) {
        return;
    }

    if ( NULL != p->key) {
        OPAL_LIST_RELEASE(p->key);
    }
    if ( NULL != p->non_compute_data) {
        OPAL_LIST_RELEASE(p->non_compute_data);
    }
    if ( NULL != p->non_compute_data) {
        OPAL_LIST_RELEASE(p->compute_data);
    }
}
OBJ_CLASS_INSTANCE(orcm_analytics_value_t,
                   opal_object_t,
                   analytics_value_con, analytics_value_des);

static void wkcaddy_con(orcm_workflow_caddy_t *p)
{
    p->wf = NULL;
    p->wf_step = NULL;
    p->analytics_value = NULL;
    p->imod = NULL;
}
static void wkcaddy_des(orcm_workflow_caddy_t *p)
{
    if (NULL == p) {
        return;
    }
    if (NULL != p->wf) {
        OBJ_RELEASE(p->wf);
    }
    if (NULL != p->wf_step) {
        OBJ_RELEASE(p->wf_step);
    }
    if (NULL != p->analytics_value) {
        OBJ_RELEASE(p->analytics_value);
    }
}
OBJ_CLASS_INSTANCE(orcm_workflow_caddy_t,
                   opal_object_t,
                   wkcaddy_con, wkcaddy_des);
