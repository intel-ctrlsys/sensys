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
#include "opal/mca/db/base/base.h"
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
#include "orcm/mca/cfgi/base/base.h"

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
static int construct_queues(orcm_scheduler_t *scheduler);

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
    opal_buffer_t buf;
    orte_job_t *jdata;
    opal_list_t config;
    orcm_scheduler_t *scheduler;
    orcm_node_t *mynode;

    if (initialized) {
        return ORCM_SUCCESS;
    }
    initialized = true;

    /* Initialize the ORTE data type support */
    if (ORTE_SUCCESS != (ret = orte_ess_base_std_prolog())) {
        error = "orte_std_prolog";
        goto error;
    }

    /* setup the global job and node arrays */
    orte_job_data = OBJ_NEW(opal_pointer_array_t);
    if (ORTE_SUCCESS != (ret = opal_pointer_array_init(orte_job_data,
                                                       1,
                                                       ORTE_GLOBAL_ARRAY_MAX_SIZE,
                                                       1))) {
        ORTE_ERROR_LOG(ret);
        error = "setup job array";
        goto error;
    }
    
    orte_node_pool = OBJ_NEW(opal_pointer_array_t);
    if (ORTE_SUCCESS != (ret = opal_pointer_array_init(orte_node_pool,
                                                       ORTE_GLOBAL_ARRAY_BLOCK_SIZE,
                                                       ORTE_GLOBAL_ARRAY_MAX_SIZE,
                                                       ORTE_GLOBAL_ARRAY_BLOCK_SIZE))) {
        ORTE_ERROR_LOG(ret);
        error = "setup node array";
        goto error;
    }
    orte_node_topologies = OBJ_NEW(opal_pointer_array_t);
    if (ORTE_SUCCESS != (ret = opal_pointer_array_init(orte_node_topologies,
                                                       ORTE_GLOBAL_ARRAY_BLOCK_SIZE,
                                                       ORTE_GLOBAL_ARRAY_MAX_SIZE,
                                                       ORTE_GLOBAL_ARRAY_BLOCK_SIZE))) {
        ORTE_ERROR_LOG(ret);
        error = "setup node topologies array";
        goto error;
    }

    /* setup the scheduler framework */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orcm_scd_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orcm_scd_base_open";
        goto error;
    }

    /* create a job tracker for the daemons */
    jdata = OBJ_NEW(orte_job_t);
    jdata->jobid = 0;
    ORTE_PROC_MY_NAME->jobid = 0;
    opal_pointer_array_set_item(orte_job_data, 0, jdata);

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

    /* now construct our queues as specified */
    if (ORCM_SUCCESS != (ret = construct_queues(scheduler))) {
        ORTE_ERROR_LOG(ret);
        error = "construct queues";
        goto error;
    }

    /* setup the global nidmap/pidmap object */
    orte_nidmap.bytes = NULL;
    orte_nidmap.size = 0;
    orte_pidmap.bytes = NULL;
    orte_pidmap.size = 0;

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
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&opal_db_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_db_base_open";
        goto error;
    }
    /* always restrict to local (i.e., non-PMI) database components */
    if (ORTE_SUCCESS != (ret = opal_db_base_select(true))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_db_base_select";
        goto error;
    }
    /* set our id */
    opal_db.set_id((opal_identifier_t*)ORTE_PROC_MY_NAME);

    /* initialize the nidmaps */
    if (ORTE_SUCCESS != (ret = orte_util_nidmap_init(NULL))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_util_nidmap_init";
        goto error;
    }

    /* load the hash tables */
    if (ORTE_SUCCESS != (ret = orte_rml_base_update_contact_info(&buf))) {
        ORTE_ERROR_LOG(ret);
        error = "load hash tables";
        goto error;
    }
    OBJ_DESTRUCT(&buf);

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
    
    /* update the routing tree */
    orte_routed.update_routing_plan();
    
    /* setup the routed info - the selected routed component
     * will know what to do. 
     */
    if (ORTE_SUCCESS != (ret = orte_routed.init_routes(ORTE_PROC_MY_NAME->jobid, NULL))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_routed.init_routes";
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
    (void) mca_base_framework_close(&opal_db_base_framework);
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

static int construct_queues(orcm_scheduler_t *scheduler)
{
#if 0
    opal_list_t pool;
    orcm_queue_t *q, *q2, *def;
    orte_node_t *n, *m;
    int i, k, j, rc;
    char **t1, **t2, **vals;
    bool found;

    /* push our default queue onto the stack */
    def = OBJ_NEW(orcm_queue_t);
    def->name = strdup("default");
    def->priority = 0;
    opal_list_append(&orcm_scd_base.queues, &def->super);

    /* now create queues as defined in the config */
    if (NULL != orcm_sst_base.schedulers.queues) {
        for (i=0; NULL != orcm_sst_base.schedulers.queues[i]; i++) {
            /* split on the colon delimiters */
            t1 = opal_argv_split(orcm_sst_base.schedulers.queues[i], ':');
            /* must have three entries */
            if (3 != opal_argv_count(t1)) {
                opal_argv_free(t1);
                OPAL_LIST_DESTRUCT(&pool);
                return ORCM_ERR_BAD_PARAM;
            }
            /* create the object */
            q = OBJ_NEW(orcm_queue_t);
            /* first position is the queue name */
            q->name = strdup(t1[0]);
            /* second is the priority */
            q->priority = strtol(t1[1], NULL, 10);
            /* last is the comma-separated list of
             * regular expressions defining the nodes
             * to be included in this queue
             */
            t2 = opal_argv_split(t1[2], ',');
            for (k=0; NULL != t2[k]; k++) {
                vals = NULL;
                /* let the regular expression parser work on it */
                if (ORTE_SUCCESS != (rc = orte_regex_extract_node_names(t2[k], &vals))) {
                    ORTE_ERROR_LOG(rc);
                    return rc;
                }
                for (j=0; NULL != vals[j]; j++) {
                    found = false;
                    OPAL_LIST_FOREACH(n, &pool, orte_node_t) {
                        opal_output(0, "CHECKING %s AGAINST %s", vals[j], n->name);
                        if (0 == strcmp(n->name, vals[j])) {
                            opal_list_remove_item(&pool, &n->super);
                            opal_pointer_array_add(&q->nodes, n);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        orte_show_help("help-orcm-sst.txt", "unknown-node",
                                       true, q->name, vals[j]);
                        return ORCM_ERR_SILENT;
                    }
                }
                opal_argv_free(vals);
            }
            opal_argv_free(t2);
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

    /* all remaining nodes go into the default queue */
    OPAL_LIST_FOREACH_SAFE(n, m, &pool, orte_node_t) {
        opal_list_remove_item(&pool, &n->super);
        opal_pointer_array_add(&def->nodes, n);
    }
    OBJ_DESTRUCT(&pool);

    if (4 < opal_output_get_verbosity(orcm_sst_base_framework.framework_output)) {
        /* print out the queue structure */
        OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
            opal_output(0, "QUEUE: %s PRI: %d", q->name, q->priority);
            for (i=0; i < q->nodes.size; i++) {
                if (NULL != (n = (orte_node_t*)opal_pointer_array_get_item(&q->nodes, i))) {
                    opal_output(0, "\t%s", n->name);
                }
            }
        }
    }
#endif
    return ORCM_SUCCESS;
}
