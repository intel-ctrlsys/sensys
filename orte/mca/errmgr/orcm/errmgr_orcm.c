/*
 * Copyright (c) 2013      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orte_config.h"

#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "opal/util/output.h"
#include "opal/dss/dss.h"

#include "orte/util/error_strings.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/routed/routed.h"
#include "orte/mca/odls/odls_types.h"
#include "orte/mca/state/state.h"

#include "orte/mca/errmgr/base/base.h"
#include "orte/mca/errmgr/base/errmgr_private.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/scd/base/base.h"
#include "orcm/mca/scd/scd_types.h"

#include "errmgr_orcm.h"

/*
 * Module functions: Global
 */
static int init(void);
static int finalize(void);

static int abort_peers(orte_process_name_t *procs,
                       orte_std_cntr_t num_procs,
                       int error_code);

/******************
 * HNP module
 ******************/
orte_errmgr_base_module_t orte_errmgr_orcm_module = {
    init,
    finalize,
    orte_errmgr_base_log,
    orte_errmgr_base_abort,
    abort_peers,
    NULL,
    NULL,
    NULL,
    orte_errmgr_base_register_migration_warning,
    NULL
};

static void proc_errors(int fd, short args, void *cbdata);

/************************
 * API Definitions
 ************************/
static int init(void)
{
    /* setup state machine to trap proc errors */
    orte_state.add_proc_state(ORTE_PROC_STATE_ERROR, proc_errors, ORTE_ERROR_PRI);

    return ORTE_SUCCESS;
}

static int finalize(void)
{
    return ORTE_SUCCESS;
}

static void proc_errors(int fd, short args, void *cbdata)
{
    orte_state_caddy_t *caddy = (orte_state_caddy_t*)cbdata;
    orte_ns_cmp_bitmask_t mask;
    opal_buffer_t *buf;
    orcm_rm_cmd_flag_t command = ORCM_NODESTATE_UPDATE_COMMAND;
    orcm_node_state_t state = ORCM_NODE_STATE_DOWN;
    int ret;

    OPAL_OUTPUT_VERBOSE((1, orte_errmgr_base_framework.framework_output,
                         "%s errmgr:orcm: proc %s state %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(&caddy->name),
                         orte_proc_state_to_str(caddy->proc_state)));
    
    /*
     * if orte is trying to shutdown, just let it
     */
    if (orte_finalizing) {
        OBJ_RELEASE(caddy);
        return;
    }

    if (ORTE_PROC_STATE_COMM_FAILED == caddy->proc_state) {
        mask = ORTE_NS_CMP_ALL;
        /* if it is our own connection, ignore it */
        if (OPAL_EQUAL == orte_util_compare_name_fields(mask, ORTE_PROC_MY_NAME, &caddy->name)) {
            OBJ_RELEASE(caddy);
            return;
        }
        /* see is this was a lifeline */
        if (ORTE_SUCCESS != orte_routed.route_lost(&caddy->name)) {
            /* order an exit */
            ORTE_ERROR_LOG(ORTE_ERR_UNRECOVERABLE);
            OBJ_RELEASE(caddy);
            exit(1);
        } else {
            /* inform the scheduler of the lost connection */
            buf = OBJ_NEW(opal_buffer_t);
            /* pack the alloc command flag */
            if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &command,1, ORCM_RM_CMD_T))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buf);
                return;
            }
            if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &state, 1, OPAL_INT8))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buf);
                return;
            }
            if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &caddy->name, 1, ORTE_NAME))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buf);
                return;
            }
            if (ORTE_SUCCESS != (ret = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                                                               ORCM_RML_TAG_RM,
                                                               orte_rml_send_callback, NULL))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buf);
                return;
            }
        }
    }

    /* cleanup */
    OBJ_RELEASE(caddy);
}

static int abort_peers(orte_process_name_t *procs,
                       orte_std_cntr_t num_procs,
                       int error_code)
{
    /* just abort */
    if (0 < opal_output_get_verbosity(orte_errmgr_base_framework.framework_output)) {
        orte_errmgr_base_abort(error_code, "%s called abort_peers",
                               ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    } else {
        orte_errmgr_base_abort(error_code, NULL);
    }
    return ORTE_SUCCESS;
}
