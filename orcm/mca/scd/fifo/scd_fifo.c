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

#include "orcm/mca/scd/base/base.h"
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
static void fifo_allocated(int sd, short args, void *cbdata);
static void fifo_active(int sd, short args, void *cbdata);
static void fifo_terminated(int sd, short args, void *cbdata);
static void fifo_schedule(int sd, short args, void *cbdata);

char* fifo_prettyprint_queue(orcm_queue_t* queue);

static orcm_session_state_t states[] = {
    ORCM_SESSION_STATE_UNDEF,
    ORCM_SESSION_STATE_INIT,
    ORCM_SESSION_STATE_SCHEDULE,
    ORCM_SESSION_STATE_ALLOCD,
    ORCM_SESSION_STATE_ACTIVE,
    ORCM_SESSION_STATE_TERMINATED
};
static orcm_state_cbfunc_t callbacks[] = {
    fifo_undef,
    fifo_find_queue,
    fifo_schedule,
    fifo_allocated,
    fifo_active,
    fifo_terminated
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
    num_states = sizeof(states) / sizeof(orcm_session_state_t);
    for (i=0; i < num_states; i++) {
        if (ORCM_SUCCESS != (rc = orcm_sched_base_add_session_state(states[i],
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

    char* printbuf;
    orcm_queue_t *q;

    /* cycle across the queues and select the one that best
     * fits this session request.  for FIFO, its just the 
     * default always.
     */

    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        /* write a conveniece function to add to the string and comma sep */
        if (0 == strcmp(q->name, "default")) {
            caddy->session->alloc->queues = strdup(q->name);
            opal_list_append(&q->sessions, &caddy->session->super);
            ORCM_ACTIVATE_SCHED_STATE(caddy->session, ORCM_SESSION_STATE_SCHEDULE);

            printbuf = fifo_prettyprint_queue(q);
            OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                                 "%s scd:fifo:find_queue\n%s\n",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), printbuf));
            free(printbuf);

            break;
        }
    }

    OBJ_RELEASE(caddy);
}

static void fifo_schedule(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;

    orcm_queue_t *q;

    /* search the queues for the next allocation to be scheduled */

    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        if (0 == strcmp(q->name, "default")) {
            if (opal_list_is_empty(&q->sessions)) {
                OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                                     "%s scd:fifo:find_schedule - no (more) sessions found on queue\n",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
                return;
            }
            caddy->session = (orcm_session_t*)opal_list_remove_first(&q->sessions);
            ORCM_ACTIVATE_SCHED_STATE(caddy->session, ORCM_SESSION_STATE_ALLOCD);
        }
    }

    OBJ_RELEASE(caddy);
}

static void fifo_allocated(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;

    /* inform the allocated daemons of the session so they
     * can launch their step daemons
     */
    ORCM_ACTIVATE_SCHED_STATE(caddy->session, ORCM_SESSION_STATE_ACTIVE);

    OBJ_RELEASE(caddy);
}

static void fifo_active(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;

    /* if an interactive job, report to the caller that this has
     * been allocated and the VM is ready for use
     */

    /* if a batch job, tell the orcmctrld to start executing
     * the batch script
     */
    ORCM_ACTIVATE_SCHED_STATE(caddy->session, ORCM_SESSION_STATE_TERMINATED);
    OBJ_RELEASE(caddy);
}

static void fifo_terminated(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;

    ORCM_ACTIVATE_SCHED_STATE(caddy->session, ORCM_SESSION_STATE_SCHEDULE);

    OBJ_RELEASE(caddy);
}

char* fifo_prettyprint_queue(orcm_queue_t* queue)
{
    orcm_session_t *session;
    char* buffer;

    if (asprintf(&buffer, "current sessions on queue: %s\n", queue->name) == -1) {
        ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
        return buffer;
    }

    OPAL_LIST_FOREACH(session, &queue->sessions, orcm_session_t) {
        if (asprintf(&buffer, "%s%d\n", buffer, (int)session->id) == -1) {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            return buffer;
        }
    }

    return buffer;

}
