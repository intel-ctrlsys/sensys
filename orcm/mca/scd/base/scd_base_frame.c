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
#include "opal/util/fd.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/runtime/orcm_progress.h"
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
orcm_scd_API_module_t orcm_scd = {
    orcm_scd_base_activate_session_state
};
orcm_scd_base_t orcm_scd_base;

/* local vars */
static void* progress_thread_engine(opal_object_t *obj);
orcm_session_id_t last_session_id = 0;

int orcm_scd_base_get_next_session_id() {
    last_session_id++;
    return last_session_id;
}

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
    if (orcm_scd_base.ev_active) {
        orcm_scd_base.ev_active = false;
        /* stop the thread */
        orcm_stop_progress_thread("scd", true);
    }

    /* deconstruct the base objects */
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

    /* setup the base objects */
    orcm_scd_base.ev_active = false;
    OBJ_CONSTRUCT(&orcm_scd_base.states, opal_list_t);
    OBJ_CONSTRUCT(&orcm_scd_base.queues, opal_list_t);
    OBJ_CONSTRUCT(&orcm_scd_base.nodes, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_scd_base.nodes, 8, INT_MAX, 8);

    if (OPAL_SUCCESS !=
        (rc = mca_base_framework_components_open(&orcm_scd_base_framework,
                                                 flags))) {
        return rc;
    }

    /* create the event base */
    orcm_scd_base.ev_active = true;
    if (NULL ==
        (orcm_scd_base.ev_base = orcm_start_progress_thread("scd",
                                                            progress_thread_engine))) {
        orcm_scd_base.ev_active = false;
        return ORCM_ERR_OUT_OF_RESOURCE;
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

const char *orcm_scd_session_state_to_str(orcm_scd_session_state_t state)
{
    char *s;

    switch (state) {
    case ORCM_SESSION_STATE_UNDEF:
        s = "UNDEF";
        break;
    case ORCM_SESSION_STATE_INIT:
        s = "PENDING QUEUE ASSIGNMENT";
        break;
    case ORCM_SESSION_STATE_SCHEDULE:
        s = "RUNNING SCHEDULERS";
        break;
    case ORCM_SESSION_STATE_ALLOCD:
        s = "ALLOCATED";
        break;
    case ORCM_SESSION_STATE_TERMINATED:
        s = "TERMINATED";
        break;
    case ORCM_SESSION_STATE_CANCEL:
        s = "CANCEL";
        break;
    default:
        s = "UNKNOWN";
    }
    return s;
}

const char *orcm_scd_node_state_to_str(orcm_scd_node_state_t state)
{
    char *s;

    switch (state) {
    case ORCM_SCD_NODE_STATE_UNDEF:
        s = "UNDEF";
        break;
    case ORCM_SCD_NODE_STATE_UNKNOWN:
        s = "UNKNOWN";
        break;     
    case ORCM_SCD_NODE_STATE_UNALLOC:
        s = "UNALLOCATED";
        break;     
    case ORCM_SCD_NODE_STATE_ALLOC:
        s = "ALLOCATED";
        break;
    case ORCM_SCD_NODE_STATE_EXCLUSIVE:
        s = "EXCLUSIVELY ALLOCATED";
        break;
    default:
        s = "STATEUNDEF";
    }
    return s;
}

const char *orcm_rm_session_state_to_str(orcm_rm_session_state_t state)
{
    char *s;
    
    switch (state) {
        case ORCM_SESSION_STATE_UNDEF:
            s = "UNDEF";
            break;
        case ORCM_SESSION_STATE_REQ:
            s = "REQUESTING RESOURCES";
            break;
        case ORCM_SESSION_STATE_ACTIVE:
            s = "LAUNCHING SESSION";
            break;
        case ORCM_SESSION_STATE_KILL:
            s = "KILLING SESSION";
            break;
        default:
            s = "UNKNOWN";
    }
    return s;
}

/****    CLASS INSTANTIATIONS    ****/
OBJ_CLASS_INSTANCE(orcm_scheduler_caddy_t,
                   opal_object_t,
                   NULL, NULL);

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
    p->caller_uid = 0;
    p->caller_gid = 0;
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

    if (NULL != p->alloc) {
        OBJ_RELEASE(p->alloc);
    }
    if (NULL != p->job) {
        OBJ_RELEASE(p->job);
    }
    for (i=0; i < p->nodes.size; i++) {
             /* set node state to unalloc */
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
                   opal_list_item_t,
                   sess_con, sess_des);

static void queue_con(orcm_queue_t *q)
{
    q->name = NULL;
    q->priority = 1;
    OBJ_CONSTRUCT(&q->sessions, opal_list_t);
}
static void queue_des(orcm_queue_t *q)
{
    if (NULL != q->name) {
        free(q->name);
    }
    OPAL_LIST_DESTRUCT(&q->sessions);
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

OBJ_CLASS_INSTANCE(orcm_scd_state_t,
                   opal_list_item_t,
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
