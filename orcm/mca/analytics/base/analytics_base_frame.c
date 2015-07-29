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

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/analytics/base/base.h"
#include "orcm/mca/analytics/base/analytics_private.h"

#include "orcm/runtime/orcm_progress.h"

/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/analytics/base/static-components.h"

static void orcm_analytics_stop_wokflow_step(orcm_workflow_step_t *wf_step);
static void orcm_analytics_stop_wokflow_thread(orcm_workflow_t *wf);
static int orcm_analytics_base_close(void);
static int orcm_analytics_base_open(mca_base_open_flag_t flags);
/*
 * Global variables
 */
orcm_analytics_API_module_t orcm_analytics = {
    orcm_analytics_base_array_create,
    orcm_analytics_base_array_append,
    orcm_analytics_base_array_cleanup,
    orcm_analytics_base_array_send
};
orcm_analytics_base_wf_t orcm_analytics_base_wf;

static void orcm_analytics_stop_wokflow_step(orcm_workflow_step_t *wf_step)
{
    orcm_analytics_base_module_t *module = NULL;

    module = (orcm_analytics_base_module_t *)wf_step->mod;
    module->finalize(wf_step->mod);
}

static void orcm_analytics_stop_wokflow_thread(orcm_workflow_t *wf)
{
    char *threadname = NULL;

    if (NULL != wf && false != wf->ev_active) {
        wf->ev_active = false;
        /* Stop each Workflow thread */
        asprintf(&threadname, "wfid%i", wf->workflow_id);
        orcm_stop_progress_thread(threadname, true);
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

    OPAL_LIST_FOREACH (wf, &orcm_analytics_base_wf.workflows, orcm_workflow_t) {
        orcm_analytics_stop_wokflow(wf);
    }
    orcm_analytics_base_comm_stop();

    /* Destroy the base objects */
    OPAL_LIST_DESTRUCT(&orcm_analytics_base_wf.workflows);

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
    OBJ_CONSTRUCT(&orcm_analytics_base_wf.workflows, opal_list_t);

    rc = mca_base_framework_components_open(&orcm_analytics_base_framework, flags);
    if (OPAL_SUCCESS != rc) {
        return rc;
    }
    
    /* start the receive */
    if (ORCM_SUCCESS != (rc = orcm_analytics_base_comm_start())) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    return rc;
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, analytics, NULL, NULL,
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

static void wkcaddy_con(orcm_workflow_caddy_t *p)
{
    p->wf = NULL;
    p->wf_step = NULL;
    p->data = NULL;
    p->imod = NULL;
}
static void wkcaddy_des(orcm_workflow_caddy_t *p)
{
    if (NULL == p) {
        return;
    }
    OBJ_RELEASE(p->wf);
    OBJ_RELEASE(p->wf_step);
    OBJ_RELEASE(p->data);
}
OBJ_CLASS_INSTANCE(orcm_workflow_caddy_t,
                   opal_object_t,
                   wkcaddy_con, wkcaddy_des);

static void value_con(orcm_analytics_value_t *p)
{
    p->comma_sep_plugin_list = NULL;
    p->sensor_name = NULL;
    p->node_regex = NULL;
}
static void value_des(orcm_analytics_value_t *p)
{
    if (NULL == p) {
        return;
    }
    free(p->comma_sep_plugin_list);
    free(p->sensor_name);
    free(p->node_regex);
}
OBJ_CLASS_INSTANCE(orcm_analytics_value_t,
                   opal_object_t,
                   value_con, value_des);
