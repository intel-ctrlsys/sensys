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
#include "scd_basic.h"

static int init(void);
static void finalize(void);


orcm_scd_base_module_t orcm_scd_basic_module = {
    init,
    finalize,
    NULL  // never directly called inside scheduler
};

static void basic_undef(int sd, short args, void *cbdata);
static void basic_find_queue(int sd, short args, void *cbdata);
static void basic_allocated(int sd, short args, void *cbdata);
static void basic_active(int sd, short args, void *cbdata);
static void basic_terminated(int sd, short args, void *cbdata);
static void basic_schedule(int sd, short args, void *cbdata);

static orcm_session_state_t states[] = {
    ORCM_SESSION_STATE_UNDEF,
    ORCM_SESSION_STATE_INIT,
    ORCM_SESSION_STATE_ALLOCD,
    ORCM_SESSION_STATE_ACTIVE,
    ORCM_SESSION_STATE_TERMINATED,
    ORCM_SESSION_STATE_SCHEDULE
};
static orcm_state_cbfunc_t callbacks[] = {
    basic_undef,
    basic_find_queue,
    basic_allocated,
    basic_active,
    basic_terminated,
    basic_schedule
};

static int init(void)
{
    int i, rc;
    int num_states;

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:basic:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* start the receive */
    if (ORCM_SUCCESS != (rc = orcm_scd_base_comm_start())) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* BUG: we can't do this in init, because some other module
     * might be trying to do the same thing on selections
     * which is a bug
     */
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
                         "%s scd:basic:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    orcm_scd_base_comm_stop();
}


static void basic_undef(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    /* this isn't defined - so just report the error */
    opal_output(0, "%s UNDEF SCHEDULER STATE CALLED",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    OBJ_RELEASE(caddy);
}

static void basic_find_queue(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;

    /* cycle across the queues and select the one that best
     * fits this session request
     */

    OBJ_RELEASE(caddy);
}

static void basic_allocated(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;

    /* inform the allocated daemons of the session so they
     * can launch their step daemons
     */

    OBJ_RELEASE(caddy);
}

static void basic_active(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;

    /* if an interactive job, report to the caller that this has
     * been allocated and the VM is ready for use
     */

    /* if a batch job, tell the orcmctrld to start executing
     * the batch script
     */
    OBJ_RELEASE(caddy);
}

static void basic_terminated(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    OBJ_RELEASE(caddy);
}

static void basic_schedule(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;

    /* search the queues for the next allocation to be scheduled */

    OBJ_RELEASE(caddy);
}

