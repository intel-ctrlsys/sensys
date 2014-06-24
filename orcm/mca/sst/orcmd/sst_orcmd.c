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
#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/db/base/base.h"
#include "orcm/mca/diag/base/base.h"
#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/sensor.h"
#include "orcm/util/utils.h"

#include "orcm/mca/sst/base/base.h"
#include "orcm/mca/sst/orcmd/sst_orcmd.h"

/* API functions */

static int orcmd_init(void);
static void orcmd_finalize(void);

/* The module struct */

orcm_sst_base_module_t orcm_sst_orcmd_module = {
    orcmd_init,
    orcmd_finalize
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

static int orcmd_init(void)
{
    int ret = ORTE_ERROR;
    char *error = NULL;
    opal_buffer_t buf, *clusterbuf, *uribuf;
    orte_job_t *jdata;
    orte_node_t *node;
    orte_proc_t *proc;
    opal_list_t config;
    orcm_scheduler_t *scheduler;
    orcm_node_t *mynode=NULL;
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

    /* if my name didn't get set, then we didn't find our node
     * in the config - report it and die
     */
    if (NULL == mynode) {
        orte_show_help("help-ess-orcm.txt", "node-not-found", true,
                       orcm_cfgi_base.config_file,
                       orte_process_info.nodename);
        OBJ_DESTRUCT(&buf);
        return ORTE_ERR_SILENT;
    }

    /* define a node and proc object for ourselves as some parts
     * of ORTE and ORCM require it */
    if (NULL == (node = OBJ_NEW(orte_node_t))) {
        ret = ORTE_ERR_OUT_OF_RESOURCE;
        error = "out of memory";
        goto error;
    }
    node->name = strdup(orte_process_info.nodename);
    opal_pointer_array_set_item(orte_node_pool, ORTE_PROC_MY_NAME->vpid, node);
    if (NULL == (proc = OBJ_NEW(orte_proc_t))) {
        ret = ORTE_ERR_OUT_OF_RESOURCE;
        error = "out of memory";
        goto error;
    }
    proc->name.jobid = ORTE_PROC_MY_NAME->jobid;
    proc->name.vpid = ORTE_PROC_MY_NAME->vpid;
    OBJ_RETAIN(proc);
    node->daemon = proc;
    OBJ_RETAIN(node);
    proc->node = node;
    opal_pointer_array_set_item(jdata->procs, ORTE_PROC_MY_NAME->vpid, proc);

    /* For now, we only support a single scheduler daemon in the system.
     * This *may* change someday in the future */
    scheduler = (orcm_scheduler_t*)opal_list_get_first(orcm_schedulers);

    /* If we are in test mode, then we don't *require* that a scheduler
     * be defined in the system - otherwise, we do */
    if (NULL == scheduler) {
        if (mca_sst_orcmd_component.scheduler_reqd) {
            error = "no scheduler found";
            ret = ORTE_ERR_NOT_FOUND;
            goto error;
        }
    } else {
        ORTE_PROC_MY_SCHEDULER->jobid = scheduler->controller.daemon.jobid;
        ORTE_PROC_MY_SCHEDULER->vpid = scheduler->controller.daemon.vpid;
    }

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
        
        if (15 < opal_output_get_verbosity(orcm_sst_base_framework.framework_output)) {
            opal_output(0, "%s Topology Info:", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            opal_dss.dump(0, opal_hwloc_topology, OPAL_HWLOC_TOPO);
        }

        /* if we were asked to bind to specific core(s), do so now */
        if (NULL != orte_daemon_cores) {
            char **cores=NULL, tmp[128];
            hwloc_obj_t pu;
            hwloc_cpuset_t ours, pucpus, res;
            int core;

            /* could be a collection of comma-delimited ranges, so
             * use our handy utility to parse it
             */
            orte_util_parse_range_options(orte_daemon_cores, &cores);
            if (NULL != cores) {
                ours = hwloc_bitmap_alloc();
                hwloc_bitmap_zero(ours);
                pucpus = hwloc_bitmap_alloc();
                res = hwloc_bitmap_alloc();
                for (i=0; NULL != cores[i]; i++) {
                    core = strtoul(cores[i], NULL, 10);
                    if (NULL == (pu = opal_hwloc_base_get_pu(opal_hwloc_topology, core))) {
                        orte_show_help("help-orted.txt", "orted:cannot-bind",
                                       true, orte_process_info.nodename,
                                       orte_daemon_cores);
                        ret = ORTE_ERR_NOT_SUPPORTED;
                        OBJ_DESTRUCT(&buf);
                        error = "cannot bind";
                        goto error;
                    }
                    hwloc_bitmap_and(pucpus, pu->online_cpuset, pu->allowed_cpuset);
                    hwloc_bitmap_or(res, ours, pucpus);
                    hwloc_bitmap_copy(ours, res);
                }
                /* if the result is all zeros, then don't bind */
                if (!hwloc_bitmap_iszero(ours)) {
                    (void)hwloc_set_cpubind(opal_hwloc_topology, ours, 0);
                    if (opal_hwloc_report_bindings) {
                        opal_hwloc_base_cset2mapstr(tmp, sizeof(tmp), opal_hwloc_topology, ours);
                        opal_output(0, "Daemon %s is bound to cores %s",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), tmp);
                    }
                }
                /* cleanup */
                hwloc_bitmap_free(ours);
                hwloc_bitmap_free(pucpus);
                hwloc_bitmap_free(res);
                opal_argv_free(cores);
            }
        }
    }
#endif            

    /* open and select the pstat framework */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&opal_pstat_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "opal_pstat_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = opal_pstat_base_select())) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "opal_pstat_base_select";
        goto error;
    }

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

    /* Runtime Messaging Layer */
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
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orcm_db_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "orcm_db_base_open";
        goto error;
    }
    /* always restrict daemons to local database components */
    if (ORTE_SUCCESS != (ret = orcm_db_base_select())) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
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
        OBJ_DESTRUCT(&buf);
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
    
    /* setup the SENSOR framework */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orcm_sensor_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orcm_sensor_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orcm_sensor_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orcm_sensor_select";
        goto error;
    }
    /* start the local sensors */
    orcm_sensor.start(ORTE_PROC_MY_NAME->jobid);
    
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

    /* open and setup the DIAG framework */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orcm_diag_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orcm_diag_base_open";
        goto error;
    }
    if (ORCM_SUCCESS != (ret = orcm_diag_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orcm_diag_select";
        goto error;
    }

    return ORTE_SUCCESS;
    
 error:
    orte_show_help("help-orte-runtime.txt",
                   "orte_init:startup:internal-failure",
                   true, error, ORTE_ERROR_NAME(ret), ret);
    
    return ORTE_ERR_SILENT;
}

static void orcmd_finalize(void)
{
    /* stop the local sensors */
    orcm_sensor.stop(ORTE_PROC_MY_NAME->jobid);
    (void) mca_base_framework_close(&orcm_sensor_base_framework);
       (void) mca_base_framework_close(&opal_pstat_base_framework);

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
    (void) mca_base_framework_close(&orcm_diag_base_framework);
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

    (void) mca_base_framework_close(&orcm_db_base_framework);
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
