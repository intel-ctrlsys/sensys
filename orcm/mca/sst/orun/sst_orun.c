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
#include "opal/hash_string.h"
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

#include "orte/mca/db/base/base.h"
#include "orte/mca/ess/base/base.h"
#include "orte/mca/rmaps/base/base.h"
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
#include "orcm/mca/cfgi/base/base.h"

#include "orcm/mca/sst/base/base.h"
#include "orcm/mca/sst/orun/sst_orun.h"

/* API functions */

static int orun_init(void);
static void orun_finalize(void);

/* The module struct */

orcm_sst_base_module_t orcm_sst_orun_module = {
    orun_init,
    orun_finalize
};

/* local globals */
static bool initialized = false;
static bool signals_set=false;
static opal_event_t term_handler;
static opal_event_t int_handler;
static opal_event_t epipe_handler;
static opal_event_t sigusr1_handler;
static opal_event_t sigusr2_handler;
static char *log_path = NULL;

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

static int orun_init(void)
{
    int ret = ORTE_ERROR;
    char *error = NULL;
    opal_buffer_t buf;
    orte_job_t *jdata;
    orte_app_context_t *app;
    orte_proc_t *proc;
    orte_node_t *node;
    opal_list_t config;
    orcm_node_t *mynode=NULL, *onode;
    orcm_rack_t *rack, *rk;
    orcm_row_t *row;
    orcm_cluster_t *cluster;

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

    /* read the site configuration */
    OBJ_CONSTRUCT(&config, opal_list_t);
    if (ORCM_SUCCESS != (ret = orcm_cfgi.read_config(&config))) {
        error = "getting config";
        goto error;
    }

    /* define the cluster and collect contact info for all
     * aggregators - we'll need to know how to talk to any
     * of them in case of failures
     */
    OBJ_CONSTRUCT(&buf, opal_buffer_t);
    if (ORCM_SUCCESS != (ret = orcm_cfgi.define_system(&config,
                                                       &mynode,
                                                       &orte_process_info.num_procs,
                                                       &buf))) {
        OBJ_DESTRUCT(&buf);
        error = "define system";
        goto error;
    }
    /* set our name - for now, it is locked until we
     * have a scheduler
     */
    ORTE_PROC_MY_NAME->jobid = 0;
    ORTE_PROC_MY_NAME->vpid = 0;
    /* I am no own daemon and HNP */
    ORTE_PROC_MY_DAEMON->jobid = 0;
    ORTE_PROC_MY_DAEMON->vpid = 0;
    ORTE_PROC_MY_HNP->jobid = 0;
    ORTE_PROC_MY_HNP->vpid = 0;

    /* Setup the job data object for the daemons */        
    /* create and store the job data object */
    jdata = OBJ_NEW(orte_job_t);
    jdata->jobid = ORTE_PROC_MY_NAME->jobid;
    opal_pointer_array_set_item(orte_job_data, 0, jdata);
    /* mark that the daemons have reported as we are the
     * only ones in the system right now, and we definitely
     * are running!
     */
    jdata->state = ORTE_JOB_STATE_DAEMONS_REPORTED;

    /* every job requires at least one app */
    app = OBJ_NEW(orte_app_context_t);
    opal_pointer_array_set_item(jdata->apps, 0, app);
    jdata->num_apps++;

    /* create and store a node object where we are */
    node = OBJ_NEW(orte_node_t);
    node->name = strdup(orte_process_info.nodename);
    node->index = opal_pointer_array_set_item(orte_node_pool, 0, node);

    /* create and store a proc object for us */
    proc = OBJ_NEW(orte_proc_t);
    proc->name.jobid = ORTE_PROC_MY_NAME->jobid;
    proc->name.vpid = ORTE_PROC_MY_NAME->vpid;
    
    proc->pid = orte_process_info.pid;
    proc->state = ORTE_PROC_STATE_RUNNING;
    OBJ_RETAIN(node);  /* keep accounting straight */
    proc->node = node;
    proc->nodename = node->name;
    opal_pointer_array_set_item(jdata->procs, proc->name.vpid, proc);

    /* record that the daemon (i.e., us) is on this node 
     * NOTE: we do not add the proc object to the node's
     * proc array because we are not an application proc.
     * Instead, we record it in the daemon field of the
     * node object
     */
    OBJ_RETAIN(proc);   /* keep accounting straight */
    node->daemon = proc;
    node->daemon_launched = true;
    node->state = ORTE_NODE_STATE_UP;
    
    /* if we are to retain aliases, get ours */
    if (orte_retain_aliases) {
        opal_ifgetaliases(&node->alias);
        /* add our own local name to it */
        opal_argv_append_nosize(&node->alias, orte_process_info.nodename);
    }

    /* record that the daemon job is running */
    jdata->num_procs = 1;
    jdata->state = ORTE_JOB_STATE_RUNNING;
    /* obviously, we have "reported" */
    jdata->num_reported = 1;

#if 0
    /* cycle thru the cluster and assemble the node/job arrays */
    OPAL_LIST_FOREACH(cluster, orcm_clusters, orcm_cluster_t) {
        OPAL_LIST_FOREACH(row, &cluster->rows, orcm_row_t) {
            if (ORTE_NODE_STATE_UNDEF != row->controller.state) {
                /* add a daemon proc object for this node */
                proc = OBJ_NEW(orte_proc_t);
                proc->name.jobid = row->controller.daemon.jobid;
                proc->name.vpid = row->controller.daemon.vpid;
                opal_pointer_array_set_item(jdata->procs, proc->name.vpid, proc);
                /* add a node object, but mark it as not
                 * available for scheduling as it isn't
                 * a compute node
                 */
                node = OBJ_NEW(orte_node_t);
                node->name = strdup(row->controller.name);
                node->state = ORTE_NODE_STATE_NOT_INCLUDED;
                node->index = opal_pointer_array_add(orte_node_pool, node);
                /* wire them together */
                OBJ_RETAIN(node);  /* keep accounting straight */
                proc->node = node;
                proc->nodename = node->name;
            }
            OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
                if (ORTE_NODE_STATE_UNDEF != rack->controller.state) {
                    /* add a daemon proc object for this node */
                    proc = OBJ_NEW(orte_proc_t);
                    proc->name.jobid = rack->controller.daemon.jobid;
                    proc->name.vpid = rack->controller.daemon.vpid;
                    opal_pointer_array_set_item(jdata->procs, proc->name.vpid, proc);
                    /* add a node object, but mark it as not
                     * available for scheduling as it isn't
                     * a compute node
                     */
                    node = OBJ_NEW(orte_node_t);
                    node->name = strdup(rack->controller.name);
                    node->index = opal_pointer_array_add(orte_node_pool, node);
                    node->state = ORTE_NODE_STATE_NOT_INCLUDED;
                    /* wire them together */
                    OBJ_RETAIN(node);  /* keep accounting straight */
                    proc->node = node;
                    proc->nodename = node->name;
                }
                OPAL_LIST_FOREACH(onode, &rack->nodes, orcm_node_t) {
                    /* every compute node needs a daemon proc object */
                    proc = OBJ_NEW(orte_proc_t);
                    proc->name.jobid = onode->daemon.jobid;
                    proc->name.vpid = onode->daemon.vpid;
                    opal_pointer_array_set_item(jdata->procs, proc->name.vpid, proc);
                    /* and a node object - mark the node as up by default */
                    node = OBJ_NEW(orte_node_t);
                    node->name = strdup(onode->name);
                    node->index = opal_pointer_array_add(orte_node_pool, node);
                    /* wire them together */
                    OBJ_RETAIN(node);  /* keep accounting straight */
                    proc->node = node;
                    proc->nodename = node->name;
                }
            }
        }
    }
#endif

    /* register the ORTE-level params at this time now that the
     * config has had a chance to push things into the environ
     */
    if (ORTE_SUCCESS != (ret = orte_register_params())) {
        OBJ_DESTRUCT(&buf);
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

    /* setup the global nidmap/pidmap object */
    orte_nidmap.bytes = NULL;
    orte_nidmap.size = 0;
    orte_pidmap.bytes = NULL;
    orte_pidmap.size = 0;

 
#if OPAL_HAVE_HWLOC
    {
        hwloc_obj_t obj;
        unsigned i, j;
        
        /* get the local topology */
        if (NULL == opal_hwloc_topology) {
            if (OPAL_SUCCESS != opal_hwloc_base_get_topology()) {
                OBJ_DESTRUCT(&buf);
                error = "topology discovery";
                goto error;
            }
        }
        
        /* remove the hostname from the topology. Unfortunately, hwloc
         * decided to add the source hostname to the "topology", thus
         * rendering it unusable as a pure topological description. So
         * we remove that information here.
         */
        obj = hwloc_get_root_obj(opal_hwloc_topology);
        for (i=0; i < obj->infos_count; i++) {
            if (NULL == obj->infos[i].name ||
                NULL == obj->infos[i].value) {
                continue;
            }
            if (0 == strncmp(obj->infos[i].name, "HostName", strlen("HostName"))) {
                free(obj->infos[i].name);
                free(obj->infos[i].value);
                /* left justify the array */
                for (j=i; j < obj->infos_count-1; j++) {
                    obj->infos[j] = obj->infos[j+1];
                }
                obj->infos[obj->infos_count-1].name = NULL;
                obj->infos[obj->infos_count-1].value = NULL;
                obj->infos_count--;
                break;
            }
        }
        /* add it to the array of known topologies */
        opal_pointer_array_add(orte_node_topologies, opal_hwloc_topology);
        
        if (15 < opal_output_get_verbosity(orcm_sst_base_framework.framework_output)) {
            opal_output(0, "%s Topology Info:", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            opal_dss.dump(0, opal_hwloc_topology, OPAL_HWLOC_TOPO);
        }
    }
#endif            

    /* open and setup the state machine */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_state_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "orte_state_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_state_base_select())) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "orte_state_base_select";
        goto error;
    }
    
    /* open the errmgr */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_errmgr_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "orte_errmgr_base_open";
        goto error;
    }
    
    /* we want the "isolated" plm */
    putenv("OMPI_MCA_plm=isolated");
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_plm_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_plm_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_plm_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_plm_base_select";
        goto error;
    }

    /* Setup the communication infrastructure */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_oob_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
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
        OBJ_DESTRUCT(&buf);
        error = "orte_rml_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_rml_base_select())) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "orte_rml_base_select";
        goto error;
    }
    
    /* select the errmgr */
    if (ORTE_SUCCESS != (ret = orte_errmgr_base_select())) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "orte_errmgr_base_select";
        goto error;
    }

    /* Routed system */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_routed_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "orte_rml_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_routed_base_select())) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "orte_routed_base_select";
        goto error;
    }

    /* database */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_db_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "orte_db_base_open";
        goto error;
    }
    /* always restrict daemons to local database components */
    if (ORTE_SUCCESS != (ret = orte_db_base_select())) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "orte_db_base_select";
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
        OBJ_DESTRUCT(&buf);
        error = "orte_util_nidmap_init";
        goto error;
    }

    /* setup our routing table */
    if (NULL != mynode && NULL != mynode->rack) {
        rack = (orcm_rack_t*)mynode->rack;
        row = rack->row;
        if (ORCM_PROC_IS_DAEMON) {
            /* set my default route to start with my "daemon" */
            orte_routed.update_route(ORTE_PROC_MY_DAEMON,
                                     ORTE_PROC_MY_DAEMON);
            /* if my rack controller is available, then
             * designate any other rack controllers as failover paths
             * by telling the routed component to route to them via
             * my rack controller
             */
            if (ORTE_NODE_STATE_UNDEF != rack->controller.state) {
                OPAL_LIST_FOREACH(rk, &row->racks, orcm_rack_t) {
                    if (rk != rack && ORTE_NODE_STATE_UNDEF != rk->controller.state) {
                        orte_routed.update_route(&rk->controller.daemon,
                                                 &rack->controller.daemon);
                    }
                }
            }
        } else {
            /* must be an aggregator - set my default
             * route to be my HNP
             */
            orte_routed.update_route(ORTE_PROC_MY_HNP,
                                     ORTE_PROC_MY_HNP);
        }
    } else {
        orte_routed.update_route(ORTE_PROC_MY_HNP,
                                 ORTE_PROC_MY_HNP);
    }

    /* load the hash tables */
    if (ORTE_SUCCESS != (ret = orte_rml_base_update_contact_info(&buf))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
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
    
    /* Now provide a chance for the PLM
     * to perform any module-specific init functions. This
     * needs to occur AFTER the communications are setup
     * as it may involve starting a non-blocking recv
     */
    if (ORTE_SUCCESS != (ret = orte_plm.init())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_plm_init";
        goto error;
    }

    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_rmaps_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rmaps_base_open";
        goto error;
    }    
    if (ORTE_SUCCESS != (ret = orte_rmaps_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rmaps_base_find_available";
        goto error;
    }
#if OPAL_HAVE_HWLOC
    {
        char *coprocessors, **sns;
        uint32_t h;
        int idx;

        /* if a topology file was given, then the rmaps framework open
         * will have reset our topology. Ensure we always get the right
         * one by setting our node topology afterwards
         */
        node = (orte_node_t*)opal_pointer_array_get_item(orte_node_pool, 0);
        node->topology = opal_hwloc_topology;

        /* init the hash table, if necessary */
        if (NULL == orte_coprocessors) {
            orte_coprocessors = OBJ_NEW(opal_hash_table_t);
            opal_hash_table_init(orte_coprocessors, orte_process_info.num_procs);
        }
        /* detect and add any coprocessors */
        coprocessors = opal_hwloc_base_find_coprocessors(opal_hwloc_topology);
        if (NULL != coprocessors) {
            /* separate the serial numbers of the coprocessors
             * on this host
             */
            sns = opal_argv_split(coprocessors, ',');
            for (idx=0; NULL != sns[idx]; idx++) {
                /* compute the hash */
                OPAL_HASH_STR(sns[idx], h);
                /* mark that this coprocessor is hosted by this node */
                opal_hash_table_set_value_uint32(orte_coprocessors, h, (void*)&(ORTE_PROC_MY_NAME->vpid));
            }
            opal_argv_free(sns);
            free(coprocessors);
            orte_coprocessors_detected = true;
        }
        /* see if I am on a coprocessor */
        coprocessors = opal_hwloc_base_check_on_coprocessor();
        if (NULL != coprocessors) {
            node->serial_number = coprocessors;
            orte_coprocessors_detected = true;
        }
    }
#endif

    /* Open/select the odls */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_odls_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_odls_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_odls_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_odls_base_select";
        goto error;
    }

    /* enable communication with the rml */
    if (ORTE_SUCCESS != (ret = orte_rml.enable_comm())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rml.enable_comm";
        goto error;
    }
    
    /* we are an hnp, so update the contact info field for later use */
    orte_process_info.my_hnp_uri = orte_rml.get_contact_info();
    proc = (orte_proc_t*)opal_pointer_array_get_item(jdata->procs, 0);
    proc->rml_uri = strdup(orte_process_info.my_hnp_uri);

    /* we are also officially a daemon, so better update that field too */
    orte_process_info.my_daemon_uri = strdup(orte_process_info.my_hnp_uri);
    
    /* setup the orte_show_help system to recv remote output */
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_SHOW_HELP,
                            ORTE_RML_PERSISTENT, orte_show_help_recv, NULL);

    /* setup my session directory */
    if (orte_create_session_dirs) {
        OPAL_OUTPUT_VERBOSE((2, orte_debug_output,
                             "%s setting up session dir with\n\ttmpdir: %s\n\thost %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             (NULL == orte_process_info.tmpdir_base) ? "UNDEF" : orte_process_info.tmpdir_base,
                             orte_process_info.nodename));
        
        if (ORTE_SUCCESS != (ret = orte_session_dir(true,
                                                    orte_process_info.tmpdir_base,
                                                    orte_process_info.nodename, NULL,
                                                    ORTE_PROC_MY_NAME))) {
            ORTE_ERROR_LOG(ret);
            error = "orte_session_dir";
            goto error;
        }
        
        /* Once the session directory location has been established, set
           the opal_output hnp file location to be in the
           proc-specific session directory. */
        opal_output_set_output_file_info(orte_process_info.proc_session_dir,
                                         "output-", NULL, NULL);
    }

    /* setup I/O forwarding system - must come after we init routes */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_iof_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_iof_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_iof_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_iof_base_select";
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

    return ORTE_SUCCESS;
    
 error:
    orte_show_help("help-orte-runtime.txt",
                   "orte_init:startup:internal-failure",
                   true, error, ORTE_ERROR_NAME(ret), ret);
    
    return ORTE_ERR_SILENT;
}

static void orun_finalize(void)
{
    if (signals_set) {
        /* Release all local signal handlers */
        opal_event_del(&epipe_handler);
        opal_event_del(&term_handler);
        opal_event_del(&int_handler);
        opal_event_signal_del(&sigusr1_handler);
        opal_event_signal_del(&sigusr2_handler);
    }
    
    /* cleanup */
    if (NULL != log_path) {
        unlink(log_path);
    }

    /* close frameworks */
    (void) mca_base_framework_close(&orte_filem_base_framework);
    (void) mca_base_framework_close(&orte_grpcomm_base_framework);
    (void) mca_base_framework_close(&orte_iof_base_framework);
    (void) mca_base_framework_close(&orte_errmgr_base_framework);
    (void) mca_base_framework_close(&orte_plm_base_framework);

    /* close the dfs so its threads can exit */
    (void) mca_base_framework_close(&orte_dfs_base_framework);

    /* make sure our local procs are dead */
    orte_odls.kill_local_procs(NULL);
    (void) mca_base_framework_close(&orte_odls_base_framework);
    (void) mca_base_framework_close(&orte_routed_base_framework);
    (void) mca_base_framework_close(&orte_rml_base_framework);
    (void) mca_base_framework_close(&orte_oob_base_framework);
    (void) mca_base_framework_close(&orte_state_base_framework);

    (void) mca_base_framework_close(&orte_db_base_framework);
    (void) mca_base_framework_close(&opal_dstore_base_framework);

    /* cleanup any lingering session directories */
    orte_session_dir_cleanup(ORTE_JOBID_WILDCARD);
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
