/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#include <ctype.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif

#include "opal/dss/dss.h"
#include "opal/mca/dstore/base/base.h"
#include "opal/mca/event/event.h"
#include "opal/runtime/opal.h"
#include "opal/runtime/opal_cr.h"
#include "opal/mca/hwloc/base/base.h"
#include "opal/mca/pstat/base/base.h"
#include "opal/util/arch.h"
#include "opal/util/if.h"
#include "opal/util/net.h"
#include "opal/util/os_path.h"

#include "orte/mca/ess/base/base.h"
#include "orte/mca/rml/base/base.h"
#include "orte/mca/rml/base/rml_contact.h"
#include "orte/mca/routed/base/base.h"
#include "orte/mca/routed/routed.h"
#include "orte/mca/oob/base/base.h"
#include "orte/mca/dfs/base/base.h"
#include "orte/mca/grpcomm/grpcomm.h"
#include "orte/mca/grpcomm/base/base.h"
#include "orte/mca/iof/base/base.h"
#include "orte/mca/plm/base/base.h"
#include "orte/mca/odls/base/base.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/filem/base/base.h"
#include "orte/util/parse_options.h"
#include "orte/util/proc_info.h"
#include "orte/util/session_dir.h"
#include "orte/util/name_fns.h"
#include "orte/util/nidmap.h"
#include "orte/util/regex.h"
#include "orte/util/show_help.h"
#include "orte/mca/errmgr/base/base.h"
#include "orte/mca/state/base/base.h"
#include "orte/mca/state/state.h"
#include "orte/runtime/orte_cr.h"
#include "orte/runtime/orte_wait.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_quit.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/scd/base/base.h"
#include "orcm/mca/scd/scd_types.h"
#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/cfgi/cfgi_types.h"
#include "orcm/mca/db/base/base.h"

#include "orcm/mca/sst/base/base.h"
#include "orcm/mca/sst/orcmsched/sst_orcmsched.h"

/* API functions */

static int orcmsched_init(void);
static void orcmsched_finalize(void);

/* The module struct */

orcm_sst_base_module_t orcm_sst_orcmsched_module = {
    orcmsched_init,
    orcmsched_finalize
};

/* local globals */
static bool initialized = false;
static bool signals_set=false;
static opal_event_t term_handler;
static opal_event_t int_handler;
static opal_event_t epipe_handler;
static opal_event_t sigusr1_handler;
static opal_event_t sigusr2_handler;

static void shutdown_signal(int fd, short flags, void *arg);
static void signal_callback(int fd, short flags, void *arg);
static void epipe_signal_callback(int fd, short flags, void *arg);

static void setup_sighandler(int signal, opal_event_t *ev,
                             opal_event_cbfunc_t cbfunc)
{
    opal_event_signal_set(orte_event_base, ev, signal, cbfunc, ev);
    opal_event_set_priority(ev, ORTE_ERROR_PRI);
    opal_event_signal_add(ev, NULL);
}

static int orcmsched_init(void)
{
    int ret = ORTE_ERROR;
    char *error = NULL;
    opal_buffer_t buf, *clusterbuf, *uribuf;
    opal_list_t config;
    orcm_scheduler_t *scheduler;
    orcm_node_t *mynode=NULL, *node;
    orcm_rack_t *rack;
    orcm_row_t *row;
    orcm_cluster_t *cluster;
    int32_t n;

    if (initialized) {
        return ORCM_SUCCESS;
    }
    initialized = true;

    /* Initialize the ORTE data type support */
    if (ORTE_SUCCESS != (ret = orte_ess_base_std_prolog())) {
        error = "orte_std_prolog";
        goto error;
    }

    /* setup the scheduler framework */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orcm_scd_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orcm_scd_base_open";
        goto error;
    }

    /* read the site configuration */
    OBJ_CONSTRUCT(&config, opal_list_t);
    if (ORCM_SUCCESS != (ret = orcm_cfgi.read_config(&config))) {
        error = "getting config";
        goto error;
    }

    /* define the cluster */
    OBJ_CONSTRUCT(&buf, opal_buffer_t);
    if (ORCM_SUCCESS != (ret = orcm_cfgi.define_system(&config,
                                                       &mynode,
                                                       &orte_process_info.num_procs,
                                                       &buf))) {
        error = "define system";
        goto error;
    }
    /* find ourselves in the schedulers */
    scheduler = (orcm_scheduler_t*)opal_list_get_first(orcm_schedulers);

    if (NULL == mynode) {
        orte_show_help("help-ess-orcm.txt", "node-not-found", true,
                       orcm_cfgi_base.config_file,
                       orte_process_info.nodename);
        return ORTE_ERR_SILENT;
    }
    /* our name is the name of the scheduler daemon */
    ORTE_PROC_MY_NAME->jobid = mynode->daemon.jobid;
    ORTE_PROC_MY_NAME->vpid = mynode->daemon.vpid;

    opal_output_verbose(2, orcm_sst_base_framework.framework_output,
                        "%s scheduler is defined",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* register the ORTE-level params at this time now that the
     * config has had a chance to push things into the environ
     */
    if (ORTE_SUCCESS != (ret = orte_register_params())) {
        error = "orte_register_params";
        goto error;
    }

   /* setup callback for SIGPIPE */
    setup_sighandler(SIGPIPE, &epipe_handler, epipe_signal_callback);
    /* Set signal handlers to catch kill signals so we can properly clean up
     * after ourselves. 
     */
    setup_sighandler(SIGTERM, &term_handler, shutdown_signal);
    setup_sighandler(SIGINT, &int_handler, shutdown_signal);
    
    /** setup callbacks for signals we should ignore */
    setup_sighandler(SIGUSR1, &sigusr1_handler, signal_callback);
    setup_sighandler(SIGUSR2, &sigusr2_handler, signal_callback);
    signals_set = true;

    /* cycle thru the cluster and set node states */
    OPAL_LIST_FOREACH(cluster, orcm_clusters, orcm_cluster_t) {
        OPAL_LIST_FOREACH(row, &cluster->rows, orcm_row_t) {
            OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
                OPAL_LIST_FOREACH(node, &rack->nodes, orcm_node_t) {
                    /* add the node to the scheduler pool */
                    opal_output_verbose(2, orcm_sst_base_framework.framework_output,
                                        "%s adding node %s",
                                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                        (NULL == node->name) ? "NULL" : node->name);
                    OBJ_RETAIN(node);  // maintain accounting
                    opal_pointer_array_add(&orcm_scd_base.nodes, node);
                }
            }
        }
    }

     /* open and setup the state machine */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_state_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_state_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_state_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_state_base_select";
        goto error;
    }
    
    /* open the errmgr */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_errmgr_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_errmgr_base_open";
        goto error;
    }
    
    /* Setup the communication infrastructure */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_oob_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_oob_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_oob_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_oob_base_select";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_rml_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rml_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_rml_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rml_base_select";
        goto error;
    }
    
    /* select the errmgr */
    if (ORTE_SUCCESS != (ret = orte_errmgr_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_errmgr_base_select";
        goto error;
    }
    
    /* Routed system */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_routed_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rml_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_routed_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_routed_base_select";
        goto error;
    }
    
    /* database */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orcm_db_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orcm_db_base_open";
        goto error;
    }
    /* always restrict to local (i.e., non-PMI) database components */
    if (ORTE_SUCCESS != (ret = orcm_db_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orcm_db_base_select";
        goto error;
    }

    /* datastore - ensure we don't pickup the pmi component, but
     * don't override anything set by user
     */
    if (NULL == getenv("OMPI_MCA_dstore")) {
        putenv("OMPI_MCA_dstore=^pmi");
    }
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&opal_dstore_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "opal_dstore_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = opal_dstore_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "opal_dstore_base_select";
        goto error;
    }
    /* create the handles */
    if (0 > (opal_dstore_peer = opal_dstore.open("PEER"))) {
        error = "opal dstore global";
        ret = ORTE_ERR_FATAL;
        goto error;
    }
    if (0 > (opal_dstore_internal = opal_dstore.open("INTERNAL"))) {
        error = "opal dstore internal";
        ret = ORTE_ERR_FATAL;
        goto error;
    }
    if (0 > (opal_dstore_nonpeer = opal_dstore.open("NONPEER"))) {
        error = "opal dstore nonpeer";
        ret = ORTE_ERR_FATAL;
        goto error;
    }

    /* initialize the nidmaps */
    if (ORTE_SUCCESS != (ret = orte_util_nidmap_init(NULL))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_util_nidmap_init";
        goto error;
    }

    /* extract the cluster description and setup the routed info - the orcm routed component
     * will know what to do. */
    n = 1;
    if (OPAL_SUCCESS != (ret = opal_dss.unpack(&buf, &clusterbuf, &n, OPAL_BUFFER))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "extract cluster buf";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_routed.init_routes(ORTE_PROC_MY_NAME->jobid, clusterbuf))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        OBJ_RELEASE(clusterbuf);
        error = "orte_routed.init_routes";
        goto error;
    }
    OBJ_RELEASE(clusterbuf);

    /* extract the uri buffer and load the hash tables */
    n = 1;
    if (OPAL_SUCCESS != (ret = opal_dss.unpack(&buf, &uribuf, &n, OPAL_BUFFER))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "extract uri buffer";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_rml_base_update_contact_info(uribuf))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        OBJ_RELEASE(uribuf);
        error = "load hash tables";
        goto error;
    }
    OBJ_DESTRUCT(&buf);
    OBJ_RELEASE(uribuf);

    /*
     * Group communications
     */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_grpcomm_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_grpcomm_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_grpcomm_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_grpcomm_base_select";
        goto error;
    }
    
    /* enable communication with the rml */
    if (ORTE_SUCCESS != (ret = orte_rml.enable_comm())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rml.enable_comm";
        goto error;
    }
    
    /* setup the FileM */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_filem_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_filem_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_filem_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_filem_base_select";
        goto error;
    }
    
    /*
     * Initalize the CR setup
     * Note: Always do this, even in non-FT builds.
     * If we don't some user level tools may hang.
     */
    opal_cr_set_enabled(false);
    if (ORTE_SUCCESS != (ret = orte_cr_init())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_cr_init";
        goto error;
    }
    
    /* setup the DFS framework */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_dfs_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_dfs_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_dfs_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_dfs_select";
        goto error;
    }

    /* select and start the scheduler */
    if (ORTE_SUCCESS != (ret = orcm_scd_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orcm_scd_select";
        goto error;
    }
    ORCM_CONSTRUCT_QUEUES(scheduler);

    return ORCM_SUCCESS;
    
 error:
    orte_show_help("help-orte-runtime.txt",
                   "orte_init:startup:internal-failure",
                   true, error, ORTE_ERROR_NAME(ret), ret);
    
    return ORCM_ERR_SILENT;
}

static void orcmsched_finalize(void)
{
    if (signals_set) {
        /* Release all local signal handlers */
        opal_event_del(&epipe_handler);
        opal_event_del(&term_handler);
        opal_event_del(&int_handler);
        opal_event_signal_del(&sigusr1_handler);
        opal_event_signal_del(&sigusr2_handler);
    }
    
    /* close frameworks */
    (void) mca_base_framework_close(&orcm_scd_base_framework);
    (void) mca_base_framework_close(&orte_filem_base_framework);
    (void) mca_base_framework_close(&orte_grpcomm_base_framework);
    (void) mca_base_framework_close(&orte_iof_base_framework);
    (void) mca_base_framework_close(&orte_errmgr_base_framework);
    (void) mca_base_framework_close(&orte_dfs_base_framework);
    (void) mca_base_framework_close(&orte_routed_base_framework);
    (void) mca_base_framework_close(&orte_rml_base_framework);
    (void) mca_base_framework_close(&orte_oob_base_framework);
    (void) mca_base_framework_close(&orte_state_base_framework);
    (void) mca_base_framework_close(&orcm_db_base_framework);
    (void) mca_base_framework_close(&opal_dstore_base_framework);
}

static void shutdown_signal(int fd, short flags, void *arg)
{
    /* trigger the call to shutdown callback to protect
     * against race conditions - the trigger event will
     * check the one-time lock
     */
    ORTE_UPDATE_EXIT_STATUS(ORTE_ERR_INTERUPTED);
    ORTE_ACTIVATE_JOB_STATE(NULL, ORTE_JOB_STATE_FORCED_EXIT);
}

/**
 * Deal with sigpipe errors
 */
static void epipe_signal_callback(int fd, short flags, void *arg)
{
    /* for now, we just announce and ignore them */
    opal_output(0, "%s reports a SIGPIPE error on fd %d",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), fd);
    return;
}

static void signal_callback(int fd, short event, void *arg)
{
    /* just ignore these signals */
}
