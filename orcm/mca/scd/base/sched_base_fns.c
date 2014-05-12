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

#include "opal/mca/mca.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"

#include "orte/types.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/name_fns.h"
#include "orte/util/regex.h"
#include "orte/util/show_help.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/scd/base/base.h"

void orcm_sched_base_activate_session_state(orcm_session_t *session,
                                            orcm_session_state_t state)
{
    orcm_state_t *s, *any=NULL, *error=NULL;
    orcm_session_caddy_t *caddy;

    OPAL_LIST_FOREACH(s, &orcm_scd_base.states, orcm_state_t) {
        if (s->state == ORCM_SESSION_STATE_ANY) {
            /* save this place */
            any = s;
            continue;
        }
        if (s->state == ORCM_SESSION_STATE_ERROR) {
            error = s;
            continue;
        }
        if (s->state == state) {
            if (NULL == s->cbfunc) {
                OPAL_OUTPUT_VERBOSE((1, orcm_scd_base_framework.framework_output,
                                     "%s NULL CBFUNC FOR SESSION %d STATE %s",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     session->id, orcm_session_state_to_str(state)));
                return;
            }
            caddy = OBJ_NEW(orcm_session_caddy_t);
            caddy->session = session;
            OBJ_RETAIN(session);
            opal_event_set(orcm_scd_base.ev_base, &caddy->ev, -1, OPAL_EV_WRITE, s->cbfunc, caddy);
            opal_event_set_priority(&caddy->ev, s->priority);
            opal_event_active(&caddy->ev, OPAL_EV_WRITE, 1);
            return;
        }
    }
    /* if we get here, then the state wasn't found, so execute
     * the default handler if it is defined
     */
    if (ORCM_SESSION_STATE_ERROR < state && NULL != error) {
        s = error;
    } else if (NULL != any) {
        s = any;
    } else {
        OPAL_OUTPUT_VERBOSE((1, orcm_scd_base_framework.framework_output,
                             "ACTIVATE: ANY STATE NOT FOUND"));
        return;
    }
    if (NULL == s->cbfunc) {
        OPAL_OUTPUT_VERBOSE((1, orcm_scd_base_framework.framework_output,
                             "ACTIVATE: ANY STATE HANDLER NOT DEFINED"));
        return;
    }
    caddy = OBJ_NEW(orcm_session_caddy_t);
    caddy->session = session;
    OBJ_RETAIN(session);
    OPAL_OUTPUT_VERBOSE((1, orcm_scd_base_framework.framework_output,
                         "%s ACTIVATING SESSION %d STATE %s PRI %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         session->id, orcm_session_state_to_str(state), s->priority));
    opal_event_set(orcm_scd_base.ev_base, &caddy->ev, -1, OPAL_EV_WRITE, s->cbfunc, caddy);
    opal_event_set_priority(&caddy->ev, s->priority);
    opal_event_active(&caddy->ev, OPAL_EV_WRITE, 1);
}


int orcm_sched_base_add_session_state(orcm_session_state_t state,
                                      orcm_state_cbfunc_t cbfunc,
                                      int priority)
{
    orcm_state_t *st;

    /* check for uniqueness */
    OPAL_LIST_FOREACH(st, &orcm_scd_base.states, orcm_state_t) {
        if (st->state == state) {
            OPAL_OUTPUT_VERBOSE((1, orcm_scd_base_framework.framework_output,
                                 "DUPLICATE STATE DEFINED: %s",
                                 orcm_session_state_to_str(state)));
            return ORCM_ERR_BAD_PARAM;
        }
    }

    st = OBJ_NEW(orcm_state_t);
    st->state = state;
    st->cbfunc = cbfunc;
    st->priority = priority;
    opal_list_append(&orcm_scd_base.states, &st->super);

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "SESSION STATE %s registered",
                         orcm_session_state_to_str(state)));

    return ORCM_SUCCESS;
}

void orcm_scd_base_construct_queues(int fd, short args, void *cbdata)
{
    orcm_scheduler_caddy_t *c = (orcm_scheduler_caddy_t*)cbdata;
    orcm_scheduler_t *scheduler = c->scheduler;
    orcm_queue_t *q, *q2, *def;
    int i;
    char **t1;

    /* push our running queue onto the stack */
    def = OBJ_NEW(orcm_queue_t);
    def->name = strdup("running");
    def->priority = 0;
    opal_list_append(&orcm_scd_base.queues, &def->super);

    /* push our default queue onto the stack */
    def = OBJ_NEW(orcm_queue_t);
    def->name = strdup("default");
    def->priority = 0;
    opal_list_append(&orcm_scd_base.queues, &def->super);

    /* now create queues as defined in the config */
    if (NULL != scheduler->queues) {
        for (i=0; NULL != scheduler->queues[i]; i++) {
            /* split on the colon delimiters */
            t1 = opal_argv_split(scheduler->queues[i], ':');
            /* must have three entries */
            if (3 != opal_argv_count(t1)) {
                opal_argv_free(t1);
                OBJ_RELEASE(c);
                return;
            }
            /* create the object */
            q = OBJ_NEW(orcm_queue_t);
            /* first position is the queue name */
            q->name = strdup(t1[0]);
            /* second is the priority */
            q->priority = strtol(t1[1], NULL, 10);
            /* insert this queue in priority order from highest
             * to lowest priority - we know the default queue
             * will always be at the bottom
             */
            OPAL_LIST_FOREACH(q2, &orcm_scd_base.queues, orcm_queue_t) {
                if (q->priority > q2->priority) {
                    opal_list_insert_pos(&orcm_scd_base.queues,
                                         &q2->super, &q->super);
                    break;
                }
            }
        }
    }

    if (4 < opal_output_get_verbosity(orcm_scd_base_framework.framework_output)) {
        /* print out the queue structure */
        OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
            opal_output(0, "QUEUE: %s PRI: %d", q->name, q->priority);
        }
    }
}
