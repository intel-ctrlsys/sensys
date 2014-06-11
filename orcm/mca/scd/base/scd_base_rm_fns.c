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

void orcm_scd_base_rm_activate_session_state(orcm_session_t *session,
                                             orcm_scd_base_rm_session_state_t state)
{
    orcm_scd_base_rm_state_t *s, *any=NULL, *error=NULL;
    orcm_session_caddy_t *caddy;

    OPAL_LIST_FOREACH(s, &orcm_scd_base.rmstates, orcm_scd_base_rm_state_t) {
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
                                     "%s NULL CBFUNC FOR RM SESSION %d STATE %s",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     session->id,
                                     orcm_rm_session_state_to_str(state)));
                return;
            }
            caddy = OBJ_NEW(orcm_session_caddy_t);
            caddy->session = session;
            OBJ_RETAIN(session);
            opal_event_set(orte_event_base, &caddy->ev, -1, OPAL_EV_WRITE,
                           s->cbfunc, caddy);
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
                             "ACTIVATE: ANY RM STATE NOT FOUND"));
        return;
    }
    if (NULL == s->cbfunc) {
        OPAL_OUTPUT_VERBOSE((1, orcm_scd_base_framework.framework_output,
                             "ACTIVATE: ANY RM STATE HANDLER NOT DEFINED"));
        return;
    }
    caddy = OBJ_NEW(orcm_session_caddy_t);
    caddy->session = session;
    OBJ_RETAIN(session);
    OPAL_OUTPUT_VERBOSE((1, orcm_scd_base_framework.framework_output,
                         "%s ACTIVATING RM SESSION %d STATE %s PRI %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         session->id, orcm_rm_session_state_to_str(state),
                         s->priority));
    opal_event_set(orte_event_base, &caddy->ev, -1,
                   OPAL_EV_WRITE, s->cbfunc, caddy);
    opal_event_set_priority(&caddy->ev, s->priority);
    opal_event_active(&caddy->ev, OPAL_EV_WRITE, 1);
}


int orcm_scd_base_rm_add_session_state(orcm_scd_base_rm_session_state_t state,
                                       orcm_scd_state_cbfunc_t cbfunc,
                                       int priority)
{
    orcm_scd_base_rm_state_t *st;

    /* check for uniqueness */
    OPAL_LIST_FOREACH(st, &orcm_scd_base.rmstates, orcm_scd_base_rm_session_state_t) {
        if (st->state == state) {
            OPAL_OUTPUT_VERBOSE((1, orcm_scd_base_framework.framework_output,
                                 "DUPLICATE RM STATE DEFINED: %s",
                                 orcm_rm_session_state_to_str(state)));
            return ORCM_ERR_BAD_PARAM;
        }
    }

    st = OBJ_NEW(orcm_scd_base_rm_state_t);
    st->state = state;
    st->cbfunc = cbfunc;
    st->priority = priority;
    opal_list_append(&orcm_scd_base.rmstates, &st->super);

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "RM SESSION STATE %s registered",
                         orcm_rm_session_state_to_str(state)));

    return ORCM_SUCCESS;
}
