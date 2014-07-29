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

#include "opal/dss/dss.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/regex.h"
#include "orte/mca/rml/rml.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/scd/base/base.h"

static void scd_base_rm_undef(int sd, short args, void *cbdata);
static void scd_base_rm_request(int sd, short args, void *cbdata);
static void scd_base_rm_active(int sd, short args, void *cbdata);
static void scd_base_rm_kill(int sd, short args, void *cbdata);

static orcm_scd_base_rm_session_state_t states[] = {
    ORCM_SESSION_STATE_UNDEF,
    ORCM_SESSION_STATE_REQ,
    ORCM_SESSION_STATE_ACTIVE,
    ORCM_SESSION_STATE_KILL
};
static orcm_scd_state_cbfunc_t callbacks[] = {
    scd_base_rm_undef,
    scd_base_rm_request,
    scd_base_rm_active,
    scd_base_rm_kill
};

int scd_base_rm_init(void)
{
    int i, rc, num_states;

    /* start the receive */
    if (ORCM_SUCCESS != (rc = orcm_scd_base_rm_comm_start())) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* define our state machine */
    num_states = sizeof(states) / sizeof(orcm_scd_base_rm_session_state_t);
    for (i=0; i < num_states; i++) {
        if (ORCM_SUCCESS !=
            (rc = orcm_scd_base_rm_add_session_state(states[i],
                                                     callbacks[i],
                                                     ORTE_SYS_PRI))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }

    return ORCM_SUCCESS;
}

void scd_base_rm_finalize(void)
{
    orcm_scd_base_rm_comm_stop();
}

static void scd_base_rm_undef(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    /* this isn't defined - so just report the error */
    opal_output(0, "%s UNDEF RM STATE CALLED",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    OBJ_RELEASE(caddy);
}

static void scd_base_rm_request(int sd, short args, void *cbdata)
{
    orcm_node_t *nodeptr;
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    int i, rc, num_nodes;
    char *nodelist = NULL;
    char *noderegex = NULL;

    num_nodes = caddy->session->alloc->min_nodes;

    if (0 < num_nodes) {
        for (i = 0; i < orcm_scd_base.nodes.size; i++) {
            if (NULL ==
                (nodeptr =
                 (orcm_node_t*)opal_pointer_array_get_item(&orcm_scd_base.nodes,
                                                           i))) {
                continue;
            }
            if (ORCM_SCD_NODE_STATE_UNALLOC == nodeptr->scd_state
                && ORCM_NODE_STATE_UP == nodeptr->state) {
                OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                                     "%s scd:rm:request adding node %s to list",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     nodeptr->name));
                if (NULL != nodelist) {
                    asprintf(&nodelist, "%s,%s", nodelist, nodeptr->name);
                } else {
                    asprintf(&nodelist, "%s", nodeptr->name);
                }
                num_nodes--;
                if (0 == num_nodes) {
                    break;
                }
            }
        }

        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:rm:request giving allocation %i nodelist %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             (int)caddy->session->alloc->id,
                             nodelist));

        if (0 == num_nodes) {
            if (ORTE_SUCCESS != (rc = orte_regex_create(nodelist, &noderegex))) {
                ORTE_ERROR_LOG(rc);
                caddy->session->alloc->nodes = strdup("ERROR");
            } else {
                caddy->session->alloc->nodes = noderegex;
            }
        } /* else, error? not enough nodes found? */

        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:rm:request giving allocation %i noderegex %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             (int)caddy->session->alloc->id,
                             noderegex));

        ORCM_ACTIVATE_SCD_STATE(caddy->session, ORCM_SESSION_STATE_ALLOCD);

        if(nodelist != NULL) {
            free(nodelist);
        }
    } /* else, error? no nodes requested */

    OBJ_RELEASE(caddy);
}

static void scd_base_rm_active(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    char **nodenames = NULL;
    int rc, i, j;
    orcm_node_t *nodeptr;
    opal_buffer_t *buf;
    orcm_rm_cmd_flag_t command = ORCM_LAUNCH_STEPD_COMMAND;

    if (ORTE_SUCCESS !=
        (rc = orte_regex_extract_node_names(caddy->session->alloc->nodes,
                                            &nodenames))) {
        ORTE_ERROR_LOG(rc);
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:rm:active - (session: %d) could not extract nodelist\n",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             caddy->session->id));
        if (NULL != nodenames) {
            opal_argv_free(nodenames);
        }
        return;
    }

    /* set hnp name to first in the list */
    if (NULL != nodenames) {
        caddy->session->alloc->hnpname = strdup(nodenames[0]);
    } else {
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:rm:active - (session: %d) got NULL nodelist\n",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             caddy->session->id));
        return;
    }

    /* node array should be indexed by node num,
     * if we change to lookup by index that would be faster */
    for (i = 0; i < caddy->session->alloc->min_nodes; i++) {
        for (j = 0; j < orcm_scd_base.nodes.size; j++) {
            if (NULL ==
                (nodeptr =
                 (orcm_node_t*)opal_pointer_array_get_item(&orcm_scd_base.nodes,
                                                           j))) {
                continue;
            }
            if (0 == strcmp(nodeptr->name, nodenames[i])) {
                if (0 == i) {
                    /* if this is the first node in the list, 
                     * then set the hnp daemon info */
                    caddy->session->alloc->hnp.jobid = nodeptr->daemon.jobid;
                    caddy->session->alloc->hnp.vpid = nodeptr->daemon.vpid;
                }
                buf = OBJ_NEW(opal_buffer_t);
                /* pack the command */
                if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                                        1, ORCM_RM_CMD_T))) {
                    ORTE_ERROR_LOG(rc);
                    opal_argv_free(nodenames);
                    return;
                }
                /* pack the allocation info */
                if (OPAL_SUCCESS != (rc = opal_dss.pack(buf,
                                                        &caddy->session->alloc,
                                                        1, ORCM_ALLOC))) {
                    ORTE_ERROR_LOG(rc);
                    opal_argv_free(nodenames);
                    return;
                }
                /* SEND ALLOC TO NODE */
                if (ORTE_SUCCESS !=
                    (rc = orte_rml.send_buffer_nb(&nodeptr->daemon, buf,
                                                  ORCM_RML_TAG_RM,
                                                  orte_rml_send_callback,
                                                  NULL))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(buf);
                    opal_argv_free(nodenames);
                    return;
                }
            }
        }
    }

    opal_argv_free(nodenames);
    OBJ_RELEASE(caddy);
}

static void scd_base_rm_kill(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    char **nodenames = NULL;
    int rc, i, j;
    orcm_node_t *nodeptr;
    opal_buffer_t *buf;
    orcm_rm_cmd_flag_t command = ORCM_CANCEL_STEPD_COMMAND;

    if (ORTE_SUCCESS !=
        (rc = orte_regex_extract_node_names(caddy->session->alloc->nodes,
                                            &nodenames))) {
        ORTE_ERROR_LOG(rc);
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:rm:kill - (session: %d) could not extract nodelist\n",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             caddy->session->id));
        if (NULL != nodenames) {
            opal_argv_free(nodenames);
        }
        return;
    }
    if (NULL == nodenames) {
        OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                             "%s scd:rm:kill - (session: %d) got NULL nodelist\n",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             caddy->session->id));
        return;
    }

    /* node array should be indexed by node num,
     * if we change to lookup by index that would be faster */
    for (i = 0; i < caddy->session->alloc->min_nodes; i++) {
        for (j = 0; j < orcm_scd_base.nodes.size; j++) {
            if (NULL ==
                (nodeptr =
                 (orcm_node_t*)opal_pointer_array_get_item(&orcm_scd_base.nodes,
                                                           j))) {
                continue;
            }
            if (0 == strcmp(nodeptr->name, nodenames[i])) {
                buf = OBJ_NEW(opal_buffer_t);
                /* pack the command */
                if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                                        1, ORCM_RM_CMD_T))) {
                    ORTE_ERROR_LOG(rc);
                    opal_argv_free(nodenames);
                    return;
                }
                /* pack the alloc so that nodes know which session to kill */
                if (OPAL_SUCCESS != (rc = opal_dss.pack(buf,
                                                        &caddy->session->alloc,
                                                        1, ORCM_ALLOC))) {
                    ORTE_ERROR_LOG(rc);
                    opal_argv_free(nodenames);
                    return;
                }
                /* SEND ALLOC TO NODE */
                if (ORTE_SUCCESS !=
                    (rc = orte_rml.send_buffer_nb(&nodeptr->daemon, buf,
                                                  ORCM_RML_TAG_RM,
                                                  orte_rml_send_callback,
                                                  NULL))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(buf);
                    opal_argv_free(nodenames);
                    return;
                }
            }
        }
    }

    opal_argv_free(nodenames);
    OBJ_RELEASE(caddy);
}
