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
#include "opal/threads/threads.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/scd/base/base.h"


/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/scd/base/static-components.h"

/*
 * Global variables
 */
orcm_scd_API_module_t orcm_sched = {
    orcm_sched_base_activate_session_state
};
orcm_scd_base_t orcm_scd_base;

/* local vars */
static opal_thread_t progress_thread;
static void* progress_thread_engine(opal_object_t *obj);
static bool progress_thread_running = false;

static int orcm_scd_base_register(mca_base_register_flag_t flags)
{
    /* do we want to just test the scheduler? */
    orcm_scd_base.test_mode = false;
    (void) mca_base_var_register("orcm", "scd", "base", "test_mode",
                                 "Test the ORCM scheduler",
                                 MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                 OPAL_INFO_LVL_9,
                                 MCA_BASE_VAR_SCOPE_READONLY,
                                 &orcm_scd_base.test_mode);
    return OPAL_SUCCESS;
}

static int orcm_scd_base_close(void)
{
    if (progress_thread_running) {
        orcm_scd_base.ev_active = false;
        /* break the event loop */
        opal_event_base_loopbreak(orcm_scd_base.ev_base);
        /* wait for thread to exit */
        opal_thread_join(&progress_thread, NULL);
        OBJ_DESTRUCT(&progress_thread);
        progress_thread_running = false;
        opal_event_base_free(orcm_scd_base.ev_base);
    }

    /* deconstruct the base objects */
    OPAL_LIST_DESTRUCT(&orcm_scd_base.active_modules);
    OPAL_LIST_DESTRUCT(&orcm_scd_base.states);
    OPAL_LIST_DESTRUCT(&orcm_scd_base.queues);

    return mca_base_framework_components_close(&orcm_scd_base_framework, NULL);
}

/**
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_scd_base_open(mca_base_open_flag_t flags)
{
    int rc;
    opal_data_type_t tmp;

    /* setup the base objects */
    orcm_scd_base.ev_active = false;
    OBJ_CONSTRUCT(&orcm_scd_base.states, opal_list_t);
    OBJ_CONSTRUCT(&orcm_scd_base.active_modules, opal_list_t);
    OBJ_CONSTRUCT(&orcm_scd_base.queues, opal_list_t);
    OBJ_CONSTRUCT(&orcm_scd_base.nodes, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_scd_base.nodes, 8, INT_MAX, 8);

    if (OPAL_SUCCESS != (rc = mca_base_framework_components_open(&orcm_scd_base_framework, flags))) {
        return rc;
    }

    /* register the scheduler types for packing/unpacking services */
    tmp = ORCM_ALLOC;
    if (OPAL_SUCCESS != (rc = opal_dss.register_type(orcm_pack_alloc,
                                                     orcm_unpack_alloc,
                                                     (opal_dss_copy_fn_t)orcm_copy_alloc,
                                                     (opal_dss_compare_fn_t)orcm_compare_alloc,
                                                     (opal_dss_print_fn_t)orcm_print_alloc,
                                                     OPAL_DSS_STRUCTURED,
                                                     "ORCM_ALLOC", &tmp))) {
        ORTE_ERROR_LOG(rc);
    }

    /* create the event base */
    orcm_scd_base.ev_base = opal_event_base_create();
    orcm_scd_base.ev_active = true;
    /* construct the thread object */
    OBJ_CONSTRUCT(&progress_thread, opal_thread_t);
    /* fork off a thread to progress it */
    progress_thread.t_run = progress_thread_engine;
    progress_thread_running = true;
    if (OPAL_SUCCESS != (rc = opal_thread_start(&progress_thread))) {
        ORTE_ERROR_LOG(rc);
        progress_thread_running = false;
    }

    return rc;
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, scd, NULL, orcm_scd_base_register,
                           orcm_scd_base_open, orcm_scd_base_close,
                           mca_scd_base_static_components, 0);

static void* progress_thread_engine(opal_object_t *obj)
{
    while (orcm_scd_base.ev_active) {
        opal_event_loop(orcm_scd_base.ev_base, OPAL_EVLOOP_ONCE);
    }
    return OPAL_THREAD_CANCELLED;
}

const char *orcm_session_state_to_str(orcm_session_state_t state)
{
    char *s;

    switch (state) {
    case ORCM_SESSION_STATE_UNDEF:
        s = "UNDEF";
        break;
    case ORCM_SESSION_STATE_INIT:
        s = "PENDING QUEUE ASSIGNMENT";
        break;
    case ORCM_SESSION_STATE_QUEUED:
        s = "QUEUED";
        break;
    case ORCM_SESSION_STATE_ALLOCD:
        s = "ALLOCATED";
        break;
    case ORCM_SESSION_STATE_ACTIVE:
        s = "ACTIVE";
        break;
    case ORCM_SESSION_STATE_TERMINATED:
        s = "TERMINATED";
        break;
    default:
        s = "UNKNOWN";
    }
    return s;
}

/****    CLASS INSTANTIATIONS    ****/
static void scd_des(orcm_scd_base_active_module_t *s)
{
    if (NULL != s->module->finalize) {
        s->module->finalize();
    }
}
OBJ_CLASS_INSTANCE(orcm_scd_base_active_module_t,
                   opal_list_item_t,
                   NULL, scd_des);

OBJ_CLASS_INSTANCE(orcm_scheduler_caddy_t,
                   opal_object_t,
                   NULL, NULL);

static void res_con(orcm_resource_t *p)
{
    p->constraint = NULL;
}
static void res_des(orcm_resource_t *p)
{
    if (NULL != p->constraint) {
        free(p->constraint);
    }
}
OBJ_CLASS_INSTANCE(orcm_resource_t,
                   opal_list_item_t,
                   res_con, res_des);

static void alloc_con(orcm_alloc_t *p)
{
    p->priority = 0;
    p->account = NULL;
    p->name = NULL;
    p->gid = 0;
    p->max_nodes = 0;
    p->max_pes = 0;
    p->min_nodes = 0;
    p->min_pes = 0;
    memset(&p->begin, 0, sizeof(time_t));
    memset(&p->walltime, 0, sizeof(time_t));
    p->exclusive = true;
    p->nodefile = NULL;
    p->nodes = NULL;
    p->queues = NULL;
    OBJ_CONSTRUCT(&p->constraints, opal_list_t);
}
static void alloc_des(orcm_alloc_t *p)
{
    if (NULL != p->account) {
        free(p->account);
    }
    if (NULL != p->name) {
        free(p->name);
    }
    if (NULL != p->nodefile) {
        free(p->nodefile);
    }
    if (NULL != p->nodes) {
        free(p->nodes);
    }
    if (NULL != p->queues) {
        free(p->queues);
    }
    OPAL_LIST_DESTRUCT(&p->constraints);
}
OBJ_CLASS_INSTANCE(orcm_alloc_t,
                   opal_object_t,
                   alloc_con, alloc_des);

static void cmn_con(orcm_cmpnode_t *c)
{
    c->node = NULL;
    c->queue = NULL;
}
static void cmn_des(orcm_cmpnode_t *c)
{
    if (NULL != c->node) {
        OBJ_RELEASE(c->node);
    }
    if (NULL != c->queue) {
        OBJ_RELEASE(c->queue);
    }
}
OBJ_CLASS_INSTANCE(orcm_cmpnode_t,
                   opal_list_item_t,
                   cmn_con, cmn_des);

OBJ_CLASS_INSTANCE(orcm_job_t,
                   opal_object_t,
                   NULL, NULL);

static void step_con(orcm_step_t *p)
{
    p->alloc = NULL;
    p->job = NULL;
    OBJ_CONSTRUCT(&p->nodes, opal_pointer_array_t);
    opal_pointer_array_init(&p->nodes, 1, INT_MAX, 8);
}
static void step_des(orcm_step_t *p)
{
    int i;
    orcm_cmpnode_t *n;

    if (NULL != p->alloc) {
        OBJ_RELEASE(p->alloc);
    }
    if (NULL != p->job) {
        OBJ_RELEASE(p->job);
    }
    for (i=0; i < p->nodes.size; i++) {
        if (NULL != (n = (orcm_cmpnode_t*)opal_pointer_array_get_item(&p->nodes, i))) {
            OBJ_RELEASE(n);
        }
    }
    OBJ_DESTRUCT(&p->nodes);
}
OBJ_CLASS_INSTANCE(orcm_step_t,
                   opal_list_item_t,
                   step_con, step_des);

static void sess_con(orcm_session_t *s)
{
    s->alloc = NULL;
    OBJ_CONSTRUCT(&s->steps, opal_list_t);
}
static void sess_des(orcm_session_t *s)
{
    if (NULL != s->alloc) {
        OBJ_RELEASE(s->alloc);
    }
    OPAL_LIST_DESTRUCT(&s->steps);
}
OBJ_CLASS_INSTANCE(orcm_session_t,
                   opal_object_t,
                   sess_con, sess_des);

static void queue_con(orcm_queue_t *q)
{
    q->name = NULL;
    OBJ_CONSTRUCT(&q->nodes, opal_pointer_array_t);
    opal_pointer_array_init(&q->nodes, 1, INT_MAX, 8);
    OBJ_CONSTRUCT(&q->all_sessions, opal_pointer_array_t);
    opal_pointer_array_init(&q->all_sessions, 1, INT_MAX, 8);
    OBJ_CONSTRUCT(&q->power_binned, opal_pointer_array_t);
    opal_pointer_array_init(&q->power_binned, 1, INT_MAX, 8);
    OBJ_CONSTRUCT(&q->node_binned, opal_pointer_array_t);
    opal_pointer_array_init(&q->node_binned, 1, INT_MAX, 8);
}
static void queue_des(orcm_queue_t *q)
{
    orcm_session_t *s;
    orcm_cmpnode_t *n;
    int i;

    if (NULL != q->name) {
        free(q->name);
    }
    for (i=0; i < q->nodes.size; i++) {
        if (NULL != (n = (orcm_cmpnode_t*)opal_pointer_array_get_item(&q->nodes, i))) {
            OBJ_RELEASE(n);
        }
    }
    OBJ_DESTRUCT(&q->nodes);
    for (i=0; i < q->all_sessions.size; i++) {
        if (NULL != (s = (orcm_session_t*)opal_pointer_array_get_item(&q->all_sessions, i))) {
            OBJ_RELEASE(s);
        }
    }
    OBJ_DESTRUCT(&q->all_sessions);
    for (i=0; i < q->power_binned.size; i++) {
        if (NULL != (s = (orcm_session_t*)opal_pointer_array_get_item(&q->power_binned, i))) {
            OBJ_RELEASE(s);
        }
    }
    OBJ_DESTRUCT(&q->power_binned);
    for (i=0; i < q->node_binned.size; i++) {
        if (NULL != (s = (orcm_session_t*)opal_pointer_array_get_item(&q->node_binned, i))) {
            OBJ_RELEASE(s);
        }
    }
    OBJ_DESTRUCT(&q->node_binned);
}
OBJ_CLASS_INSTANCE(orcm_queue_t,
                   opal_list_item_t,
                   queue_con, queue_des);

static void cd_con(orcm_session_caddy_t *p)
{
    p->session = NULL;
}
static void cd_des(orcm_session_caddy_t *p)
{
    if (NULL != p->session) {
        OBJ_RELEASE(p->session);
    }
}
OBJ_CLASS_INSTANCE(orcm_session_caddy_t,
                   opal_object_t,
                   cd_con, cd_des);

OBJ_CLASS_INSTANCE(orcm_state_t,
                   opal_list_item_t,
                   NULL, NULL);
