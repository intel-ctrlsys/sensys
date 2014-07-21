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
#include "scd_external.h"

static int init(void);
static void finalize(void);
static int external_launch(orcm_session_t *session);
static int external_cancel(orcm_session_id_t sessionid);

orcm_scd_base_module_t orcm_scd_external_module = {
    init,
    finalize,
    external_launch,
    external_cancel
};

static void external_terminated(int sd, short args, void *cbdata);

static orcm_scd_session_state_t states[] = {
    ORCM_SESSION_STATE_TERMINATED
};
static orcm_scd_state_cbfunc_t callbacks[] = {
    external_terminated
};

static int init(void)
{
    int i, rc, num_states;

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:external:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* start the receive */
    if (ORCM_SUCCESS != (rc = orcm_scd_base_comm_start())) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* initialize the resource management service */
    scd_base_rm_init();

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
                         "%s scd:external:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    orcm_scd_base_comm_stop();
}

static int external_launch(orcm_session_t *session)
{
    char **nodenames = NULL;
    int rc, num_nodes, i, j;
    orcm_node_t *nodeptr;
    orcm_queue_t *q;

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:external:launch - (session: %d) got nodelist %s\n",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), 
                         session->id,
                         session->alloc->nodes));

    /* put session on running queue */
    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        if (0 == strcmp(q->name, "running")) {
            session->alloc->queues = strdup(q->name);
            opal_list_append(&q->sessions, &session->super);
            break;
        }
    }

    ORCM_ACTIVATE_RM_STATE(session, ORCM_SESSION_STATE_ACTIVE);

    /* set nodes to ALLOC
    */
    if (ORTE_SUCCESS !=
        (rc = orte_regex_extract_node_names(session->alloc->nodes, &nodenames))) {
        ORTE_ERROR_LOG(rc);
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:external:allocated - (session: %d) could not extract nodelist\n",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             session->id));
        goto ERROR;
    }

    num_nodes = opal_argv_count(nodenames);
    if (num_nodes != session->alloc->min_nodes) {
        /* what happened? we didn't get all of the nodes we needed? */
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:external:allocated - (session: %d) nodelist did not contain same number of nodes as requested, expected: %i got: %i\n",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), 
                             session->id,
                             session->alloc->min_nodes,
                             num_nodes));
        goto ERROR;
    }

    /* node array should be indexed by node num, 
     * if we change to lookup by index that would be faster 
     */
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

    if (NULL != nodenames) {
        opal_argv_free(nodenames);
    }

    return ORCM_SUCCESS;

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
    return ORCM_ERR_OUT_OF_RESOURCE;
}

static int external_cancel(orcm_session_id_t sessionid)
{
    orcm_queue_t *q;
    orcm_session_t *session;

    /* if session is queued, find it and delete it */
    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        OPAL_LIST_FOREACH(session, &q->sessions, orcm_session_t) {
            if (session->id == sessionid) {
                /* if session is running, send cancel launch command */
                if (0 == strcmp(q->name, "running")) {
                    ORCM_ACTIVATE_RM_STATE(session, ORCM_SESSION_STATE_KILL);
                } else {
                    opal_list_remove_item(&q->sessions, &session->super);
                }
                break;
            }
        }
    }
    return ORCM_SUCCESS;
}

static void external_terminated(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    int rc, i, j, num_nodes;
    orcm_node_t* nodeptr;
    char **nodenames = NULL;
    orcm_queue_t *q;
    orcm_session_t *session;

    /* set nodes to UNALLOC
    */
    if (ORTE_SUCCESS !=
        (rc = orte_regex_extract_node_names(caddy->session->alloc->nodes,
                                            &nodenames))) {
        ORTE_ERROR_LOG(rc);
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:external:terminated - (session: %d) could not extract nodelist\n",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             caddy->session->id));

        if (NULL != nodenames) {
            opal_argv_free(nodenames);
        }

        return;
    }

    num_nodes = opal_argv_count(nodenames);

    /* node array should be indexed by node num, 
     * if we change to lookup by index that would be faster 
     */
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

    OBJ_RELEASE(caddy);
    if (NULL != nodenames) {
        opal_argv_free(nodenames);
    }
}

