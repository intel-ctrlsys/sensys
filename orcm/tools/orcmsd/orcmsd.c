/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007      Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif  /* HAVE_SYS_TIME_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif


#include "opal/mca/event/event.h"
#include "opal/mca/base/base.h"
#include "opal/util/output.h"
#include "opal/util/cmd_line.h"
#include "opal/util/if.h"
#include "opal/util/net.h"
#include "opal/util/opal_environ.h"
#include "opal/util/os_path.h"
#include "opal/util/printf.h"
#include "opal/util/argv.h"
#include "opal/runtime/opal.h"
#include "opal/mca/base/mca_base_var.h"
#include "opal/util/daemon_init.h"
#include "opal/dss/dss.h"
#include "opal/mca/hwloc/hwloc.h"
#include "opal/hash_string.h"

#include "orte/util/show_help.h"
#include "orte/util/proc_info.h"
#include "orte/util/session_dir.h"
#include "orte/util/name_fns.h"
#include "orte/util/nidmap.h"
#include "orte/util/parse_options.h"
#include "orte/mca/rml/base/rml_contact.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/ess/ess.h"
#include "orte/mca/grpcomm/grpcomm.h"
#include "orte/mca/grpcomm/base/base.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/rml/rml_types.h"
#include "orte/mca/odls/odls.h"
#include "orte/mca/odls/base/odls_private.h"
#include "orte/mca/plm/plm.h"
#include "orte/mca/ras/ras.h"
#include "orte/mca/routed/routed.h"
#include "orte/mca/rmaps/rmaps_types.h"
#include "orte/mca/state/state.h"

/* need access to the create_jobid fn used by plm components
* so we can set singleton name, if necessary
*/
#include "orte/mca/plm/base/plm_private.h"
#include "orte/runtime/runtime.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_locks.h"
#include "orte/runtime/orte_quit.h"

#include "orcm/mca/sst/sst.h"
#include "orcm/mca/sst/base/base.h"
#include "orcm/runtime/runtime.h"

/*
 * Globals
 */
static opal_event_t *pipe_handler;
static opal_pointer_array_t *procs_prev_ordered_to_terminate = NULL;
static char *orte_parent_uri;
static bool orted_failed_launch;
static orte_job_t *jdatorted=NULL;

int orcms_daemon(int argc, char *argv[]);
static void shutdown_callback(int fd, short flags, void *arg);
static void pipe_closed(int fd, short flags, void *arg);
static char *get_orcmsd_comm_cmd_str(int command);
void orcms_hnp_recv(int status, orte_process_name_t* sender,
                      opal_buffer_t *buffer, orte_rml_tag_t tag,
                      void* cbdata);
void orcms_daemon_recv(int status, orte_process_name_t* sender,
                      opal_buffer_t *buffer, orte_rml_tag_t tag,
                      void* cbdata);


static struct {
    bool debug;
    bool help;
    bool set_sid;
    bool hnp;
    bool daemonize;
    char* name;
    char* vpid_start;
    char* num_procs;
    int uri_pipe;
    int singleton_died_pipe;
    int fail;
    int fail_delay;
    bool abort;
    bool mapreduce;
    bool tree_spawn;
} orcmsd_globals;

/*
 * define the orted context table for obtaining parameters
 */
opal_cmd_line_init_t orcmsd_cmd_line_opts[] = {
    /* Various "obvious" options */
    { NULL, 'h', NULL, "help", 0,
      &orcmsd_globals.help, OPAL_CMD_LINE_TYPE_BOOL,
      "This help message" },
    { NULL, '\0', NULL, "debug-failure", 1,
      &orcmsd_globals.fail, OPAL_CMD_LINE_TYPE_INT,
      "Have the specified orted fail after init for debugging purposes" },
    
    { NULL, '\0', NULL, "debug-failure-delay", 1,
      &orcmsd_globals.fail_delay, OPAL_CMD_LINE_TYPE_INT,
      "Have the orted specified for failure delay for the provided number of seconds before failing" },
    
    { "orte_debug", 'd', NULL, "debug", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Debug the OpenRTE" },
        
    { "orte_daemonize", '\0', NULL, "daemonize", 0,
      &orcmsd_globals.daemonize, OPAL_CMD_LINE_TYPE_BOOL,
      "Daemonize the orted into the background" },

    { "orte_debug_daemons", '\0', NULL, "debug-daemons", 0,
      &orcmsd_globals.debug, OPAL_CMD_LINE_TYPE_BOOL,
      "Enable debugging of OpenRTE daemons" },

    { "orte_debug_daemons_file", '\0', NULL, "debug-daemons-file", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Enable debugging of OpenRTE daemons, storing output in files" },

    { NULL, '\0', NULL, "hnp", 0,
      &orcmsd_globals.hnp, OPAL_CMD_LINE_TYPE_BOOL,
      "Direct the orted to act as the HNP"},

    { "orte_hnp_uri", '\0', NULL, "hnp-uri", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "URI for the HNP"},
    
    { "orte_parent_uri", '\0', NULL, "parent-uri", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "URI for the parent if tree launch is enabled."},
    
    { NULL, '\0', NULL, "set-sid", 0,
      &orcmsd_globals.set_sid, OPAL_CMD_LINE_TYPE_BOOL,
      "Direct the orted to separate from the current session"},
    
    { NULL, '\0', NULL, "report-uri", 1,
      &orcmsd_globals.uri_pipe, OPAL_CMD_LINE_TYPE_INT,
      "Report this process' uri on indicated pipe"},

    { NULL, '\0', NULL, "singleton-died-pipe", 1,
      &orcmsd_globals.singleton_died_pipe, OPAL_CMD_LINE_TYPE_INT,
      "Watch on indicated pipe for singleton termination"},
    
    { "orte_output_filename", '\0', "output-filename", "output-filename", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Redirect output from application processes into filename.rank" },
    
    { "orcm_node_regex", '\0', "nodes", "nodes", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Regular expression defining nodes in system" },

    /* End of list */
    { NULL, '\0', NULL, NULL, 0,
      NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }
};

int main(int argc, char *argv[])
{
    int ret = 0;
    opal_cmd_line_t *cmd_line = NULL;
    char *rml_uri;
    int i;
    opal_buffer_t *buffer;
    char hostname[100];
    char *umask_str = getenv("ORTE_DAEMON_UMASK_VALUE");
    if (NULL != umask_str) {
        char *endptr;
        long mask = strtol(umask_str, &endptr, 8);
        if ((! (0 == mask && (EINVAL == errno || ERANGE == errno))) &&
            (*endptr == '\0')) {
            umask(mask);
        }
    }

    memset(&orcmsd_globals, 0, sizeof(orcmsd_globals));
    /* initialize the singleton died pipe to an illegal value so we can detect it was set */
    orcmsd_globals.singleton_died_pipe = -1;
    /* init the failure orted vpid to an invalid value */
    orcmsd_globals.fail = ORTE_VPID_INVALID;
    
    /* setup to check common command line options that just report and die */
    cmd_line = OBJ_NEW(opal_cmd_line_t);
    if (OPAL_SUCCESS != opal_cmd_line_create(cmd_line, orcmsd_cmd_line_opts)) {
        OBJ_RELEASE(cmd_line);
        exit(1);
    }
    mca_base_cmd_line_setup(cmd_line);
    if (ORTE_SUCCESS != (ret = opal_cmd_line_parse(cmd_line, false,
                                                   argc, argv))) {
        char *args = NULL;
        args = opal_cmd_line_get_usage_msg(cmd_line);
        fprintf(stderr, "Usage: %s [OPTION]...\n%s\n", argv[0], args);
        free(args);
        OBJ_RELEASE(cmd_line);
        return ret;
    }
    
    /*
     * Since this process can now handle MCA/GMCA parameters, make sure to
     * process them.
     */
    mca_base_cmd_line_process_args(cmd_line, &environ, &environ);
    
    /* Ensure that enough of OPAL is setup for us to be able to run */
    /*
     * NOTE: (JJH)
     *  We need to allow 'mca_base_cmd_line_process_args()' to process command
     *  line arguments *before* calling opal_init_util() since the command
     *  line could contain MCA parameters that affect the way opal_init_util()
     *  functions. AMCA parameters are one such option normally received on the
     *  command line that affect the way opal_init_util() behaves.
     *  It is "safe" to call mca_base_cmd_line_process_args() before 
     *  opal_init_util() since mca_base_cmd_line_process_args() does *not*
     *  depend upon opal_init_util() functionality.
     */
    if (OPAL_SUCCESS != opal_init_util(&argc, &argv)) {
        fprintf(stderr, "OPAL failed to initialize -- orted aborting\n");
        exit(1);
    }

    /* save the environment for launch purposes. This MUST be
     * done so that we can pass it to any local procs we
     * spawn - otherwise, those local procs won't see any
     * non-MCA envars that were set in the enviro when the
     * orted was executed - e.g., by .csh
     */
    orte_launch_environ = opal_argv_copy(environ);
    
    /* purge any ess flag set in the environ when we were launched */
    opal_unsetenv("OMPI_MCA_ess", &orte_launch_environ);
    
    /* if orte_daemon_debug is set, let someone know we are alive right
     * away just in case we have a problem along the way
     */
    if (orcmsd_globals.debug) {
        gethostname(hostname, 100);
        fprintf(stderr, "Daemon was launched on %s - beginning to initialize\n", hostname);
    }
    
    /* check for help request */
    if (orcmsd_globals.help) {
        char *args = NULL;
        args = opal_cmd_line_get_usage_msg(cmd_line);
        orte_show_help("help-orted.txt", "orted:usage", false,
                       argv[0], args);
        free(args);
        return 1;
    }
#if defined(HAVE_SETSID)
    /* see if we were directed to separate from current session */
    if (orcmsd_globals.set_sid) {
        setsid();
    }
#endif


    /* detach from controlling terminal
     * otherwise, remain attached so output can get to us
     */
    if(!orte_debug_flag &&
       !orte_debug_daemons_flag &&
       orcmsd_globals.daemonize) {
        opal_daemon_init(NULL);
    }
    
    /* Set the flag telling OpenRTE that I am NOT a
     * singleton, but am "infrastructure" - prevents setting
     * up incorrect infrastructure that only a singleton would
     * require.
     */
    if(orcmsd_globals.hnp) {
        if (ORCM_SUCCESS != (ret = orcm_init(ORCM_HNP))) {
            return ret;
        }
    } else {
        if (ORCM_SUCCESS != (ret = orcm_init(ORCM_STEPD))) {
            return ret;
        }
    }

    /* finalize the OPAL utils. As they are opened again from orte_init->opal_init
     * we continue to have a reference count on them. So we have to finalize them twice...
     */
    opal_finalize_util();

#if OPAL_HAVE_HWLOC
    /* bind ourselves if so directed */
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
                    /* turn off the show help forwarding as we won't
                     * be able to cycle the event library to send
                     */
                    orte_show_help_finalize();
                    /* the message will now come out locally */
                    orte_show_help("help-orted.txt", "orted:cannot-bind",
                                   true, orte_process_info.nodename,
                                   orte_daemon_cores);
                    ret = ORTE_ERR_NOT_SUPPORTED;
                    goto DONE;
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
#endif

    /* insert our contact info into our process_info struct so we
     * have it for later use and set the local daemon field to our name
     */
    orte_process_info.my_daemon_uri = orte_rml.get_contact_info();
    ORTE_PROC_MY_DAEMON->jobid = ORTE_PROC_MY_NAME->jobid;
    ORTE_PROC_MY_DAEMON->vpid = ORTE_PROC_MY_NAME->vpid;
    
    /* if I am also the hnp, then update that contact info field too */
    if (ORTE_PROC_IS_HNP) {
        orte_process_info.my_hnp_uri = orte_rml.get_contact_info();
        ORTE_PROC_MY_HNP->jobid = ORTE_PROC_MY_NAME->jobid;
        ORTE_PROC_MY_HNP->vpid = ORTE_PROC_MY_NAME->vpid;

        /* setup the orcms ctrl or hnp daemon command receive function */
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORCM_RML_TAG_HNP,
                            ORTE_RML_PERSISTENT, orcms_hnp_recv, NULL);
    } else {
        (void) mca_base_var_register ("orte", "orte", NULL, "hnp_uri",
                                  "URI for the hnp.",
                                  MCA_BASE_VAR_TYPE_STRING, NULL, 0,
                                  MCA_BASE_VAR_FLAG_INTERNAL,
                                  OPAL_INFO_LVL_9,
                                  MCA_BASE_VAR_SCOPE_CONSTANT,
                                  &orte_process_info.my_hnp_uri);
        if (NULL != orte_process_info.my_hnp_uri) {
            /* set the contact info into the hash table */
            orte_rml.set_contact_info(orte_process_info.my_hnp_uri);
            ret = orte_rml_base_parse_uris(orte_process_info.my_hnp_uri, ORTE_PROC_MY_HNP, NULL);
            if (ORTE_SUCCESS != ret) {
                ORTE_ERROR_LOG(ret);
                goto DONE;
            }
        }
    }
    
    /* setup the primary daemon command receive function */
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_DAEMON,
                            ORTE_RML_PERSISTENT, orcms_daemon_recv, NULL);
    
    /* output a message indicating we are alive, our name, and our pid
     * for debugging purposes
     */
    if (orte_debug_daemons_flag) {
        fprintf(stderr, "Daemon %s checking in as pid %ld on host %s\n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), (long)orte_process_info.pid,
                orte_process_info.nodename);
    }

    /* We actually do *not* want the orted to voluntarily yield() the
       processor more than necessary.  The orted already blocks when
       it is doing nothing, so it doesn't use any more CPU cycles than
       it should; but when it *is* doing something, we do not want it
       to be unnecessarily delayed because it voluntarily yielded the
       processor in the middle of its work.

       For example: when a message arrives at the orted, we want the
       OS to wake up the orted in a timely fashion (which most OS's
       seem good about doing) and then we want the orted to process
       the message as fast as possible.  If the orted yields and lets
       aggressive MPI applications get the processor back, it may be a
       long time before the OS schedules the orted to run again
       (particularly if there is no IO event to wake it up).  Hence,
       routed OOB messages (for example) may be significantly delayed
       before being delivered to MPI processes, which can be
       problematic in some scenarios (e.g., COMM_SPAWN, BTL's that
       require OOB messages for wireup, etc.). */
    opal_progress_set_yield_when_idle(false);

    /* Change the default behavior of libevent such that we want to
       continually block rather than blocking for the default timeout
       and then looping around the progress engine again.  There
       should be nothing in the orted that cannot block in libevent
       until "something" happens (i.e., there's no need to keep
       cycling through progress because the only things that should
       happen will happen in libevent).  This is a minor optimization,
       but what the heck... :-) */
    opal_progress_set_event_flag(OPAL_EVLOOP_ONCE);

    /* if requested, report my uri to the indicated pipe */
    if (orcmsd_globals.uri_pipe > 0) {
        char *sysinfo, *tmp, *nptr;
        orte_job_t *jdata;
        orte_grpcomm_collective_t *coll;
        orte_namelist_t *nm;

    /* get the daemon job, if necessary */
        jdata = orte_get_job_data_object(ORTE_PROC_MY_NAME->vpid);

        /* must create a map for it (even though it has no
         * info in it) so that the job info will be picked
         * up in subsequent pidmaps or other daemons won't
         * know how to route
         */
        jdata->map = OBJ_NEW(orte_job_map_t);

        /* account for the collectives in its modex/barriers */
        jdata->peer_modex = orte_grpcomm_base_get_coll_id();
        coll = orte_grpcomm_base_setup_collective(jdata->peer_modex);
        nm = OBJ_NEW(orte_namelist_t);
        nm->name.jobid = jdata->jobid;
        nm->name.vpid = ORTE_VPID_WILDCARD;
        opal_list_append(&coll->participants, &nm->super);

        jdata->peer_init_barrier = orte_grpcomm_base_get_coll_id();
        coll = orte_grpcomm_base_setup_collective(jdata->peer_init_barrier);
        nm = OBJ_NEW(orte_namelist_t);
        nm->name.jobid = jdata->jobid;
        nm->name.vpid = ORTE_VPID_WILDCARD;
        opal_list_append(&coll->participants, &nm->super);

        jdata->peer_fini_barrier = orte_grpcomm_base_get_coll_id();
        coll = orte_grpcomm_base_setup_collective(jdata->peer_fini_barrier);
        nm = OBJ_NEW(orte_namelist_t);
        nm->name.jobid = jdata->jobid;
        nm->name.vpid = ORTE_VPID_WILDCARD;
        opal_list_append(&coll->participants, &nm->super);

        /* create a string that contains our uri + sysinfo */
        orte_util_convert_sysinfo_to_string(&sysinfo, orte_local_cpu_type, orte_local_cpu_model);
        asprintf(&tmp, "%s[%s]", orte_process_info.my_daemon_uri, sysinfo);
	free(sysinfo);

        /* pass that info to the singleton */
        write(orcmsd_globals.uri_pipe, tmp, strlen(tmp)+1); /* need to add 1 to get the NULL */

        /* cleanup */
        free(tmp);

        /* since a singleton spawned us, we need to harvest
         * any MCA params from the local environment so
         * we can pass them along to any subsequent daemons
         * we may start as the result of a comm_spawn
         */
        for (i=0; NULL != environ[i]; i++) {
            if (0 == strncmp(environ[i], "OMPI_MCA", 8)) {
                /* make a copy to manipulate */
                tmp = strdup(environ[i]);
                /* find the equal sign */
                nptr = strchr(tmp, '=');
                *nptr = '\0';
                nptr++;
                /* add the mca param to the orted cmd line */
                opal_argv_append_nosize(&orted_cmd_line, "-mca");
                opal_argv_append_nosize(&orted_cmd_line, &tmp[9]);
                opal_argv_append_nosize(&orted_cmd_line, nptr);
                free(tmp);
            }
        }
    }

    /* if we were given a pipe to monitor for singleton termination, set that up */
    if (orcmsd_globals.singleton_died_pipe > 0) {
        /* register shutdown handler */
        pipe_handler = (opal_event_t*)malloc(sizeof(opal_event_t));
        opal_event_set(orte_event_base, pipe_handler,
                       orcmsd_globals.singleton_died_pipe,
                       OPAL_EV_READ,
                       pipe_closed,
                       pipe_handler);
        opal_event_add(pipe_handler, NULL);
    }

    /* If I have a parent, then save his contact info so
     * any messages we send can flow thru him.
     */

    orte_parent_uri = NULL;
    (void) mca_base_var_register ("orte", "orte", NULL, "parent_uri",
                                  "URI for the parent if tree launch is enabled.",
                                  MCA_BASE_VAR_TYPE_STRING, NULL, 0,
                                  MCA_BASE_VAR_FLAG_INTERNAL,
                                  OPAL_INFO_LVL_9,
                                  MCA_BASE_VAR_SCOPE_CONSTANT,
                                  &orte_parent_uri);
    if (NULL != orte_parent_uri) {
        orte_process_name_t parent;

        /* set the contact info into the hash table */
        orte_rml.set_contact_info(orte_parent_uri);
        ret = orte_rml_base_parse_uris(orte_parent_uri, &parent, NULL);
        if (ORTE_SUCCESS != ret) {
            ORTE_ERROR_LOG(ret);
            free (orte_parent_uri);
            orte_parent_uri = NULL;
            goto DONE;
        }

        /* don't need this value anymore */
        free(orte_parent_uri);
        orte_parent_uri = NULL;

        /* tell the routed module that we have a path
         * back to the HNP
         */
        if (ORTE_SUCCESS != (ret = orte_routed.update_route(ORTE_PROC_MY_HNP, &parent))) {
            ORTE_ERROR_LOG(ret);
            goto DONE;
        }
        /* set the lifeline to point to our parent so that we
         * can handle the situation if that lifeline goes away
         */
        if (ORTE_SUCCESS != (ret = orte_routed.set_lifeline(&parent))) {
            ORTE_ERROR_LOG(ret);
            goto DONE;
        }
    }

    /* if we are not the HNP...the only time we will be an HNP
     * is if we are launched by a singleton to provide support
     * for it
     */
    if (!ORTE_PROC_IS_HNP) {
        /* send the information to the orted report-back point - this function
         * will process the data, but also counts the number of
         * orteds that reported back so the launch procedure can continue.
         * We need to do this at the last possible second as the HNP
         * can turn right around and begin issuing orders to us
         */

        buffer = OBJ_NEW(opal_buffer_t);
        /* insert our name for rollup purposes */
        if (ORTE_SUCCESS != (ret = opal_dss.pack(buffer, ORTE_PROC_MY_NAME, 1, ORTE_NAME))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(buffer);
            goto DONE;
        }
        /* for now, always include our contact info, even if we are using
         * static ports. Eventually, this will be removed
         */
        rml_uri = orte_rml.get_contact_info();
        if (ORTE_SUCCESS != (ret = opal_dss.pack(buffer, &rml_uri, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(buffer);
            goto DONE;
        }

        /* include our node name */
        opal_dss.pack(buffer, &orte_process_info.nodename, 1, OPAL_STRING);

        /* if requested, include any non-loopback aliases for this node */
        if (orte_retain_aliases) {
            char **aliases=NULL;
            uint8_t naliases, ni;
            char hostname[ORTE_MAX_HOSTNAME_SIZE];

            /* if we stripped the prefix or removed the fqdn,
             * include full hostname as an alias
             */
            gethostname(hostname, ORTE_MAX_HOSTNAME_SIZE);
            if (strlen(orte_process_info.nodename) < strlen(hostname)) {
                opal_argv_append_nosize(&aliases, hostname);
            }
            opal_ifgetaliases(&aliases);
            naliases = opal_argv_count(aliases);
            if (ORTE_SUCCESS != (ret = opal_dss.pack(buffer, &naliases, 1, OPAL_UINT8))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buffer);
                goto DONE;
            }
            for (ni=0; ni < naliases; ni++) {
                if (ORTE_SUCCESS != (ret = opal_dss.pack(buffer, &aliases[ni], 1, OPAL_STRING))) {
                    ORTE_ERROR_LOG(ret);
                    OBJ_RELEASE(buffer);
                    goto DONE;
                }
            }
            opal_argv_free(aliases);
        }

        /* send to the HNP's callback - will be routed if routes are available */
        if (0 > (ret = orte_rml.send_buffer_nb(ORTE_PROC_MY_HNP, buffer,
                                               ORCM_RML_TAG_HNP,
                                               orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(buffer);
            goto DONE;
        }
    }

            
    if (orte_debug_daemons_flag) {
        opal_output(0, "%s orted: up and running - waiting for commands!", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    }
    ret = ORTE_SUCCESS;

    /* loop the event lib until an exit event is detected */
    while (orte_event_base_active) {
        opal_event_loop(orte_event_base, OPAL_EVLOOP_ONCE);
    }

    /* ensure all local procs are dead */
    orte_odls.kill_local_procs(NULL);

 DONE:
    /* update the exit status, in case it wasn't done */
    ORTE_UPDATE_EXIT_STATUS(ret);

    /* cleanup and leave */
    orcm_finalize();

    if (orte_debug_flag) {
        fprintf(stderr, "exiting with status %d\n", orte_exit_status);
    }
    exit(orte_exit_status);
}

static void pipe_closed(int fd, short flags, void *arg)
{
    opal_event_t *ev = (opal_event_t*)arg;

    /* no error here - we just want to terminate */
    opal_event_free(ev);
    ORTE_ACTIVATE_JOB_STATE(NULL, ORTE_JOB_STATE_DAEMONS_TERMINATED);
}

static void shutdown_callback(int fd, short flags, void *arg)
{
    orte_timer_t *tm = (orte_timer_t*)arg;

    if (NULL != tm) {
        /* release the timer */
        OBJ_RELEASE(tm);
    }
    
    /* if we were ordered to abort, do so */
    if (orcmsd_globals.abort) {
        opal_output(0, "%s is executing clean abort", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        /* do -not- call finalize as this will send a message to the HNP
         * indicating clean termination! Instead, just kill our
         * local procs, forcibly cleanup the local session_dir tree, and abort
         */
        orte_odls.kill_local_procs(NULL);
        abort();
    }
    opal_output(0, "%s is executing clean abnormal termination", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    /* do -not- call finalize as this will send a message to the HNP
     * indicating clean termination! Instead, just forcibly cleanup
     * the local session_dir tree and exit
     */
    orte_odls.kill_local_procs(NULL);
    exit(ORTE_ERROR_DEFAULT_EXIT_CODE);
}


void orcms_hnp_recv(int status, orte_process_name_t* sender,
                      opal_buffer_t *buffer, orte_rml_tag_t tag,
                      void* cbdata)
{
    char *rml_uri = NULL, *ptr;
    int rc, idx;
    orte_proc_t *daemon=NULL;
    char *nodename;
    orte_node_t *node;
    orte_job_t *jdata;
    orte_process_name_t dname;
    opal_buffer_t *relay;

    /* get the daemon job, if necessary */
    if (NULL == jdatorted) {
        jdatorted = orte_get_job_data_object(ORTE_PROC_MY_NAME->vpid);
    }

    /* multiple daemons could be in this buffer, so unpack until we exhaust the data */
    idx = 1;
    while (OPAL_SUCCESS == (rc = opal_dss.unpack(buffer, &dname, &idx, ORTE_NAME))) {
        /* unpack its contact info */
        idx = 1;
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &rml_uri, &idx, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            orted_failed_launch = true;
            goto CLEANUP;
        }
        
        /* set the contact info into the hash table */
        orte_rml.set_contact_info(rml_uri);

        OPAL_OUTPUT_VERBOSE((5, orte_plm_base_framework.framework_output,
                             "%s plm:base:orted_report_launch from daemon %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_NAME_PRINT(&dname)));
        
        /* update state and record for this daemon contact info */
        if (NULL == (daemon = (orte_proc_t*)opal_pointer_array_get_item(jdatorted->procs, dname.vpid))) {
            ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
            orted_failed_launch = true;
            goto CLEANUP;
        }
        daemon->state = ORTE_PROC_STATE_RUNNING;
        daemon->rml_uri = rml_uri;
        /* record that this daemon is alive */
        ORTE_FLAG_SET(daemon, ORTE_PROC_FLAG_ALIVE);

        /* unpack the node name */
        idx = 1;
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &nodename, &idx, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            orted_failed_launch = true;
            goto CLEANUP;
        }
        if (!orte_have_fqdn_allocation) {
            /* remove any domain info */
            if (NULL != (ptr = strchr(nodename, '.'))) {
                *ptr = '\0';
                ptr = strdup(nodename);
                free(nodename);
                nodename = ptr;
            }
        }
        
        OPAL_OUTPUT_VERBOSE((5, orte_plm_base_framework.framework_output,
                             "%s plm:base:orted_report_launch from daemon %s on node %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_NAME_PRINT(&dname), nodename));
        
        /* look this node up, if necessary */
        if (!orte_plm_globals.daemon_nodes_assigned_at_launch) {
            OPAL_OUTPUT_VERBOSE((5, orte_plm_base_framework.framework_output,
                                 "%s plm:base:orted_report_launch attempting to assign daemon %s to node %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 ORTE_NAME_PRINT(&dname), nodename));
            for (idx=0; idx < orte_node_pool->size; idx++) {
                if (NULL == (node = (orte_node_t*)opal_pointer_array_get_item(orte_node_pool, idx))) {
                    continue;
                }
                if (ORTE_FLAG_TEST(node, ORTE_NODE_FLAG_LOC_VERIFIED)) {
                    /* already assigned */
                    continue;
                }
                if (0 == strcmp(nodename, node->name)) {
                    /* flag that we verified the location */
                    ORTE_FLAG_SET(node, ORTE_NODE_FLAG_LOC_VERIFIED);
                    if (node == daemon->node) {
                        /* it wound up right where it should */
                        break;
                    }
                    /* remove the prior association */
                    if (NULL != daemon->node) {
                        OBJ_RELEASE(daemon->node);
                    }
                    if (NULL != node->daemon) {
                        OBJ_RELEASE(node->daemon);
                    }
                    /* associate this daemon with the node */
                    node->daemon = daemon;
                    OBJ_RETAIN(daemon);
                    /* associate this node with the daemon */
                    daemon->node = node;
                    OBJ_RETAIN(node);
                    OPAL_OUTPUT_VERBOSE((5, orte_plm_base_framework.framework_output,
                                         "%s plm:base:orted_report_launch assigning daemon %s to node %s",
                                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                         ORTE_NAME_PRINT(&daemon->name), node->name));
                    break;
                }
            }
        }

        node = daemon->node;
        if (NULL == node) {
            /* this shouldn't happen - it indicates an error in the
             * prior node matching logic, so report it and error out
             */
            orte_show_help("help-plm-base.txt", "daemon-no-assigned-node", true,
                           ORTE_NAME_PRINT(&daemon->name), nodename);
            orted_failed_launch = true;
            goto CLEANUP;
        }
        
        /* mark the daemon as launched */
        ORTE_FLAG_SET(node, ORTE_NODE_FLAG_DAEMON_LAUNCHED);

        if (orte_retain_aliases) {
            char *alias, **atmp=NULL;
            uint8_t naliases, ni;
            /* first, store the nodename itself as an alias. We do
             * this in case the nodename isn't the same as what we
             * were given by the allocation. For example, a hostfile
             * might contain an IP address instead of the value returned
             * by gethostname, yet the daemon will have returned the latter
             * and apps may refer to the host by that name
             */
            opal_argv_append_nosize(&atmp, nodename);
            /* unpack and store the provided aliases */
            idx = 1;
            if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &naliases, &idx, OPAL_UINT8))) {
                ORTE_ERROR_LOG(rc);
                orted_failed_launch = true;
                goto CLEANUP;
            }
            for (ni=0; ni < naliases; ni++) {
                idx = 1;
                if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &alias, &idx, OPAL_STRING))) {
                    ORTE_ERROR_LOG(rc);
                    orted_failed_launch = true;
                    goto CLEANUP;
                }
                opal_argv_append_nosize(&atmp, alias);
                free(alias);
            }
            alias = opal_argv_join(atmp, ',');
            opal_argv_free(atmp);
            orte_set_attribute(&node->attributes, ORTE_NODE_ALIAS, ORTE_ATTR_LOCAL, alias, OPAL_STRING);
            free(alias);
        }

    CLEANUP:
        OPAL_OUTPUT_VERBOSE((5, orte_plm_base_framework.framework_output,
                             "%s plm:base:orted_report_launch %s for daemon %s at contact %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             orted_failed_launch ? "failed" : "completed",
                             ORTE_NAME_PRINT(&dname),
                             (NULL == daemon) ? "UNKNOWN" : daemon->rml_uri));
        
        jdatorted->num_reported++;
        if (jdatorted->num_procs == jdatorted->num_reported) {
            jdatorted->state = ORTE_JOB_STATE_DAEMONS_REPORTED;
            /* activate the daemons_reported state for all jobs
             * whose daemons were launched
             */
            for (idx=1; idx < orte_job_data->size; idx++) {
                if (NULL == (jdata = (orte_job_t*)opal_pointer_array_get_item(orte_job_data, idx))) {
                    continue;
                }
                if (ORTE_JOB_STATE_DAEMONS_LAUNCHED == jdata->state) {
                    ORTE_ACTIVATE_JOB_STATE(jdata, ORTE_JOB_STATE_DAEMONS_REPORTED);
                }
            }
        }
        idx = 1;
    }
    if (ORTE_ERR_UNPACK_READ_PAST_END_OF_BUFFER != rc) {
        ORTE_ERROR_LOG(rc);
        ORTE_ACTIVATE_JOB_STATE(jdatorted, ORTE_JOB_STATE_FAILED_TO_START);
    } else if (NULL != orte_tree_launch_cmd) {
        /* if a tree-launch is underway, send the cmd back */
        relay = OBJ_NEW(opal_buffer_t);
        opal_dss.copy_payload(relay, orte_tree_launch_cmd);
        orte_rml.send_buffer_nb(sender, relay,
                                ORTE_RML_TAG_DAEMON,
                                orte_rml_send_callback, NULL);
    }
}

void orcms_daemon_recv(int status, orte_process_name_t* sender,
                      opal_buffer_t *buffer, orte_rml_tag_t tag,
                      void* cbdata)
{
    orte_daemon_cmd_flag_t command;
    opal_buffer_t *relay_msg;
    int ret;
    orte_std_cntr_t n;
    int32_t signal;
    orte_jobid_t job;
    orte_rml_tag_t target_tag;
    char *contact_info;
    opal_buffer_t *answer;
    orte_rml_cmd_flag_t rml_cmd;
    orte_job_t *jdata;
    orte_process_name_t proc, proc2;
    orte_process_name_t *return_addr;
    int32_t i, num_replies;
    bool hnp_accounted_for;
    opal_pointer_array_t procarray;
    orte_proc_t *proct;
    char *cmd_str = NULL;
    opal_pointer_array_t *procs_to_kill = NULL;
    orte_std_cntr_t num_procs, num_new_procs = 0, p;
    orte_proc_t *cur_proc = NULL, *prev_proc = NULL;
    bool found = false;

    /* unpack the command */
    n = 1;
    if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &command, &n, ORTE_DAEMON_CMD))) {
        ORTE_ERROR_LOG(ret);
        return;
    }

    cmd_str = get_orcmsd_comm_cmd_str(command);
    OPAL_OUTPUT_VERBOSE((1, orte_debug_output,
                         "%s orted:comm:process_commands() Processing Command: %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), cmd_str));
    free(cmd_str);
    cmd_str = NULL;

    /* now process the command locally */
    switch(command) {

        /****    NULL    ****/
    case ORTE_DAEMON_NULL_CMD:
        ret = ORTE_SUCCESS;
        break;
            
        /****    KILL_LOCAL_PROCS   ****/
    case ORTE_DAEMON_KILL_LOCAL_PROCS:
        num_replies = 0;

        /* construct the pointer array */
        OBJ_CONSTRUCT(&procarray, opal_pointer_array_t);
        opal_pointer_array_init(&procarray, num_replies, ORTE_GLOBAL_ARRAY_MAX_SIZE, 16);
        
        /* unpack the proc names into the array */
        while (ORTE_SUCCESS == (ret = opal_dss.unpack(buffer, &proc, &n, ORTE_NAME))) {
            proct = OBJ_NEW(orte_proc_t);
            proct->name.jobid = proc.jobid;
            proct->name.vpid = proc.vpid;

            opal_pointer_array_add(&procarray, proct);
            num_replies++;
        }
        if (ORTE_ERR_UNPACK_READ_PAST_END_OF_BUFFER != ret) {
            ORTE_ERROR_LOG(ret);
            goto KILL_PROC_CLEANUP;
        }
            
        if (0 == num_replies) {
            /* kill everything */
            if (ORTE_SUCCESS != (ret = orte_odls.kill_local_procs(NULL))) {
                ORTE_ERROR_LOG(ret);
            }
            break;
        } else {
            /* kill the procs */
            if (ORTE_SUCCESS != (ret = orte_odls.kill_local_procs(&procarray))) {
                ORTE_ERROR_LOG(ret);
            }
        }

        /* cleanup */
    KILL_PROC_CLEANUP:
        for (i=0; i < procarray.size; i++) {
            if (NULL != (proct = (orte_proc_t*)opal_pointer_array_get_item(&procarray, i))) {
                free(proct);
            }
        }
        OBJ_DESTRUCT(&procarray);
        break;
            
        /****    SIGNAL_LOCAL_PROCS   ****/
    case ORTE_DAEMON_SIGNAL_LOCAL_PROCS:
        /* unpack the jobid */
        n = 1;
        if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &job, &n, ORTE_JOBID))) {
            ORTE_ERROR_LOG(ret);
            goto CLEANUP;
        }
                
        /* look up job data object */
        jdata = orte_get_job_data_object(job);

        /* get the signal */
        n = 1;
        if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &signal, &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            goto CLEANUP;
        }

        /* Convert SIGTSTP to SIGSTOP so we can suspend a.out */
        if (SIGTSTP == signal) {
            if (orte_debug_daemons_flag) {
                opal_output(0, "%s orted_cmd: converted SIGTSTP to SIGSTOP before delivering",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            }
            signal = SIGSTOP;
            if (NULL != jdata) {
                jdata->state |= ORTE_JOB_STATE_SUSPENDED;
            }
        } else if (SIGCONT == signal && NULL != jdata) {
            jdata->state &= ~ORTE_JOB_STATE_SUSPENDED;
        }

        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_cmd: received signal_local_procs, delivering signal %d",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        signal);
        }

        /* signal them */
        if (ORTE_SUCCESS != (ret = orte_odls.signal_local_procs(NULL, signal))) {
            ORTE_ERROR_LOG(ret);
        }
        break;

        /****    ADD_LOCAL_PROCS   ****/
    case ORTE_DAEMON_ADD_LOCAL_PROCS:
        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_cmd: received add_local_procs",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        }
        /* launch the processes */
        if (ORTE_SUCCESS != (ret = orte_odls.launch_local_procs(buffer))) {
            OPAL_OUTPUT_VERBOSE((1, orte_debug_output,
                                 "%s orted:comm:add_procs failed to launch on error %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ORTE_ERROR_NAME(ret)));
        }
        break;
           
    case ORTE_DAEMON_ABORT_PROCS_CALLED:
        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_cmd: received abort_procs report",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        }

        /* Number of processes */
        n = 1;
        if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &num_procs, &n, ORTE_STD_CNTR)) ) {
            ORTE_ERROR_LOG(ret);
            goto CLEANUP;
        }

        /* Retrieve list of processes */
        procs_to_kill = OBJ_NEW(opal_pointer_array_t);
        opal_pointer_array_init(procs_to_kill, num_procs, INT32_MAX, 2);

        /* Keep track of previously terminated, so we don't keep ordering the
         * same processes to die.
         */
        if( NULL == procs_prev_ordered_to_terminate ) {
            procs_prev_ordered_to_terminate = OBJ_NEW(opal_pointer_array_t);
            opal_pointer_array_init(procs_prev_ordered_to_terminate, num_procs+1, INT32_MAX, 8);
        }

        num_new_procs = 0;
        for( i = 0; i < num_procs; ++i) {
            cur_proc = OBJ_NEW(orte_proc_t);

            n = 1;
            if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &(cur_proc->name), &n, ORTE_NAME)) ) {
                ORTE_ERROR_LOG(ret);
                goto CLEANUP;
            }

            /* See if duplicate */
            found = false;
            for( p = 0; p < procs_prev_ordered_to_terminate->size; ++p) {
                if( NULL == (prev_proc = (orte_proc_t*)opal_pointer_array_get_item(procs_prev_ordered_to_terminate, p))) {
                    continue;
                }
                if(OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL,
                                                               &cur_proc->name,
                                                               &prev_proc->name) ) {
                    found = true;
                    break;
                }
            }

            OPAL_OUTPUT_VERBOSE((2, orte_debug_output,
                                 "%s orted:comm:abort_procs Application %s requests term. of %s (%2d of %2d) %3s.",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 ORTE_NAME_PRINT(sender),
                                 ORTE_NAME_PRINT(&(cur_proc->name)), i, num_procs,
                                 (found ? "Dup" : "New") ));

            /* If not a duplicate, then add to the to_kill list */
            if( !found ) {
                opal_pointer_array_add(procs_to_kill, (void*)cur_proc);
                OBJ_RETAIN(cur_proc);
                opal_pointer_array_add(procs_prev_ordered_to_terminate, (void*)cur_proc);
                num_new_procs++;
            }
        }

        /*
         * Send the request to termiante
         */
        if( num_new_procs > 0 ) {
            OPAL_OUTPUT_VERBOSE((2, orte_debug_output,
                                 "%s orted:comm:abort_procs Terminating application requested processes (%2d / %2d).",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 num_new_procs, num_procs));
            orte_plm.terminate_procs(procs_to_kill);
        } else {
            OPAL_OUTPUT_VERBOSE((2, orte_debug_output,
                                 "%s orted:comm:abort_procs No new application processes to terminating from request (%2d / %2d).",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 num_new_procs, num_procs));
        }

        break;

        /****    DELIVER A MESSAGE TO THE LOCAL PROCS    ****/
    case ORTE_DAEMON_MESSAGE_LOCAL_PROCS:
        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_cmd: received message_local_procs",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        }
                        
        /* unpack the jobid of the procs that are to receive the message */
        n = 1;
        if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &job, &n, ORTE_JOBID))) {
            ORTE_ERROR_LOG(ret);
            goto CLEANUP;
        }
                
        /* unpack the tag where we are to deliver the message */
        n = 1;
        if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &target_tag, &n, ORTE_RML_TAG))) {
            ORTE_ERROR_LOG(ret);
            goto CLEANUP;
        }
                
        OPAL_OUTPUT_VERBOSE((1, orte_debug_output,
                             "%s orted:comm:message_local_procs delivering message to job %s tag %d",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_JOBID_PRINT(job), (int)target_tag));

        relay_msg = OBJ_NEW(opal_buffer_t);
        opal_dss.copy_payload(relay_msg, buffer);
            
        /* if job=my_jobid, then this message is for us and not for our children */
        if (ORTE_PROC_MY_NAME->jobid == job) {
            /* if the target tag is our xcast_barrier or rml_update, then we have
             * to handle the message as a special case. The RML has logic in it
             * intended to make it easier to use. This special logic mandates that
             * any message we "send" actually only goes into the queue for later
             * transmission. Thus, since we are already in a recv when we enter
             * the "process_commands" function, any attempt to "send" the relay
             * buffer to ourselves will only be added to the queue - it won't
             * actually be delivered until *after* we conclude the processing
             * of the current recv.
             *
             * The problem here is that, for messages where we need to relay
             * them along the orted chain, the rml_update
             * message contains contact info we may well need in order to do
             * the relay! So we need to process those messages immediately.
             * The only way to accomplish that is to (a) detect that the
             * buffer is intended for those tags, and then (b) process
             * those buffers here.
             *
             */
            if (ORTE_RML_TAG_RML_INFO_UPDATE == target_tag) {
                n = 1;
                if (ORTE_SUCCESS != (ret = opal_dss.unpack(relay_msg, &rml_cmd, &n, ORTE_RML_CMD))) {
                    ORTE_ERROR_LOG(ret);
                    goto CLEANUP;
                }
                /* initialize the routes to my peers - this will update the number
                 * of daemons in the system (i.e., orte_process_info.num_procs) as
                 * this might have changed
                 */
                if (ORTE_SUCCESS != (ret = orte_routed.init_routes(ORTE_PROC_MY_NAME->jobid, relay_msg))) {
                    ORTE_ERROR_LOG(ret);
                    goto CLEANUP;
                }
            } else {
                /* just deliver it to ourselves */
                if ((ret = orte_rml.send_buffer_nb(ORTE_PROC_MY_NAME, relay_msg, target_tag,
                                                   orte_rml_send_callback, NULL)) < 0) {
                    ORTE_ERROR_LOG(ret);
                    OBJ_RELEASE(relay_msg);
                }
            }
        } else {
            /* must be for our children - deliver the message */
            if (ORTE_SUCCESS != (ret = orte_odls.deliver_message(job, relay_msg, target_tag))) {
                ORTE_ERROR_LOG(ret);
            }
            OBJ_RELEASE(relay_msg);
        }
        break;
    
        /****    EXIT COMMAND    ****/
    case ORTE_DAEMON_EXIT_CMD:
        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_cmd: received exit cmd",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        }
        /* kill the local procs */
        orte_odls.kill_local_procs(NULL);
        /* flag that orteds were ordered to terminate */
        orte_orteds_term_ordered = true;
        /* if all my routes and local children are gone, then terminate ourselves */
        if (0 == orte_routed.num_routes()) {
            for (i=0; i < orte_local_children->size; i++) {
                if (NULL != (proct = (orte_proc_t*)opal_pointer_array_get_item(orte_local_children, i)) &&
                    ORTE_FLAG_TEST(proct, ORTE_PROC_FLAG_ALIVE)) {
                    /* at least one is still alive */
                    return;
                }
            }
            /* call our appropriate exit procedure */
            if (orte_debug_daemons_flag) {
                opal_output(0, "%s orted_cmd: all routes and children gone - exiting",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            }
            ORTE_ACTIVATE_JOB_STATE(NULL, ORTE_JOB_STATE_DAEMONS_TERMINATED);
        }
        return;
        break;
            
    case ORTE_DAEMON_HALT_VM_CMD:
        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_cmd: received halt_vm cmd",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        }
        /* kill the local procs */
        orte_odls.kill_local_procs(NULL);
        /* flag that orteds were ordered to terminate */
        orte_orteds_term_ordered = true;
        if (ORTE_PROC_IS_HNP) {
            /* if all my routes and local children are gone, then terminate ourselves */
            if (0 == orte_routed.num_routes()) {
                for (i=0; i < orte_local_children->size; i++) {
                    if (NULL != (proct = (orte_proc_t*)opal_pointer_array_get_item(orte_local_children, i)) &&
                        ORTE_FLAG_TEST(proct, ORTE_PROC_FLAG_ALIVE)) {
                        /* at least one is still alive */
                        return;
                    }
                }
                /* call our appropriate exit procedure */
                if (orte_debug_daemons_flag) {
                    opal_output(0, "%s orted_cmd: all routes and children gone - exiting",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
                }
                ORTE_ACTIVATE_JOB_STATE(NULL, ORTE_JOB_STATE_DAEMONS_TERMINATED);
            }
        } else {
            ORTE_ACTIVATE_JOB_STATE(NULL, ORTE_JOB_STATE_DAEMONS_TERMINATED);
        }
        return;
        break;

        /****    SPAWN JOB COMMAND    ****/
    case ORTE_DAEMON_SPAWN_JOB_CMD:
        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_cmd: received spawn job",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        }
        answer = OBJ_NEW(opal_buffer_t);
        job = ORTE_JOBID_INVALID;
        /* can only process this if we are the HNP */
        if (ORTE_PROC_IS_HNP) {
            /* unpack the job data */
            n = 1;
            if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &jdata, &n, ORTE_JOB))) {
                ORTE_ERROR_LOG(ret);
                goto ANSWER_LAUNCH;
            }
                    
            /* launch it */
            if (ORTE_SUCCESS != (ret = orte_plm.spawn(jdata))) {
                ORTE_ERROR_LOG(ret);
                goto ANSWER_LAUNCH;
            }
            job = jdata->jobid;
        }
    ANSWER_LAUNCH:
        /* pack the jobid to be returned */
        if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &job, 1, ORTE_JOBID))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(answer);
            goto CLEANUP;
        }
        /* return response */
        if (0 > (ret = orte_rml.send_buffer_nb(sender, answer, ORTE_RML_TAG_TOOL,
                                               orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(answer);
        }
        break;
            
        /****     CONTACT QUERY COMMAND    ****/
    case ORTE_DAEMON_CONTACT_QUERY_CMD:
        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_cmd: received contact query",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        }
        /* send back contact info */
        contact_info = orte_rml.get_contact_info();
            
        if (NULL == contact_info) {
            ORTE_ERROR_LOG(ORTE_ERROR);
            ret = ORTE_ERROR;
            goto CLEANUP;
        }
            
        /* setup buffer with answer */
        answer = OBJ_NEW(opal_buffer_t);
        if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &contact_info, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(answer);
            goto CLEANUP;
        }
            
        if (0 > (ret = orte_rml.send_buffer_nb(sender, answer, ORTE_RML_TAG_TOOL,
                                               orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(answer);
        }
        break;
            
        /****     REPORT_JOB_INFO_CMD COMMAND    ****/
    case ORTE_DAEMON_REPORT_JOB_INFO_CMD:
        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_cmd: received job info query",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        }
        /* if we are not the HNP, we can do nothing - report
         * back 0 procs so the tool won't hang
         */
        if (!ORTE_PROC_IS_HNP) {
            int32_t zero=0;
                
            answer = OBJ_NEW(opal_buffer_t);
            if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &zero, 1, OPAL_INT32))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(answer);
                goto CLEANUP;
            }
            if (0 > (ret = orte_rml.send_buffer_nb(sender, answer, ORTE_RML_TAG_TOOL,
                                                   orte_rml_send_callback, NULL))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(answer);
            }
        } else {
            /* if we are the HNP, process the request */
            int32_t i, num_jobs;
            orte_job_t *jobdat;
                
            /* unpack the jobid */
            n = 1;
            if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &job, &n, ORTE_JOBID))) {
                ORTE_ERROR_LOG(ret);
                goto CLEANUP;
            }
                
            /* setup return */
            answer = OBJ_NEW(opal_buffer_t);
                
            /* if they asked for a specific job, then just get that info */
            if (ORTE_JOBID_WILDCARD != job) {
                job = ORTE_CONSTRUCT_LOCAL_JOBID(ORTE_PROC_MY_NAME->jobid, job);
                if (NULL != (jobdat = orte_get_job_data_object(job))) {
                    num_jobs = 1;
                    if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &num_jobs, 1, OPAL_INT32))) {
                        ORTE_ERROR_LOG(ret);
                        OBJ_RELEASE(answer);
                        goto CLEANUP;
                    }
                    if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &jobdat, 1, ORTE_JOB))) {
                        ORTE_ERROR_LOG(ret);
                        OBJ_RELEASE(answer);
                        goto CLEANUP;
                    }
                } else {
                    /* if we get here, then send a zero answer */
                    num_jobs = 0;
                    if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &num_jobs, 1, OPAL_INT32))) {
                        ORTE_ERROR_LOG(ret);
                        OBJ_RELEASE(answer);
                        goto CLEANUP;
                    }
                }
            } else {
                /* since the job array is no longer
                 * left-justified and may have holes, we have
                 * to cnt the number of jobs. Be sure to include the daemon
                 * job - the user can slice that info out if they don't care
                 */
                num_jobs = 0;
                for (i=0; i < orte_job_data->size; i++) {
                    if (NULL != opal_pointer_array_get_item(orte_job_data, i)) {
                        num_jobs++;
                    }
                }
                /* pack the number of jobs */
                if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &num_jobs, 1, OPAL_INT32))) {
                    ORTE_ERROR_LOG(ret);
                    OBJ_RELEASE(answer);
                    goto CLEANUP;
                }
                /* now pack the data, one at a time */
                for (i=0; i < orte_job_data->size; i++) {
                    if (NULL != (jobdat = (orte_job_t*)opal_pointer_array_get_item(orte_job_data, i))) {
                        if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &jobdat, 1, ORTE_JOB))) {
                            ORTE_ERROR_LOG(ret);
                            OBJ_RELEASE(answer);
                            goto CLEANUP;
                        }
                    }
                }
            }
            if (0 > (ret = orte_rml.send_buffer_nb(sender, answer, ORTE_RML_TAG_TOOL,
                                                   orte_rml_send_callback, NULL))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(answer);
            }
        }
        break;
            
        /****     REPORT_NODE_INFO_CMD COMMAND    ****/
    case ORTE_DAEMON_REPORT_NODE_INFO_CMD:
        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_cmd: received node info query",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        }
        /* if we are not the HNP, we can do nothing - report
         * back 0 nodes so the tool won't hang
         */
        if (!ORTE_PROC_IS_HNP) {
            int32_t zero=0;
                
            answer = OBJ_NEW(opal_buffer_t);
            if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &zero, 1, OPAL_INT32))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(answer);
                goto CLEANUP;
            }
            if (0 > (ret = orte_rml.send_buffer_nb(sender, answer, ORTE_RML_TAG_TOOL,
                                                   orte_rml_send_callback, NULL))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(answer);
            }
        } else {
            /* if we are the HNP, process the request */
            int32_t i, num_nodes;
            orte_node_t *node;
            char *nid;
                
            /* unpack the nodename */
            n = 1;
            if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &nid, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                goto CLEANUP;
            }
                
            /* setup return */
            answer = OBJ_NEW(opal_buffer_t);
            num_nodes = 0;
                
            /* if they asked for a specific node, then just get that info */
            if (NULL != nid) {
                /* find this node */
                for (i=0; i < orte_node_pool->size; i++) {
                    if (NULL == (node = (orte_node_t*)opal_pointer_array_get_item(orte_node_pool, i))) {
                        continue;
                    }
                    if (0 == strcmp(nid, node->name)) {
                        num_nodes = 1;
                        break;
                    }
                }
                if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &num_nodes, 1, OPAL_INT32))) {
                    ORTE_ERROR_LOG(ret);
                    OBJ_RELEASE(answer);
                    goto CLEANUP;
                }
                if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &node, 1, ORTE_NODE))) {
                    ORTE_ERROR_LOG(ret);
                    OBJ_RELEASE(answer);
                    goto CLEANUP;
                }
            } else {
                /* count number of nodes */
                for (i=0; i < orte_node_pool->size; i++) {
                    if (NULL != opal_pointer_array_get_item(orte_node_pool, i)) {
                        num_nodes++;
                    }
                }
                /* pack the answer */
                if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &num_nodes, 1, OPAL_INT32))) {
                    ORTE_ERROR_LOG(ret);
                    OBJ_RELEASE(answer);
                    goto CLEANUP;
                }
                /* pack each node separately */
                for (i=0; i < orte_node_pool->size; i++) {
                    if (NULL != (node = (orte_node_t*)opal_pointer_array_get_item(orte_node_pool, i))) {
                        if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &node, 1, ORTE_NODE))) {
                            ORTE_ERROR_LOG(ret);
                            OBJ_RELEASE(answer);
                            goto CLEANUP;
                        }
                    }
                }
            }
            /* send the info */
            if (0 > (ret = orte_rml.send_buffer_nb(sender, answer, ORTE_RML_TAG_TOOL,
                                                   orte_rml_send_callback, NULL))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(answer);
            }
        }
        break;
            
        /****     REPORT_PROC_INFO_CMD COMMAND    ****/
    case ORTE_DAEMON_REPORT_PROC_INFO_CMD:
        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_cmd: received proc info query",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        }
        /* if we are not the HNP, we can do nothing - report
         * back 0 procs so the tool won't hang
         */
        if (!ORTE_PROC_IS_HNP) {
            int32_t zero=0;
                
            answer = OBJ_NEW(opal_buffer_t);
            if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &zero, 1, OPAL_INT32))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(answer);
                goto CLEANUP;
            }
            if (0 > (ret = orte_rml.send_buffer_nb(sender, answer, ORTE_RML_TAG_TOOL,
                                                   orte_rml_send_callback, NULL))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(answer);
            }
        } else {
            /* if we are the HNP, process the request */
            orte_job_t *jdata;
            orte_proc_t *proc;
            orte_vpid_t vpid;
            int32_t i, num_procs;
            char *nid;

            /* setup the answer */
            answer = OBJ_NEW(opal_buffer_t);
                
            /* unpack the jobid */
            n = 1;
            if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &job, &n, ORTE_JOBID))) {
                ORTE_ERROR_LOG(ret);
                goto CLEANUP;
            }
                
            /* look up job data object */
            job = ORTE_CONSTRUCT_LOCAL_JOBID(ORTE_PROC_MY_NAME->jobid, job);
            if (NULL == (jdata = orte_get_job_data_object(job))) {
                ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
                goto CLEANUP;
            }
                
            /* unpack the vpid */
            n = 1;
            if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &vpid, &n, ORTE_VPID))) {
                ORTE_ERROR_LOG(ret);
                goto CLEANUP;
            }


            /* if they asked for a specific proc, then just get that info */
            if (ORTE_VPID_WILDCARD != vpid) {
                /* find this proc */
                for (i=0; i < jdata->procs->size; i++) {
                    if (NULL == (proc = (orte_proc_t*)opal_pointer_array_get_item(jdata->procs, i))) {
                        continue;
                    }
                    if (vpid == proc->name.vpid) {
                        num_procs = 1;
                        if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &num_procs, 1, OPAL_INT32))) {
                            ORTE_ERROR_LOG(ret);
                            goto CLEANUP;
                        }
                        if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &proc, 1, ORTE_PROC))) {
                            ORTE_ERROR_LOG(ret);
                            goto CLEANUP;
                        }
                        /* the vpid and nodename for this proc are no longer packed
                         * in the ORTE_PROC packing routines to save space for other
                         * uses, so we have to pack them separately
                         */
                        if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &proc->pid, 1, OPAL_PID))) {
                            ORTE_ERROR_LOG(ret);
                            goto CLEANUP;
                        }
                        if (NULL == proc->node) {
                            nid = "UNKNOWN";
                        } else {
                            nid = proc->node->name;
                        }
                        if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &nid, 1, OPAL_STRING))) {
                            ORTE_ERROR_LOG(ret);
                            goto CLEANUP;
                        }
                        break;
                    }                    
                }
            } else {
                /* count number of procs */
                num_procs = 0;
                for (i=0; i < jdata->procs->size; i++) {
                    if (NULL != opal_pointer_array_get_item(jdata->procs, i)) {
                        num_procs++;
                    }
                }
                /* pack the answer */
                if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &num_procs, 1, OPAL_INT32))) {
                    ORTE_ERROR_LOG(ret);
                    OBJ_RELEASE(answer);
                    goto CLEANUP;
                }
                /* pack each proc separately */
                for (i=0; i < jdata->procs->size; i++) {
                    if (NULL != (proc = (orte_proc_t*)opal_pointer_array_get_item(jdata->procs, i))) {
                        if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &proc, 1, ORTE_PROC))) {
                            ORTE_ERROR_LOG(ret);
                            OBJ_RELEASE(answer);
                            goto CLEANUP;
                        }
                        /* the vpid and nodename for this proc are no longer packed
                         * in the ORTE_PROC packing routines to save space for other
                         * uses, so we have to pack them separately
                         */
                        if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &proc->pid, 1, OPAL_PID))) {
                            ORTE_ERROR_LOG(ret);
                            goto CLEANUP;
                        }
                        if (NULL == proc->node) {
                            nid = "UNKNOWN";
                        } else {
                            nid = proc->node->name;
                        }
                        if (ORTE_SUCCESS != (ret = opal_dss.pack(answer, &nid, 1, OPAL_STRING))) {
                            ORTE_ERROR_LOG(ret);
                            goto CLEANUP;
                        }
                    }
                }
            }
            /* send the info */
            if (0 > (ret = orte_rml.send_buffer_nb(sender, answer, ORTE_RML_TAG_TOOL,
                                                   orte_rml_send_callback, NULL))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(answer);
            }
        }
        break;
            
        /****     HEARTBEAT COMMAND    ****/
    case ORTE_DAEMON_HEARTBEAT_CMD:
        ORTE_ERROR_LOG(ORTE_ERR_NOT_IMPLEMENTED);
        ret = ORTE_ERR_NOT_IMPLEMENTED;
        break;
            
        /****    SYNC FROM LOCAL PROC    ****/
    case ORTE_DAEMON_SYNC_BY_PROC:
        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_recv: received sync from local proc %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        ORTE_NAME_PRINT(sender));
        }
        if (ORTE_SUCCESS != (ret = orte_odls.require_sync(sender, buffer, false))) {
            ORTE_ERROR_LOG(ret);
            goto CLEANUP;
        }
        break;
            
    case ORTE_DAEMON_SYNC_WANT_NIDMAP:
        if (orte_debug_daemons_flag) {
            opal_output(0, "%s orted_recv: received sync+nidmap from local proc %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        ORTE_NAME_PRINT(sender));
        }
        if (ORTE_SUCCESS != (ret = orte_odls.require_sync(sender, buffer, true))) {
            ORTE_ERROR_LOG(ret);
            goto CLEANUP;
        }
        break;
        
        /****     TOP COMMAND     ****/
    case ORTE_DAEMON_TOP_CMD:
        /* setup the answer */
        answer = OBJ_NEW(opal_buffer_t);
        num_replies = 0;
        hnp_accounted_for = false;
            
        n = 1;
        return_addr = NULL;
        while (ORTE_SUCCESS == opal_dss.unpack(buffer, &proc, &n, ORTE_NAME)) {
            /* the jobid provided will, of course, have the job family of
             * the requestor. We need to convert that to our own job family
             */
            proc.jobid = ORTE_CONSTRUCT_LOCAL_JOBID(ORTE_PROC_MY_NAME->jobid, proc.jobid);
            if (ORTE_PROC_IS_HNP) {
                return_addr = sender;
                proc2.jobid = ORTE_PROC_MY_NAME->jobid;
                /* if the request is for a wildcard vpid, then it goes to every
                 * daemon. For scalability, we should probably xcast this some
                 * day - but for now, we just loop
                 */
                if (ORTE_VPID_WILDCARD == proc.vpid) {
                    /* loop across all daemons */
                    for (proc2.vpid=1; proc2.vpid < orte_process_info.num_procs; proc2.vpid++) {

                        /* setup the cmd */
                        relay_msg = OBJ_NEW(opal_buffer_t);
                        command = ORTE_DAEMON_TOP_CMD;
                        if (ORTE_SUCCESS != (ret = opal_dss.pack(relay_msg, &command, 1, ORTE_DAEMON_CMD))) {
                            ORTE_ERROR_LOG(ret);
                            OBJ_RELEASE(relay_msg);
                            goto SEND_TOP_ANSWER;
                        }
                        if (ORTE_SUCCESS != (ret = opal_dss.pack(relay_msg, &proc, 1, ORTE_NAME))) {
                            ORTE_ERROR_LOG(ret);
                            OBJ_RELEASE(relay_msg);
                            goto SEND_TOP_ANSWER;
                        }
                        if (ORTE_SUCCESS != (ret = opal_dss.pack(relay_msg, sender, 1, ORTE_NAME))) {
                            ORTE_ERROR_LOG(ret);
                            OBJ_RELEASE(relay_msg);
                            goto SEND_TOP_ANSWER;
                        }
                        /* the callback function will release relay_msg buffer */
                        if (0 > orte_rml.send_buffer_nb(&proc2, relay_msg,
                                                        ORTE_RML_TAG_DAEMON,
                                                        orte_rml_send_callback, NULL)) {
                            ORTE_ERROR_LOG(ORTE_ERR_COMM_FAILURE);
                            OBJ_RELEASE(relay_msg);
                            ret = ORTE_ERR_COMM_FAILURE;
                        }
                        num_replies++;
                    }
                    /* account for our own reply */
                    if (!hnp_accounted_for) {
                        hnp_accounted_for = true;
                        num_replies++;
                    }
                    /* now get the data for my own procs */
                    goto GET_TOP;
                } else {
                    /* this is for a single proc - see which daemon
                     * this rank is on
                     */
                    if (ORTE_VPID_INVALID == (proc2.vpid = orte_get_proc_daemon_vpid(&proc))) {
                        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
                        goto SEND_TOP_ANSWER;
                    }
                    /* if the vpid is me, then just handle this myself */
                    if (proc2.vpid == ORTE_PROC_MY_NAME->vpid) {
                        if (!hnp_accounted_for) {
                            hnp_accounted_for = true;
                            num_replies++;
                        }
                        goto GET_TOP;
                    }
                    /* otherwise, forward the cmd on to the appropriate daemon */
                    relay_msg = OBJ_NEW(opal_buffer_t);
                    command = ORTE_DAEMON_TOP_CMD;
                    if (ORTE_SUCCESS != (ret = opal_dss.pack(relay_msg, &command, 1, ORTE_DAEMON_CMD))) {
                        ORTE_ERROR_LOG(ret);
                        OBJ_RELEASE(relay_msg);
                        goto SEND_TOP_ANSWER;
                    }
                    if (ORTE_SUCCESS != (ret = opal_dss.pack(relay_msg, &proc, 1, ORTE_NAME))) {
                        ORTE_ERROR_LOG(ret);
                        OBJ_RELEASE(relay_msg);
                        goto SEND_TOP_ANSWER;
                    }
                    if (ORTE_SUCCESS != (ret = opal_dss.pack(relay_msg, sender, 1, ORTE_NAME))) {
                        ORTE_ERROR_LOG(ret);
                        OBJ_RELEASE(relay_msg);
                        goto SEND_TOP_ANSWER;
                    }
                    /* the callback function will release relay_msg buffer */
                    if (0 > orte_rml.send_buffer_nb(&proc2, relay_msg,
                                                    ORTE_RML_TAG_DAEMON,
                                                    orte_rml_send_callback, NULL)) {
                        ORTE_ERROR_LOG(ORTE_ERR_COMM_FAILURE);
                        OBJ_RELEASE(relay_msg);
                        ret = ORTE_ERR_COMM_FAILURE;
                    }
                }
                /* end if HNP */
            } else {
                /* this came from the HNP, but needs to go back to the original
                 * requestor. Unpack the name of that entity first
                 */
                n = 1;
                if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &proc2, &n, ORTE_NAME))) {
                    ORTE_ERROR_LOG(ret);
                    /* in this case, we are helpless - we have no idea who to send an
                     * error message TO! All we can do is return - the tool that sent
                     * this request is going to hang, but there isn't anything we can
                     * do about it
                     */
                    goto CLEANUP;
                }
                return_addr = &proc2;
            GET_TOP:
                /* this rank must be local to me, or the HNP wouldn't
                 * have sent it to me - process the request
                 */
                if (ORTE_SUCCESS != (ret = orte_odls_base_get_proc_stats(answer, &proc))) {
                    ORTE_ERROR_LOG(ret);
                    goto SEND_TOP_ANSWER;
                }
            }
        }
    SEND_TOP_ANSWER:
        /* send the answer back to requester */
        if (ORTE_PROC_IS_HNP) {
            /* if I am the HNP, I need to also provide the number of
             * replies the caller should recv and the sample time
             */
            time_t mytime;
            char *cptr;
                
            relay_msg = OBJ_NEW(opal_buffer_t);
            if (ORTE_SUCCESS != (ret = opal_dss.pack(relay_msg, &num_replies, 1, OPAL_INT32))) {
                ORTE_ERROR_LOG(ret);
            }
            time(&mytime);
            cptr = ctime(&mytime);
            cptr[strlen(cptr)-1] = '\0';  /* remove trailing newline */
            if (ORTE_SUCCESS != (ret = opal_dss.pack(relay_msg, &cptr, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
            }
            /* copy the stats payload */
            opal_dss.copy_payload(relay_msg, answer);
            OBJ_RELEASE(answer);
            answer = relay_msg;
        }
        /* if we don't have a return address, then we are helpless */
        if (NULL == return_addr) {
            ORTE_ERROR_LOG(ORTE_ERR_COMM_FAILURE);
            ret = ORTE_ERR_COMM_FAILURE;
            break;
        }
        if (0 > (ret = orte_rml.send_buffer_nb(return_addr, answer, ORTE_RML_TAG_TOOL,
                                               orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(answer);
        }
        break;
        
    default:
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
    }
    
 CLEANUP:
    return;
}

static char *get_orcmsd_comm_cmd_str(int command)
{
    switch(command) {
    case ORTE_DAEMON_NULL_CMD:
        return strdup("NULL");
    case ORTE_DAEMON_KILL_LOCAL_PROCS:
        return strdup("ORCMSD_DAEMON_KILL_LOCAL_PROCS");
    case ORTE_DAEMON_SIGNAL_LOCAL_PROCS:
        return strdup("ORCMSD_DAEMON_SIGNAL_LOCAL_PROCS");
    case ORTE_DAEMON_ADD_LOCAL_PROCS:
        return strdup("ORCMSD_DAEMON_ADD_LOCAL_PROCS");
    case ORTE_DAEMON_TREE_SPAWN:
        return strdup("ORCMSD_DAEMON_TREE_SPAWN");
    case ORTE_DAEMON_MESSAGE_LOCAL_PROCS:
        return strdup("ORCMSD_DAEMON_MESSAGE_LOCAL_PROCS");
     case ORTE_DAEMON_EXIT_CMD:
        return strdup("ORCMSD_DAEMON_EXIT_CMD");
    case ORTE_DAEMON_SPAWN_JOB_CMD:
        return strdup("ORCMSD_DAEMON_SPAWN_JOB_CMD");
    case ORTE_DAEMON_CONTACT_QUERY_CMD:
        return strdup("ORCMSD_DAEMON_CONTACT_QUERY_CMD");
    case ORTE_DAEMON_REPORT_JOB_INFO_CMD:
        return strdup("ORCMSD_DAEMON_REPORT_JOB_INFO_CMD");
    case ORTE_DAEMON_REPORT_NODE_INFO_CMD:
        return strdup("ORCMSD_DAEMON_REPORT_NODE_INFO_CMD");
    case ORTE_DAEMON_REPORT_PROC_INFO_CMD:
        return strdup("ORCMSD_DAEMON_REPORT_PROC_INFO_CMD");
    case ORTE_DAEMON_HEARTBEAT_CMD:
        return strdup("ORCMSD_DAEMON_HEARTBEAT_CMD");
    case ORTE_DAEMON_SYNC_BY_PROC:
        return strdup("ORCMSD_DAEMON_SYNC_BY_PROC");
    case ORTE_DAEMON_SYNC_WANT_NIDMAP:
        return strdup("ORCMSD_DAEMON_SYNC_WANT_NIDMAP");
    case ORTE_DAEMON_TOP_CMD:
        return strdup("ORCMSD_DAEMON_TOP_CMD");
    case ORTE_DAEMON_ABORT_PROCS_CALLED:
        return strdup("ORCMSD_DAEMON_ABORT_PROCS_CALLED");
    default:
        return strdup("Unknown Command!");
    }
}
