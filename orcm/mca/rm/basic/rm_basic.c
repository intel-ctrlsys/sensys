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

#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/scd/base/base.h"
#include "orcm/mca/rm/base/base.h"
#include "rm_basic.h"

static int init(void);
static void finalize(void);


orcm_rm_base_module_t orcm_rm_basic_module = {
    init,
    finalize
};

static void basic_undef(int sd, short args, void *cbdata);
static void basic_request(int sd, short args, void *cbdata);

static orcm_rm_session_state_t states[] = {
    ORCM_SESSION_STATE_UNDEF,
    ORCM_SESSION_STATE_REQ
};
static orcm_rm_state_cbfunc_t callbacks[] = {
    basic_undef,
    basic_request
};

static int init(void)
{
    int i, rc, num_states;

    OPAL_OUTPUT_VERBOSE((5, orcm_rm_base_framework.framework_output,
                         "%s rm:basic:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* start the receive */
    if (ORCM_SUCCESS != (rc = orcm_rm_base_comm_start())) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* define our state machine */
    num_states = sizeof(states) / sizeof(orcm_rm_session_state_t);
    for (i=0; i < num_states; i++) {
        if (ORCM_SUCCESS != (rc = orcm_rm_base_add_session_state(states[i],
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
    OPAL_OUTPUT_VERBOSE((5, orcm_rm_base_framework.framework_output,
                         "%s rm:basic:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    orcm_rm_base_comm_stop();
}

static void basic_undef(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    /* this isn't defined - so just report the error */
    opal_output(0, "%s UNDEF RM STATE CALLED",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    OBJ_RELEASE(caddy);
}

static void basic_request(int sd, short args, void *cbdata)
{
    orcm_node_t *nodeptr;
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    int i, rc, num_nodes;
    char *nodelist, *noderegex;

    num_nodes = caddy->session->alloc->min_nodes;

    if (0 < num_nodes) {
        for (i = orcm_rm_base.nodes.lowest_free + 1; i < orcm_rm_base.nodes.size; i++) {
            if (NULL == (nodeptr =
                         (orcm_node_t*)opal_pointer_array_get_item(&orcm_rm_base.nodes, i))) {
                continue;
            }
            if (ORCM_SCD_NODE_STATE_UNALLOC == nodeptr->scd_state && ORCM_NODE_STATE_UP == nodeptr->state) {
                if (nodelist) {
                    asprintf(&nodelist, "%s,%s", nodelist, nodeptr->name);
                } else {
                    asprintf(&nodelist, "%s", nodelist);
                }
                num_nodes--;
                if (0 == num_nodes) {
                    break;
                }
            }
        }

        if (0 == num_nodes) {
            if (ORTE_SUCCESS != (rc = orte_regex_create(nodelist, &noderegex))) {
                ORTE_ERROR_LOG(rc);
                caddy->session->alloc->nodes = strdup("ERROR");
            } else {
                caddy->session->alloc->nodes = noderegex;
            }
        } /* else, error? not enough nodes found? */

        ORCM_ACTIVATE_SCD_STATE(caddy->session, ORCM_SESSION_STATE_ALLOCD);

        if(nodelist) {
            free(nodelist);
        }
    } /* else, error? no nodes requested */

    OBJ_RELEASE(caddy);
}
