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

#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/regex.h"
#include "orte/mca/rml/rml.h"

#include "orcm/mca/scd/base/base.h"
#include "orcm/mca/rm/base/base.h"
#include "scd_fifo.h"

static int init(void);
static void finalize(void);


orcm_scd_base_module_t orcm_scd_fifo_module = {
    init,
    finalize,
    NULL  // never directly called inside scheduler
};

static void fifo_undef(int sd, short args, void *cbdata);
static void fifo_find_queue(int sd, short args, void *cbdata);
static void fifo_schedule(int sd, short args, void *cbdata);
static void fifo_allocated(int sd, short args, void *cbdata);
static void fifo_terminated(int sd, short args, void *cbdata);
static void fifo_cancel(int sd, short args, void *cbdata);

static orcm_scd_session_state_t states[] = {
    ORCM_SESSION_STATE_UNDEF,
    ORCM_SESSION_STATE_INIT,
    ORCM_SESSION_STATE_SCHEDULE,
    ORCM_SESSION_STATE_ALLOCD,
    ORCM_SESSION_STATE_TERMINATED,
    ORCM_SESSION_STATE_CANCEL
};
static orcm_scd_state_cbfunc_t callbacks[] = {
    fifo_undef,
    fifo_find_queue,
    fifo_schedule,
    fifo_allocated,
    fifo_terminated,
    fifo_cancel
};

static int init(void)
{
    int i, rc;
    int num_states;

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:fifo:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* start the receive */
    if (ORCM_SUCCESS != (rc = orcm_scd_base_comm_start())) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* define our state machine */
    num_states = sizeof(states) / sizeof(orcm_scd_session_state_t);
    for (i=0; i < num_states; i++) {
        if (ORCM_SUCCESS != (rc = orcm_scd_base_add_session_state(states[i],
                                                                    callbacks[i],
                                                                    ORTE_SYS_PRI))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:fifo:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    orcm_scd_base_comm_stop();
}


static void fifo_undef(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    /* this isn't defined - so just report the error */
    opal_output(0, "%s UNDEF SCHEDULER STATE CALLED",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    OBJ_RELEASE(caddy);
}

static void fifo_find_queue(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    orcm_queue_t *q;

    /* cycle across the queues and select the one that best
     * fits this session request.  for FIFO, its just the 
     * default always.
     */

    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        if (0 == strcmp(q->name, "default")) {
            caddy->session->alloc->queues = strdup(q->name);
            opal_list_append(&q->sessions, &caddy->session->super);
            ORCM_ACTIVATE_SCD_STATE(caddy->session, ORCM_SESSION_STATE_SCHEDULE);

            OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                                 "%s scd:fifo:find_queue %s\n",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), q->name));
            break;
        }
    }

    OBJ_RELEASE(caddy);
}

static void fifo_schedule(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    orcm_session_t *sessionptr;
    orcm_node_t* nodeptr;
    int i, free_nodes;

    orcm_queue_t *q;

    /* search the queues for the next allocation to be scheduled */

    /* find the default queue */
    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        /* found it */
        if (0 == strcmp(q->name, "default")) {
            /* if its empty, we are done */
            if (opal_list_is_empty(&q->sessions)) {
                OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                                     "%s scd:fifo:schedule - no (more) sessions found on queue\n",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
                return;
            }
            /* as per FIFO rules, get the first session on the queue */
            sessionptr = (orcm_session_t*)opal_list_remove_first(&q->sessions);

            /* find out how many nodes are available */
            free_nodes = 0;
            if (orcm_scd_base.nodes.number_free > 0) {
                for (i = 0; i < orcm_scd_base.nodes.size; i++) {
                    if (NULL == (nodeptr = (orcm_node_t*)opal_pointer_array_get_item(&orcm_scd_base.nodes, i))) {
                        continue;
                    }
                    /* TODO need to add logic for partially allocated nodes */
                    /* TODO need to add interface to query rm status for external rm compat
                     * getting rm state from nodeptr->state is cheating
                     * it should be safe since its only reading it though, right? */
                    /* TODO check for other constraints, but how? */
                    if (ORCM_SCD_NODE_STATE_UNALLOC == nodeptr->scd_state && ORCM_NODE_STATE_UP == nodeptr->state) {
                        free_nodes++;
                    }
                }
            }

            /* if there are enough nodes to meet job requirement, allocate them */
            if (sessionptr->alloc->min_nodes <= free_nodes) {
                OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                                     "%s scd:fifo:schedule - found enough nodes, activiating session\n",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
                ORCM_ACTIVATE_RM_STATE(sessionptr, ORCM_SESSION_STATE_REQ);
            } else {
                OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                                     "%s scd:fifo:schedule - (session: %d) not enough free nodes (required: %d found: %d), re-queueing session\n",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), sessionptr->id, sessionptr->alloc->min_nodes, free_nodes));
                opal_list_prepend(&q->sessions, &sessionptr->super);
            }
        }
    }

    OBJ_RELEASE(caddy);
}

static void fifo_allocated(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    char **nodenames = NULL;
    int rc, num_nodes, i, j;
    orcm_node_t *nodeptr;
    orcm_queue_t *q;

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:fifo:allocated - (session: %d) got nodelist %s\n",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), 
                         caddy->session->id,
                         caddy->session->alloc->nodes));

    /* put session on running queue
     */
    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        if (0 == strcmp(q->name, "running")) {
            caddy->session->alloc->queues = strdup(q->name);
            opal_list_append(&q->sessions, &caddy->session->super);
            break;
        }
    }

    if (0 == strcmp(caddy->session->alloc->nodes, "ERROR")) {
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:fifo:allocated - (session: %d) nodelist came back as ERROR\n",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), caddy->session->id));
        goto ERROR;
    }

    ORCM_ACTIVATE_RM_STATE(caddy->session, ORCM_SESSION_STATE_ACTIVE);

    /* set nodes to ALLOC
    */
    if (ORTE_SUCCESS != (rc = orte_regex_extract_node_names(caddy->session->alloc->nodes, &nodenames))) {
        ORTE_ERROR_LOG(rc);
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:fifo:allocated - (session: %d) could not extract nodelist\n",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), caddy->session->id));
        goto ERROR;
    }

    num_nodes = opal_argv_count(nodenames);
    if (num_nodes != caddy->session->alloc->min_nodes) {
        /* what happened? we didn't get all of the nodes we needed? */
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:fifo:allocated - (session: %d) nodelist did not contain same number of nodes as requested, expected: %i got: %i\n",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), 
                             caddy->session->id,
                             caddy->session->alloc->min_nodes,
                             num_nodes));
        goto ERROR;
    }

    /* node array should be indexed by node num, if we change to lookup by index that would be faster */
    for (i = 0; i < num_nodes; i++) {
        for (j = 0; j < orcm_scd_base.nodes.size; j++) {
            if (NULL == (nodeptr =
                         (orcm_node_t*)opal_pointer_array_get_item(&orcm_scd_base.nodes, j))) {
                continue;
            }
            if (0 == strcmp(nodeptr->name, nodenames[i])) {
                nodeptr->scd_state = ORCM_SCD_NODE_STATE_ALLOC;
            }
        }
    }

    OBJ_RELEASE(caddy);
    return;

ERROR:
    if (NULL != nodenames) {
        opal_argv_free(nodenames);
    }
    /* remove session from running queue
     */
    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        if (0 == strcmp(q->name, "running")) {
            opal_list_remove_first(&q->sessions);
            break;
        }
    }
    /* try to find the default queue and requeue session */
    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        /* found it */
        if (0 == strcmp(q->name, "default")) {
            opal_list_prepend(&q->sessions, &caddy->session->super);
            ORCM_ACTIVATE_SCD_STATE(caddy->session, ORCM_SESSION_STATE_SCHEDULE);
            break;
        }
    }
}

static void fifo_terminated(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    int rc, i, j, num_nodes;
    orcm_node_t* nodeptr;
    char **nodenames = NULL;
    orcm_queue_t *q;
    orcm_session_t *session;

    /* set nodes to UNALLOC
    */
    if (ORTE_SUCCESS != (rc = orte_regex_extract_node_names(caddy->session->alloc->nodes, &nodenames))) {
        ORTE_ERROR_LOG(rc);
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:fifo:terminated - (session: %d) could not extract nodelist\n",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), caddy->session->id));
        return;
    }

    num_nodes = opal_argv_count(nodenames);

    /* node array should be indexed by node num, if we change to lookup by index that would be faster */
    for (i = 0; i < num_nodes; i++) {
        for (j = 0; j < orcm_scd_base.nodes.size; j++) {
            if (NULL == (nodeptr =
                         (orcm_node_t*)opal_pointer_array_get_item(&orcm_scd_base.nodes, j))) {
                continue;
            }
            if (0 == strcmp(nodeptr->name, nodenames[i])) {
                nodeptr->scd_state = ORCM_SCD_NODE_STATE_UNALLOC;
            }
        }
    }

    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        if (0 == strcmp(q->name, "running")) {
            OPAL_LIST_FOREACH(session, &q->sessions, orcm_session_t) {
                if (session->id == caddy->session->id) {
                    opal_list_remove_item(&q->sessions, &session->super);
                }
            }
            break;
        }
    }

    ORCM_ACTIVATE_SCD_STATE(caddy->session, ORCM_SESSION_STATE_SCHEDULE);

    OBJ_RELEASE(caddy);
}

static void fifo_cancel(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;

    /* if session is queued, find it and delete it */
    /* if session is running, send cancel launch command */
    /* if session is neither, return */

    OBJ_RELEASE(caddy);
}
