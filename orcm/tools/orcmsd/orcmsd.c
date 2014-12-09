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
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif


#include "opal/mca/event/event.h"
#include "opal/mca/base/base.h"
#include "opal/util/output.h"
#include "opal/util/cmd_line.h"
#include "opal/util/basename.h"
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
static bool orted_failed_launch;
static orte_job_t *jdatorted=NULL;

int orcms_daemon(int argc, char *argv[]);
static char *get_orcmsd_comm_cmd_str(int command);
void orcms_hnp_recv(int status, orte_process_name_t* sender,
                      opal_buffer_t *buffer, orte_rml_tag_t tag,
                      void* cbdata);
void orcms_daemon_recv(int status, orte_process_name_t* sender,
                      opal_buffer_t *buffer, orte_rml_tag_t tag,
                      void* cbdata);
static void orcmsd_batch_launch(char *batchfile,  char **environ);
static void orcmsd_wpid_event_recv(int fd, short args, void* cbdata);


static struct {
    bool debug;
    bool debug_daemons;
    bool help;
    bool hnp;
    bool abort;
    bool mapreduce;
    bool tree_spawn;
    bool report_uri;
    bool persistent;
    int fail;
    int fail_delay;
    char* name;
    char* num_procs;
    char* parent_uri;
    char* batchfile;
} orcmsd_globals;

/* timer event to check for batch pid */
typedef struct {
    opal_object_t object;
    opal_event_t ev;
    struct timeval rate;
    pid_t pid;
    void *cbdata;
} orcmsd_wpid_event_cb_t;
OBJ_CLASS_DECLARATION(orcmsd_wpid_event_cb_t);
/* Instance of class */
OBJ_CLASS_INSTANCE(orcmsd_wpid_event_cb_t, opal_object_t, NULL, NULL);

/*
 * define the orted context table for obtaining parameters
 */
opal_cmd_line_init_t orcmsd_cmd_line_opts[] = {
    /* Various "obvious" options */
    { "sst_orcmsd_jobid", '\0', "jobid", "jobid", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Regular expression defining nodes in system" },

    { "sst_orcmsd_vpid", '\0', "vpid", "vpid", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Regular expression defining nodes in system" },

    { "sst_orcmsd_node_regex", '\0', "node-regex", "node-regex", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Regular expression defining nodes in system" },

    { NULL, '\0', NULL, "hnp", 0,
      &orcmsd_globals.hnp, OPAL_CMD_LINE_TYPE_BOOL,
      "Direct the orted to act as the HNP"},

    { "orte_hnp_uri", '\0', NULL, "hnp-uri", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "URI for the HNP"},

    { NULL, '\0', NULL, "persistent", 0,
      &orcmsd_globals.persistent, OPAL_CMD_LINE_TYPE_BOOL,
      "persistent launch is enabled"},

    { "orcm_batch_file", '\0', NULL, "batchfile", 1,
      &orcmsd_globals.batchfile, OPAL_CMD_LINE_TYPE_STRING,
      "batch script to launch"},
    
    { NULL, '\0', "parent-uri", "parent-uri", 1,
      &orcmsd_globals.parent_uri, OPAL_CMD_LINE_TYPE_STRING,
      "URI for the parent"},

    { "orte_output_filename", '\0', "output-filename", "output-filename", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Redirect output from application processes into filename.rank" },
    
    { NULL, 'h', NULL, "help", 0,
      &orcmsd_globals.help, OPAL_CMD_LINE_TYPE_BOOL,
      "This help message" },
    
    { "orte_debug", 'd', NULL, "debug", 0,
      &orcmsd_globals.debug, OPAL_CMD_LINE_TYPE_BOOL,
      "Debug the OpenRTE" },

    { "orte_debug_daemons", 'd', NULL, "debug-daemons", 0,
      &orcmsd_globals.debug_daemons, OPAL_CMD_LINE_TYPE_BOOL,
      "Debug the OpenRTE" },

    /* select XML output */
    { "orte_xml_output", '\0', "xml", "xml", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Provide all output in XML format" },

    { "orte_xml_file", '\0', "xml-file", "xml-file", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Provide all output in XML format to the specified file" },
        
    { NULL, '\0', NULL, "report-uri", 0,
      &orcmsd_globals.report_uri, OPAL_CMD_LINE_TYPE_BOOL,
      "Print out the URI of this process" },

    /* End of list */
    { NULL, '\0', NULL, NULL, 0,
      NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }
};

int main(int argc, char *argv[])
{
    int ret = 0;
    opal_cmd_line_t *cmd_line = NULL;
    int i;
    opal_buffer_t *buffer;
    orte_job_t *jdata_obj;
    char *umask_str = getenv("ORTE_DAEMON_UMASK_VALUE");

    /* find our basename (the name of the executable) so that we can
     *        use it in pretty-print error messages */
    orte_basename = opal_basename(argv[0]);

    if (NULL != umask_str) {
        char *endptr;
        long mask = strtol(umask_str, &endptr, 8);
        if ((! (0 == mask && (EINVAL == errno || ERANGE == errno))) &&
            (*endptr == '\0')) {
            umask(mask);
        }
    }

    memset(&orcmsd_globals, 0, sizeof(orcmsd_globals));
    
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
    opal_unsetenv(OPAL_MCA_PREFIX"ess", &orte_launch_environ);
    
    /* if orte_daemon_debug is set, let someone know we are alive right
     * away just in case we have a problem along the way
     */
    
    /* check for help request */
    if (orcmsd_globals.help) {
        char *args = NULL;
        args = opal_cmd_line_get_usage_msg(cmd_line);
        orte_show_help("help-orcmsd.txt", "orcmsd:usage", false,
                       argv[0], args);
        free(args);
        return 1;
    }
#if defined(HAVE_SETSID)
    /* see if we were directed to separate from current session */
    setsid();
#endif


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

    if (orcmsd_globals.report_uri) {
        char *uri;
        uri = orte_rml.get_contact_info();
        fprintf(stderr, "URI=%s\n", uri);
        free(uri);
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
                if (NULL == (pu = opal_hwloc_base_get_pu(opal_hwloc_topology, core, OPAL_HWLOC_LOGICAL))) {
                    /* turn off the show help forwarding as we won't
                     * be able to cycle the event library to send
                     */
                    orte_show_help_finalize();
                    /* the message will now come out locally */
                    orte_show_help("help-orcmsd.txt", "orcmsd:cannot-bind",
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

        /* setup the orcms ctrl or hnp daemon command receive function */
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORCM_RML_TAG_HNP,
                            ORTE_RML_PERSISTENT, orcms_hnp_recv, NULL);
    
        if (orcmsd_globals.debug) {
            opal_output(0, "%s orcmsd hnp jobid: %d vpid: %d\n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ORTE_PROC_MY_NAME->jobid, 
                ORTE_PROC_MY_NAME->vpid);
            opal_output(0, "%s orcmsd hnp pid: %ld hnp-uri: %s host: %s\n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), 
                (long) orte_process_info.pid, orte_process_info.my_daemon_uri,
                orte_process_info.nodename);
        } 
    } else { 
        if (orcmsd_globals.debug) {
            opal_output(0, "%s orcmsd daemon jobid: %d vpid: %d\n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ORTE_PROC_MY_NAME->jobid, 
                ORTE_PROC_MY_NAME->vpid);
            opal_output(0, "%s orcmsd daemon pid: %ld hnp-uri: %s host: %s\n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), 
                (long) orte_process_info.pid, orte_process_info.my_hnp_uri,
                orte_process_info.nodename);
        } 
    } 
    /* setup the primary daemon command receive function */
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_DAEMON,
                            ORTE_RML_PERSISTENT, orcms_daemon_recv, NULL);
    
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


    /* get the daemon job, if necessary */
    jdata_obj=orte_get_job_data_object(0);
    if (NULL == jdata_obj) {
        goto DONE;
    }
    /* must create a map for it (even though it has no
     * info in it) so that the job info will be picked
     * up in subsequent pidmaps or other daemons won't
     * know how to route
     */
    jdata_obj->map = OBJ_NEW(orte_job_map_t);

    /* if we are not the HNP...the only time we will be an HNP
     * is if we are launched by a singleton to provide support
     * for it
     */
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

    /* send to the HNP's callback - will be routed if routes are available */
    if (0 > (ret = orte_rml.send_buffer_nb(ORTE_PROC_MY_HNP, buffer,
                                           ORCM_RML_TAG_HNP,
                                           orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(ret);
        OBJ_RELEASE(buffer);
        goto DONE;
    }

            
    opal_output(0, "%s orcmsd: up and running - waiting for commands!", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
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

    opal_output(0, "%s orcmsd: exiting with status %d\n", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), orte_exit_status);
    exit(orte_exit_status);
}


void orcms_hnp_recv(int status, orte_process_name_t* sender,
                      opal_buffer_t *buffer, orte_rml_tag_t tag,
                      void* cbdata)
{
    int rc, idx;
    orte_proc_t *daemon=NULL;
    char *nodename = NULL;
    orte_node_t *node;
    orte_job_t *jdata;
    orte_process_name_t dname;
    orcm_rm_cmd_flag_t command;
    opal_buffer_t *vmready_msg;
    char *contact_info;

    /* get the daemon job, if necessary */
    if (NULL == jdatorted) {
        jdatorted = orte_get_job_data_object(0);
        if (NULL == jdatorted) {
            ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
            return;
        }
    }

    /* multiple daemons could be in this buffer, so unpack until we exhaust the data */
    idx = 1;
    while (OPAL_SUCCESS == (rc = opal_dss.unpack(buffer, &dname, &idx, ORTE_NAME))) {
        /* update state and record for this daemon contact info */
        if (NULL == (daemon = (orte_proc_t*)opal_pointer_array_get_item(jdatorted->procs, dname.vpid))) {
            ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
            orted_failed_launch = true;
            goto CLEANUP;
        }

        daemon->state = ORTE_PROC_STATE_RUNNING;
        /* record that this daemon is alive */
        ORTE_FLAG_SET(daemon, ORTE_PROC_FLAG_ALIVE);

        node = daemon->node;
        if (NULL == node) {
            /* this shouldn't happen - it indicates an error in the
             * prior node matching logic, so report it and error out
             */
            orte_show_help("help-orcmsd.txt", "daemon-no-assigned-node", true,
                           ORTE_NAME_PRINT(&daemon->name), nodename);
            orted_failed_launch = true;
            goto CLEANUP;
        }
        
        /* mark the daemon as added */
        node->state = ORTE_NODE_STATE_ADDED;

    CLEANUP:
        OPAL_OUTPUT_VERBOSE((5, orcm_debug_output,
                             "%s plm:base:orted_report_launch %s for daemon %s at contact %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             orted_failed_launch ? "failed" : "completed",
                             ORTE_NAME_PRINT(&dname),
                             (NULL == daemon) ? "UNKNOWN" : daemon->rml_uri));
        
        jdatorted->num_reported++;
        if (orcmsd_globals.debug) {
            opal_output(0, "%s orcmsd: hnp_recv Number of daemons reported %d\n", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), jdatorted->num_reported);
        }
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

	    /* Execute Batch scripts */
	    if (NULL != orcmsd_globals.batchfile && true == orcmsd_globals.persistent) {
                
                orcmsd_batch_launch(orcmsd_globals.batchfile, orte_launch_environ);

            }
            /* send vm ready to the parent */
            if ( NULL != orcmsd_globals.parent_uri) {
                /* set the contact info into the hash table */
                orte_rml.set_contact_info(orcmsd_globals.parent_uri);
                rc = orte_rml_base_parse_uris(orcmsd_globals.parent_uri, ORTE_PROC_MY_PARENT, NULL);
                
                if (ORTE_SUCCESS != rc) {
                    ORTE_ERROR_LOG(rc);
                    return;
                }

                orte_routed.update_route(ORTE_PROC_MY_PARENT,ORTE_PROC_MY_PARENT);

                /* pack the VM_READY MESSAGE */
                command = ORCM_VM_READY_COMMAND;
                contact_info = orte_rml.get_contact_info();
                vmready_msg = OBJ_NEW(opal_buffer_t);
                if (OPAL_SUCCESS !=
                    (rc = opal_dss.pack(vmready_msg, &command, 1, ORCM_RM_CMD_T))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(vmready_msg);
                    return;
                }

                if (OPAL_SUCCESS !=
                    (rc = opal_dss.pack(vmready_msg, &contact_info, 1, OPAL_STRING))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(vmready_msg);
                    return;
                }

                if (orcmsd_globals.debug_daemons) {
                    opal_output(0, "hnp_recv: send VM Ready to the originator %s:%s\n", 
                                        ORTE_NAME_PRINT(ORTE_PROC_MY_PARENT), 
                                        orcmsd_globals.parent_uri);
                }
                if ((rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_PARENT, 
                    vmready_msg, ORCM_RML_TAG_VM_READY,
                    orte_rml_send_callback, NULL)) < 0) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(vmready_msg);
                    return;
                }
                if (orcmsd_globals.debug_daemons) {
                    opal_output(0, "hnp_recv: after sending VM Ready to the originator %s:%s\n", 
                                        ORTE_NAME_PRINT(ORTE_PROC_MY_PARENT), 
                                        orcmsd_globals.parent_uri);
                }
            }
        }
        idx = 1;
    }
    if (ORTE_ERR_UNPACK_READ_PAST_END_OF_BUFFER != rc) {
        ORTE_ERROR_LOG(rc);
        ORTE_ACTIVATE_JOB_STATE(jdatorted, ORTE_JOB_STATE_FAILED_TO_START);
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
    opal_buffer_t *answer;
    orte_rml_cmd_flag_t rml_cmd;
    orte_job_t *jdata;
    orte_process_name_t proc;
    int32_t i, num_replies;
    opal_pointer_array_t procarray;
    orte_proc_t *proct;
    char *cmd_str = NULL;

    /* unpack the command */
    n = 1;
    if (ORTE_SUCCESS != (ret = opal_dss.unpack(buffer, &command, &n, ORTE_DAEMON_CMD))) {
        ORTE_ERROR_LOG(ret);
        return;
    }

    cmd_str = get_orcmsd_comm_cmd_str(command);
    OPAL_OUTPUT_VERBOSE((1, orcm_debug_output,
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
            if (orcmsd_globals.debug_daemons) {
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

        if (orcmsd_globals.debug_daemons) {
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
        if (orcmsd_globals.debug_daemons) {
            opal_output(0, "%s orted_cmd: received add_local_procs",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        }
        /* launch the processes */
        if (ORTE_SUCCESS != (ret = orte_odls.launch_local_procs(buffer))) {
            OPAL_OUTPUT_VERBOSE((1, orcm_debug_output,
                                 "%s orted:comm:add_procs failed to launch on error %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ORTE_ERROR_NAME(ret)));
        }
        break;
           
        /****    DELIVER A MESSAGE TO THE LOCAL PROCS    ****/
    case ORTE_DAEMON_MESSAGE_LOCAL_PROCS:
        if (orcmsd_globals.debug_daemons) {
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
                
        OPAL_OUTPUT_VERBOSE((1, orcm_debug_output,
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
        if (orcmsd_globals.debug_daemons) {
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
            if (orcmsd_globals.debug_daemons) {
                opal_output(0, "%s orted_cmd: all routes and children gone - exiting",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            }
            ORTE_ACTIVATE_JOB_STATE(NULL, ORTE_JOB_STATE_DAEMONS_TERMINATED);
        }
        return;
        break;
            
    case ORTE_DAEMON_HALT_VM_CMD:
        if (orcmsd_globals.debug_daemons) {
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
                if (orcmsd_globals.debug_daemons) {
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
        if (orcmsd_globals.debug_daemons) {
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
            
    default:
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
    }

 CLEANUP:
    return;
}

static void set_handler_default(int sig)
{
    struct sigaction act;

    act.sa_handler = SIG_DFL;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    sigaction(sig, &act, (struct sigaction *)0);
}

static void orcmsd_wpid_event_recv(int fd, short args, void* cbdata)
{
    orcmsd_wpid_event_cb_t *req = (orcmsd_wpid_event_cb_t *) cbdata;
    int w = 0;
    int status = 0;
    if ( NULL == req || 0 >= req->pid) {
        opal_output(0, "batch timer event error req->pid is 0\n");
        return;
    }
    w = waitpid (req->pid, &status, WNOHANG|WUNTRACED|WCONTINUED);
    if (0 == w) {
        opal_output(0, "timer continued\n");
        req->rate.tv_sec++;
        opal_event_evtimer_add(&req->ev, &req->rate);
        /*
        kill_local(req->pid, SIGKILL);
        */
    } else { /*if (req->pid  == w) */ 
        opal_output(0, "batch job complete terminate stepd\n");
        /*ORTE_ACTIVATE_JOB_STATE(NULL, ORTE_JOB_STATE_DAEMONS_TERMINATED);
         */
        /* ensure all local procs are dead */
         orte_odls.kill_local_procs(NULL);
        /* cleanup and leave */
         orcm_finalize();
         opal_output(0, "%s orcmsd: exiting with status %d\n", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), orte_exit_status);
         exit(orte_exit_status);
    }
}

static void orcmsd_batch_launch(char *batchfile,  char **environ)
{
    char **argv = NULL;
    int argc=0;
    int ret;
    pid_t batch_pid;


    /*setup the HNP URI in the environment */
    opal_setenv("ORCM_MCA_HNP_URI", orte_process_info.my_hnp_uri, 
                true, &orte_launch_environ);

    batch_pid = fork();
    if (batch_pid < 0) {
        ORTE_ERROR_LOG(ORTE_ERR_SYS_LIMITS_CHILDREN);
        return;
    }

    if (batch_pid == 0) {
        sigset_t sigs;
        /* Set signal handlers back to the default.  Do this close
         to the execve() because the event library may (and likely
         will) reset them.  If we don't do this, the event
         library may have left some set that, at least on some
         OS's, don't get reset via fork() or exec().  Hence, the
         orted could be unkillable (for example). */
    
        set_handler_default(SIGTERM);
        set_handler_default(SIGINT);
        set_handler_default(SIGHUP);
        set_handler_default(SIGPIPE);
        set_handler_default(SIGCHLD);
    
    
        /* Unblock all signals, for many of the same reasons that
         we set the default handlers, above.  This is noticable
         on Linux where the event library blocks SIGTERM, but we
         don't want that blocked by the orted (or, more
         specifically, we don't want it to be blocked by the
         orted and then inherited by the ORTE processes that it
         forks, making them unkillable by SIGTERM). */
        sigprocmask(0, 0, &sigs);
        sigprocmask(SIG_UNBLOCK, &sigs, 0);
        opal_output(0, "%s BATCH JOB execution : %s \n",
               ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), batchfile);
        opal_argv_append(&argc, &argv, batchfile);
        /*
        opal_argv_join(argv, ' ');
        */
        ret = execve(batchfile, argv, orte_launch_environ);
        opal_output(0, "%s BATCH JOB execve - %d errno - %d  args  %s %s %s %s\n",
               ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ret, errno, batchfile, argv[1], orte_launch_environ[0], orte_launch_environ[1]);
        exit(-1);
    } else {
        /* I am the parent - report the batch pid back
         */
        orcmsd_wpid_event_cb_t * req;
        req = OBJ_NEW(orcmsd_wpid_event_cb_t);
        req->cbdata = (void *)jdatorted;
        req->rate.tv_sec=5;
        req->rate.tv_usec=0;
        req->pid = batch_pid;
        opal_event_evtimer_set(orte_event_base, &req->ev,
                              orcmsd_wpid_event_recv, req);
        opal_event_evtimer_add(&req->ev, &req->rate);

        opal_argv_free(argv);
        /* all done - report success */
        return;
    }
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
    case ORTE_DAEMON_MESSAGE_LOCAL_PROCS:
        return strdup("ORCMSD_DAEMON_MESSAGE_LOCAL_PROCS");
     case ORTE_DAEMON_EXIT_CMD:
        return strdup("ORCMSD_DAEMON_EXIT_CMD");
    case ORTE_DAEMON_SPAWN_JOB_CMD:
        return strdup("ORCMSD_DAEMON_SPAWN_JOB_CMD");
    case ORTE_DAEMON_ABORT_PROCS_CALLED:
        return strdup("ORCMSD_DAEMON_ABORT_PROCS_CALLED");
    default:
        return strdup("Unknown Command!");
    }
}
