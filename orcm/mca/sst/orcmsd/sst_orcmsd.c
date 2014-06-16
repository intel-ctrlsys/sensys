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

#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <stdarg.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif

#include "opal/util/if.h"
#include "opal/util/net.h"
#include "opal/util/os_path.h"
#include "opal/util/basename.h"
#include "opal/dss/dss.h"
#include "opal/mca/dstore/base/base.h"
#include "opal/mca/hwloc/base/base.h"
#include "opal/mca/pstat/base/base.h"
#include "opal/mca/pstat/pstat.h"
#include "opal/runtime/opal_cr.h"

#include "orte/mca/errmgr/base/base.h"
#include "orte/mca/ess/base/base.h"
#include "orte/mca/rml/base/base.h"
#include "orte/mca/rml/base/rml_contact.h"
#include "orte/mca/routed/base/base.h"
#include "orte/mca/routed/routed.h"
#include "orte/mca/oob/base/base.h"
#include "orte/mca/plm/base/base.h"
#include "orte/mca/plm/base/plm_private.h"
#include "orte/mca/rmaps/base/base.h"
#include "orte/mca/rmaps/rmaps.h"
#include "orte/mca/state/base/base.h"
#include "orte/mca/iof/base/base.h"
#include "orte/mca/ras/base/base.h"
#include "orte/mca/grpcomm/base/base.h"
#include "orte/mca/odls/base/base.h"
#include "orte/mca/rtc/base/base.h"
#include "orte/mca/dfs/base/base.h"
#include "orte/util/regex.h"
#include "orte/util/proc_info.h"
#include "orte/util/show_help.h"
#include "orte/util/session_dir.h"
#include "orte/util/hnp_contact.h"
#include "orte/util/name_fns.h"
#include "orte/util/nidmap.h"
#include "orte/util/comm/comm.h"
#include "orte/runtime/orte_wait.h"
#include "orte/runtime/orte_cr.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/scd/base/base.h"
#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/db/base/base.h"
#include "orcm/util/utils.h"

#include "orcm/mca/sst/base/base.h"
#include "orcm/mca/sst/orcmsd/sst_orcmsd.h"

#define ORCM_SST_TOOL_MAX_LINE_LENGTH 1024

/* API functions */

static int orcmsd_init(void);
static int orcmsd_setup_node_pool(void);
static void orcmsd_finalize(void);

/* The module struct */

orcm_sst_base_module_t orcm_sst_orcmsd_module = {
    orcmsd_init,
    orcmsd_finalize
};

/* local globals */
static bool plm_in_use = false;
static bool initialized = false;
static bool signals_set = false;
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

static int orcmsd_init(void)
{
    int ret;
    int32_t n;
    char *error;
    char *contact_path, *jobfam_dir;
    char *param;
    opal_buffer_t buf, *clusterbuf, *uribuf;
    opal_list_t config;
    orte_vpid_t nprocs;
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

    /* define a name for myself */
    /* if we were spawned by a singleton, our jobid was given to us */
    if (NULL != orcm_base_jobid) {
        if (ORTE_SUCCESS != (ret = orte_util_convert_string_to_jobid(&ORTE_PROC_MY_NAME->jobid, orcm_base_jobid))) {
            ORTE_ERROR_LOG(ret);
            error = "convert_string_to_jobid";
            goto error;
        }
        if (ORTE_PROC_IS_HNP) {
            ORTE_PROC_MY_NAME->vpid = 0;
        } else {
            if(NULL == orcm_base_vpid) {
                ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
                return ORTE_ERR_NOT_FOUND;
            }
            if (ORTE_SUCCESS != (ret = orte_util_convert_string_to_vpid(&ORTE_PROC_MY_NAME->vpid, orcm_base_vpid))) {
                ORTE_ERROR_LOG(ret);
                error = "convert_string_to_vpid";
                goto error;
            }
        }

    } else {
        if (ORTE_SUCCESS != (ret = orte_plm.set_hnp_name())) {
            ORTE_ERROR_LOG(ret);
            error = "orte_plm_set_hnp_name";
            goto error;
        }
    }

    /* setup the database */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orcm_db_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orcm_db_base_open";
        goto error;
    }
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

    /* read the site configuration */
    OBJ_CONSTRUCT(&config, opal_list_t);
    if (ORCM_SUCCESS != (ret = orcm_cfgi.read_config(&config))) {
        error = "getting config";
        goto error;
    }
    /* define the cluster */
    OBJ_CONSTRUCT(&buf, opal_buffer_t);
    if (ORCM_SUCCESS != (ret = orcm_cfgi.define_system(&config, &mynode, &nprocs, &buf))) {
        error = "define system";
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
        
        if (4 < opal_output_get_verbosity(orte_ess_base_framework.framework_output)) {
            opal_output(0, "%s Topology Info:", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            opal_dss.dump(0, opal_hwloc_topology, OPAL_HWLOC_TOPO);
        }
    }
#endif
    
    /* open and setup the opal_pstat framework so we can provide
     * process stats if requested
     */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&opal_pstat_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "opal_pstat_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = opal_pstat_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "opal_pstat_base_select";
        goto error;
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

    if (ORTE_PROC_IS_HNP) {
    /* Since we are the HNP, then responsibility for
     * defining the name falls to the PLM component for our
     * respective environment - hence, we have to open the PLM
     * first and select that component.
     */
        plm_in_use = 1;
    } else {
    /* some environments allow remote launches - e.g., ssh - so
     * open and select something -only- if we are given
     * a specific module to use
     */
        (void) mca_base_var_env_name("plm", &param);

        plm_in_use = !!(getenv(param));
        free (param);
    }

    if (plm_in_use)  {
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
    }


    
    /* Setup the communication infrastructure */
   
    /*
     * OOB Layer
     */
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

    /*
     * Runtime Messaging Layer 
     */
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

    /* setup job data object and node pool array */
    if (ORTE_SUCCESS != (ret = orcmsd_setup_node_pool())) {
        ORTE_ERROR_LOG(ret);
        error = "orcmsd_setup_node_pool";
        goto error;
    }
 
    /* Routed system */
    if (NULL == getenv("OMPI_MCA_routed")) {
        putenv("OMPI_MCA_routed=radix");
    }

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
    if (plm_in_use) {
        if (ORTE_SUCCESS != (ret = orte_plm.init())) {
            ORTE_ERROR_LOG(ret);
            error = "orte_plm_init";
            goto error;
        }
    }

    /*
     * Setup the remaining resource
     * management and errmgr frameworks - application procs
     * and daemons do not open these frameworks as they only use
     * the hnp proxy support in the PLM framework.
     */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_ras_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_ras_base_open";
        goto error;
    }    
    if (ORTE_SUCCESS != (ret = orte_ras_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_ras_base_find_available";
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
    
    /* Open/select the rtc */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_rtc_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rtc_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_rtc_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rtc_base_select";
        goto error;
    }

    if (ORCM_PROC_IS_HNP) {
    /* we are an hnp, so update the contact info field for later use */
        orte_process_info.my_hnp_uri = orte_rml.get_contact_info();

    /* we are also officially a daemon, so better update that field too */
        orte_process_info.my_daemon_uri = strdup(orte_process_info.my_hnp_uri);
    } else {
        orte_process_info.my_daemon_uri = orte_rml.get_contact_info();
    }
    
    orte_create_session_dirs = false;

    /* extract the cluster description and setup the routed info - the orcm routed component
     * will know what to do. */
    n = 1;
    if (OPAL_SUCCESS != (ret = opal_dss.unpack(&buf, &clusterbuf, &n, OPAL_BUFFER))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        error = "extract cluster buf";
        goto error;
    }
    /*
    if (ORTE_SUCCESS != (ret = orte_routed.init_routes(ORTE_PROC_MY_NAME->jobid, clusterbuf))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&buf);
        OBJ_RELEASE(clusterbuf);
        error = "orte_routed.init_routes";
        goto error;
    }
    */
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

    /* enable communication via the rml */
    if (ORTE_SUCCESS != (ret = orte_rml.enable_comm())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rml.enable_comm";
        goto error;
    }

    /* setup the routed info - the selected routed component
     * will know what to do. 
     */
    if (ORTE_SUCCESS != (ret = orte_routed.init_routes(ORTE_PROC_MY_NAME->jobid, NULL))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_routed.init_routes";
        goto error;
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

    opal_cr_set_enabled(false);

    /*
     * Initalize the CR setup
     * Note: Always do this, even in non-FT builds.
     * If we don't some user level tools may hang.
     */
    if (ORTE_SUCCESS != (ret = orte_cr_init())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_cr_init";
        goto error;
    }
    
    /* setup the dfs framework */
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

    /* if a tool has launched us and is requesting event reports,
     * then set its contact info into the comm system
     */
    if (orte_report_events) {
        if (ORTE_SUCCESS != (ret = orte_util_comm_connect_tool(orte_report_events_uri))) {
            error = "could not connect to tool";
            goto error;
        }
    }

    /* We actually do *not* want an HNP to voluntarily yield() the
       processor more than necessary.  Orterun already blocks when
       it is doing nothing, so it doesn't use any more CPU cycles than
       it should; but when it *is* doing something, we do not want it
       to be unnecessarily delayed because it voluntarily yielded the
       processor in the middle of its work.
     
       For example: when a message arrives at orterun, we want the
       OS to wake us up in a timely fashion (which most OS's
       seem good about doing) and then we want orterun to process
       the message as fast as possible.  If orterun yields and lets
       aggressive MPI applications get the processor back, it may be a
       long time before the OS schedules orterun to run again
       (particularly if there is no IO event to wake it up).  Hence,
       routed OOB messages (for example) may be significantly delayed
       before being delivered to MPI processes, which can be
       problematic in some scenarios (e.g., COMM_SPAWN, BTL's that
       require OOB messages for wireup, etc.). */
    opal_progress_set_yield_when_idle(false);

    return ORTE_SUCCESS;

 error:
    if (ORTE_ERR_SILENT != ret && !orte_report_silent_errors) {
        orte_show_help("help-orte-runtime.txt",
                       "orte_init:startup:internal-failure",
                       true, error, ORTE_ERROR_NAME(ret), ret);
    }
}

static int orcmsd_setup_node_pool(void)
{
    int ret = ORTE_ERROR;
    int i = 0;
    char *error = NULL;
    orte_job_t *jdata;
    orte_proc_t *proc;
    orte_app_context_t *app;
    orte_node_t *node;
    char **hosts = NULL;
    char *myhostname[2] = { NULL, NULL};

    if (NULL != orcm_node_regex) {
    /* extract the nodes */
        if (ORTE_SUCCESS != (ret = orte_regex_extract_node_names(orcm_node_regex, &hosts))) {
            error = "orte_regex_extract_node_names";
            goto error;
        }
    } else {
        myhostname[0] = strdup( orte_process_info.nodename );
        hosts = myhostname;
    }

    /* setup the global job and node arrays */
    orte_job_data = OBJ_NEW(opal_pointer_array_t);
    if (ORTE_SUCCESS != (ret = opal_pointer_array_init(orte_job_data,
        1, ORTE_GLOBAL_ARRAY_MAX_SIZE, 1))) {
        ORTE_ERROR_LOG(ret);
        error = "setup job array";
        goto error;
    }
    
    orte_node_pool = OBJ_NEW(opal_pointer_array_t);
    if (ORTE_SUCCESS != (ret = opal_pointer_array_init(orte_node_pool,
        ORTE_GLOBAL_ARRAY_BLOCK_SIZE, ORTE_GLOBAL_ARRAY_MAX_SIZE,
        ORTE_GLOBAL_ARRAY_BLOCK_SIZE))) {
        ORTE_ERROR_LOG(ret);
        error = "setup node array";
    }
    orte_node_topologies = OBJ_NEW(opal_pointer_array_t);
    if (ORTE_SUCCESS != (ret = opal_pointer_array_init(orte_node_topologies,
        ORTE_GLOBAL_ARRAY_BLOCK_SIZE, ORTE_GLOBAL_ARRAY_MAX_SIZE,
        ORTE_GLOBAL_ARRAY_BLOCK_SIZE))) {
        ORTE_ERROR_LOG(ret);
        error = "setup node topologies array";
        goto error;
    }

    /* init the nidmap - just so we register that verbosity */
    orte_util_nidmap_init(NULL);


    /* Setup the job data object for the daemons */        
    /* create and store the job data object */
    jdata = OBJ_NEW(orte_job_t);
    jdata->jobid = ORTE_PROC_MY_NAME->jobid;
    opal_pointer_array_set_item(orte_job_data, 0, jdata);
    
    /* every job requires at least one app */
    app = OBJ_NEW(orte_app_context_t);
    opal_pointer_array_set_item(jdata->apps, 0, app);
    jdata->num_apps++;
    
    for(i=0; hosts[i] != NULL; i++) {
        /* create and store a node object where we are */
        node = OBJ_NEW(orte_node_t);
        node->name = strdup(hosts[i]);
        node->index = opal_pointer_array_set_item(orte_node_pool, i, node);
#if OPAL_HAVE_HWLOC
        /* point our topology to the one detected locally */
        node->topology = opal_hwloc_topology;
#endif

        /* create and store a proc object for us */
        proc = OBJ_NEW(orte_proc_t);
        proc->name.jobid = ORTE_PROC_MY_NAME->jobid;
        proc->name.vpid = i;
    
        proc->pid = orte_process_info.pid;
        proc->rml_uri = orte_rml.get_contact_info();
        proc->state = ORTE_PROC_STATE_RUNNING;
        opal_pointer_array_set_item(jdata->procs, i, proc);
    
        /* record that the daemon (i.e., us) is on this node 
         * NOTE: we do not add the proc object to the node's
         * proc array because we are not an application proc.
         * Instead, we record it in the daemon field of the
         * node object
         */
        OBJ_RETAIN(proc);   /* keep accounting straight */
        node->daemon = proc;
        ORTE_FLAG_SET(node, ORTE_NODE_FLAG_DAEMON_LAUNCHED);
        node->state = ORTE_NODE_STATE_UP;
    
        /* now point our proc node field to the node */
        OBJ_RETAIN(node);   /* keep accounting straight */
        proc->node = node;

        /* record that the daemon job is running */
        jdata->num_procs++;
        jdata->state = ORTE_JOB_STATE_RUNNING;
        /* num reported only one for now */
        jdata->num_reported = 1;
    }
    

    return ORTE_SUCCESS;
    
 error:
    orte_show_help("help-orte-runtime.txt",
                   "orte_init:startup:internal-failure",
                   true, error, ORTE_ERROR_NAME(ret), ret);
    
    return ORTE_ERR_SILENT;
}

static void orcmsd_finalize(void)
{
    if (signals_set) {
        /* Release all local signal handlers */
        opal_event_del(&epipe_handler);
        opal_event_del(&term_handler);
        opal_event_del(&int_handler);
        opal_event_signal_del(&sigusr1_handler);
        opal_event_signal_del(&sigusr2_handler);
    }
    
    (void) mca_base_framework_close(&orcm_db_base_framework);
    (void) mca_base_framework_close(&opal_dstore_base_framework);
    (void) mca_base_framework_close(&opal_pstat_base_framework);
    (void) mca_base_framework_close(&orte_state_base_framework);
    (void) mca_base_framework_close(&orte_errmgr_base_framework);
    (void) mca_base_framework_close(&orte_plm_base_framework);
    (void) mca_base_framework_close(&orte_oob_base_framework);
    (void) mca_base_framework_close(&orte_rml_base_framework);
    (void) mca_base_framework_close(&orte_routed_base_framework);
    (void) mca_base_framework_close(&orte_grpcomm_base_framework);
    (void) mca_base_framework_close(&orte_ras_base_framework);
    (void) mca_base_framework_close(&orte_rmaps_base_framework);
    (void) mca_base_framework_close(&orte_odls_base_framework);
    (void) mca_base_framework_close(&orte_rtc_base_framework);
    (void) mca_base_framework_close(&orte_iof_base_framework);
    (void) mca_base_framework_close(&orte_dfs_base_framework);

}

static void* progress_thread_engine(opal_object_t *obj)
{
    while (orte_event_base_active) {
        opal_event_loop(orte_event_base, OPAL_EVLOOP_ONCE);
    }
    return OPAL_THREAD_CANCELLED;
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

