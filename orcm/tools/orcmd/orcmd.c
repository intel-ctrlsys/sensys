/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdio.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <stdarg.h>
#include <pwd.h>
#include <unistd.h>

#include "opal/types.h"
#include "opal/dss/dss.h"
#include "opal/util/path.h"
#include "opal/util/argv.h"
#include "opal/util/cmd_line.h"
#include "opal/util/opal_environ.h"
#include "opal/util/show_help.h"
#include "opal/mca/base/mca_base_var.h"
#include "opal/mca/installdirs/installdirs.h"
#include "opal/mca/event/event.h"

#include "orte/util/show_help.h"
#include "orte/util/regex.h"
#include "orte/util/proc_info.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/scd/base/base.h"
#include "orcm/mca/scd/scd_types.h"
#include "orcm/mca/diag/diag.h"

#include "orcm/runtime/runtime.h"
#include "orcm/version.h"

static struct {
    bool help;
    bool version;
    bool verbose;
    bool master;
    int output;
} orcm_globals;

/* timer event to check for stepd proc id */
typedef struct {
    opal_object_t object;
    opal_event_t ev;
    struct timeval rate;
    pid_t pid;
    void *cbdata;
} orcmd_wpid_event_cb_t;
OBJ_CLASS_DECLARATION(orcmd_wpid_event_cb_t);
/* Instance of class */
OBJ_CLASS_INSTANCE(orcmd_wpid_event_cb_t, opal_object_t, NULL, NULL);

#define HNP_PORT_NUM 12345
static int stepd_pid = 0;

static opal_cmd_line_init_t cmd_line_init[] = {
    /* Various "obvious" options */
    { NULL, 'h', NULL, "help", 0,
      &orcm_globals.help, OPAL_CMD_LINE_TYPE_BOOL,
      "This help message" },

    { NULL, 'V', NULL, "version", 0,
      &orcm_globals.version, OPAL_CMD_LINE_TYPE_BOOL,
      "Print version and exit" },

    { NULL, 'v', NULL, "verbose", 0,
      &orcm_globals.verbose, OPAL_CMD_LINE_TYPE_BOOL,
      "Be verbose" },

    { "cfgi_base_config_file", 's', "site-file", "site-file", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Site configuration file for this orcm chain" },

    { "cfgi_base_validate", '\0', "validate-config", "validate-config", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Validate site file and exit" },

    /* End of list */
    { NULL, '\0', NULL, NULL, 0,
      NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }
};

static void orcmd_recv(int status, orte_process_name_t* sender,
                       opal_buffer_t* buffer, orte_rml_tag_t tag,
                       void* cbdata);
static int slm_fork_hnp_procs(orte_jobid_t jobid, int port_num, int hnp, 
                       char *hnp_uri, orcm_alloc_t *alloc, int *stepd_pid);
static int kill_local(pid_t pid, int signum);

int main(int argc, char *argv[])
{
    int ret;
    opal_cmd_line_t cmd_line;
    char *ctmp;
    char *args = NULL;
    char *str = NULL;
    time_t now;
    opal_buffer_t *buf;
    orcm_rm_cmd_flag_t command = ORCM_NODESTATE_UPDATE_COMMAND;
    orcm_node_state_t state = ORCM_NODE_STATE_UP;
    bool have_hwloc_topology;

    /* process the cmd line arguments to get any MCA params on them */
    opal_cmd_line_create(&cmd_line, cmd_line_init);
    if (ORCM_SUCCESS != (ret = opal_cmd_line_parse(&cmd_line, false, argc, argv))) {
        fprintf(stderr, "Command line error, use -h to get help.  Error: %d\n", ret);
        exit(1);
    }
    
    if( orcm_globals.verbose ) {
        orcm_globals.output = opal_output_open(NULL);
        opal_output_set_verbosity(orcm_globals.output, 10);
    } else {
        orcm_globals.output = 0; /* Default=STDERR */
    }



    /*
     * Since this process can now handle MCA/GMCA parameters, make sure to
     * process them.
     */
    mca_base_cmd_line_process_args(&cmd_line, &environ, &environ);

    /***************
     * Initialize
     ***************/
    if (ORCM_SUCCESS != (ret = orcm_init(ORCM_DAEMON))) {
        return ret;
    }

    if(orcm_globals.help) {
        args = opal_cmd_line_get_usage_msg(&cmd_line);
        str = opal_show_help_string("help-orcmd.txt", "usage", false, args);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        orcm_finalize();
        exit(0);
    }
    
    if (orcm_globals.version) {
        str = opal_show_help_string("help-orcmd.txt", "version", false,
                                    ORCM_VERSION,
                                    PACKAGE_BUGREPORT);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        orcm_finalize();
        exit(0);
    }

    /* print out a message alerting that we are alive */
    now = time(NULL);
    /* convert the time to a simple string */
    ctmp = ctime(&now);
    /* strip the trailing newline */
    ctmp[strlen(ctmp)-1] = '\0';

    /* setup to recieve commands, even aggregators might need to answer to commands */
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_RM,
                            ORTE_RML_PERSISTENT,
                            orcmd_recv,
                            NULL);

    if (ORCM_PROC_IS_AGGREGATOR) {
        opal_output(0, "%s: ORCM aggregator %s started",
                    ctmp, ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    } else {
        opal_output(0, "\n******************************\n%s: ORCM daemon %s started and connected to aggregator %s\nMy scheduler: %s\nMy parent: %s\n******************************\n",
                    ctmp, ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                    ORTE_NAME_PRINT(ORTE_PROC_MY_DAEMON),
                    ORTE_NAME_PRINT(ORTE_PROC_MY_SCHEDULER),
                    ORTE_NAME_PRINT(ORTE_PROC_MY_PARENT));

        /* inform the scheduler */
        buf = OBJ_NEW(opal_buffer_t);
        /* pack the alloc command flag */
        if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &command,1, ORCM_RM_CMD_T))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(buf);
            return ret;
        }
        if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &state, 1, OPAL_INT8))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(buf);
            return ret;
        }
        if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, ORTE_PROC_MY_NAME, 1, ORTE_NAME))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(buf);
            return ret;
        }

        /* send hwloc topo to scheduler */
        if (NULL != opal_hwloc_topology) {
            have_hwloc_topology = true;
            if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &have_hwloc_topology, 1, OPAL_BOOL))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buf);
                return ret;
            }
            if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &opal_hwloc_topology, 1, OPAL_HWLOC_TOPO))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buf);
                return ret;
            }
        } else {
            have_hwloc_topology = false;
            if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &have_hwloc_topology, 1, OPAL_BOOL))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buf);
                return ret;
            }
        }

        if (ORTE_SUCCESS != (ret = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                                                          ORCM_RML_TAG_RM,
                                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(buf);
            return ret;
        }
    }

    while (orte_event_base_active) {
        opal_event_loop(orte_event_base, OPAL_EVLOOP_ONCE);
    }

    /* print out a message alerting that we are shutting down */
    now = time(NULL);
    /* convert the time to a simple string */
    ctmp = ctime(&now);
    /* strip the trailing newline */
    ctmp[strlen(ctmp)-1] = '\0';

    if (ORCM_PROC_IS_AGGREGATOR) {
        opal_output(0, "%s: ORCM aggregator %s shutting down with status %d (%s)",
                    ctmp, ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                    orte_exit_status,
                    (0 < orte_exit_status) ? "SYS" : ORTE_ERROR_NAME(orte_exit_status));
    } else {
        opal_output(0, "%s: ORCM daemon %s shutting down with status %d (%s)",
                    ctmp, ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                    orte_exit_status,
                    (0 < orte_exit_status) ? "SYS" : ORTE_ERROR_NAME(orte_exit_status));
    }
    
     /***************
     * Cleanup
     ***************/
    orcm_finalize();

    return ret;
}

/* timer call back function to cancel session daemons*/
static void orcmd_wpid_timer_recv(int fd, short args, void* cbdata)
{
    orcm_rm_cmd_flag_t command;
    int  rc;
    opal_buffer_t *buf;
    orcmd_wpid_event_cb_t *req = (orcmd_wpid_event_cb_t *) cbdata;
    int w = 0;
    int status = 0;
    pid_t stepd_pid;
    orcm_alloc_t *alloc;

    if (NULL == req  || 0 >= req->pid || NULL == req->cbdata) {
        opal_output(0, "error processing wpid timer recv\n");
        return;
    }
    stepd_pid = req->pid;
    alloc = (orcm_alloc_t*) req->cbdata;
    w = waitpid (stepd_pid, &status, WNOHANG|WUNTRACED|WCONTINUED);
    if (0 == w) {
        req->rate.tv_sec++;
        opal_event_evtimer_add(&req->ev, &req->rate);
    } else { /*if (stepd_pid  == w) */ 
        opal_output(0, "%s:  session: %d completed notify scheduler \n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), (int) alloc->id);
        command = ORCM_STEPD_COMPLETE_COMMAND;
        buf = OBJ_NEW(opal_buffer_t);
        /* pack the complete command flag */
        if (OPAL_SUCCESS !=
            (rc = opal_dss.pack(buf, &command, 1, ORCM_RM_CMD_T))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            return;
        }
        if (OPAL_SUCCESS !=
            (rc = opal_dss.pack(buf, &alloc, 1, ORCM_ALLOC))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            return;
        }
        if (ORTE_SUCCESS !=
            (rc =
             orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                         ORCM_RML_TAG_RM,
                         orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            return;
        }
    }
}
/* process incoming messages in order of receipt */
static void orcmd_recv(int status, orte_process_name_t* sender,
                       opal_buffer_t* buffer, orte_rml_tag_t tag,
                       void* cbdata)
{
    orcm_rm_cmd_flag_t command;
    int n, rc;
    orcm_alloc_t *alloc;
    opal_buffer_t *buf;
    orte_jobid_t jobid;
    int hnp = 0;
    int port_num = HNP_PORT_NUM;
    char *hnp_uri = NULL;
    char *hnp_ip = NULL;
    struct hostent *hnp_hostent = NULL;
    struct in_addr **addr_list;

    n = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &command, &n, ORCM_RM_CMD_T))) {
        ORTE_ERROR_LOG (rc);
        return;
    }

    switch(command) {
    case ORCM_LAUNCH_STEPD_COMMAND:

        n = 1; 
        stepd_pid = 0;
        if (OPAL_SUCCESS !=
            (rc = opal_dss.unpack(buffer, &alloc, &n, ORCM_ALLOC))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        if (OPAL_EQUAL ==
            orte_util_compare_name_fields(ORTE_NS_CMP_ALL, &alloc->hnp,
                          ORTE_PROC_MY_NAME)) {
            hnp = 1;
        }

        opal_output(0,
                "%s: allocate session : %d\n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), (int) alloc->id);

        jobid = alloc->id << 16;

        port_num++;

        /* construct the hnp uri */
        if (alloc->hnpname != NULL) {

            if ((hnp_hostent =
                 gethostbyname(alloc->hnpname)) == NULL
                || hnp_hostent->h_addr_list == NULL) {
                ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
                return;
            }

            addr_list = (struct in_addr **)hnp_hostent->h_addr_list;
            hnp_ip = strdup(inet_ntoa(*addr_list[0]));
            asprintf(&hnp_uri, "%i.0;tcp://%s:%i[$.$]", jobid, hnp_ip,
                port_num);
            alloc->hnpuri = hnp_uri;
        } 

        if (hnp_uri) {
            slm_fork_hnp_procs(jobid, port_num, hnp, hnp_uri, alloc, &stepd_pid);
        }

        command = ORCM_VM_READY_COMMAND;
        buf = OBJ_NEW(opal_buffer_t);
        /* pack the complete command flag */
        if (OPAL_SUCCESS !=
            (rc = opal_dss.pack(buf, &command, 1, ORCM_RM_CMD_T))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            if (hnp_ip) {
                free(hnp_ip);
            }
            if (hnp_uri) {
                free(hnp_uri);
            }
            return;
        }
        if (OPAL_SUCCESS !=
            (rc = opal_dss.pack(buf, &alloc, 1, ORCM_ALLOC))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            if (hnp_ip) {
                free(hnp_ip);
            }
            if (hnp_uri) {
                free(hnp_uri);
            }
            return;
        }
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                                          ORCM_RML_TAG_RM,
                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            if (hnp_ip) {
                free(hnp_ip);
            }
            if (hnp_uri) {
                free(hnp_uri);
            }
            return;
        }
        break;

    case ORCM_CANCEL_STEPD_COMMAND:
        opal_output(0,
                "%s: ORCM daemon got cancel stepd command, executing hostname",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            
        n = 1;
        if (OPAL_SUCCESS !=
            (rc = opal_dss.unpack(buffer, &alloc, &n, ORCM_ALLOC))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        orte_rml.recv_cancel(ORTE_NAME_WILDCARD,
                     ORTE_RML_TAG_ABORT);

        if (stepd_pid > 0) {
            kill_local(stepd_pid, SIGKILL);
            stepd_pid = 0;
        } else {
            opal_output(0, "%s orcmd:STEPD PID %d FAILED",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), stepd_pid);
        }
        command = ORCM_STEPD_COMPLETE_COMMAND;
        buf = OBJ_NEW(opal_buffer_t);
        /* pack the complete command flag */
        if (OPAL_SUCCESS !=
            (rc = opal_dss.pack(buf, &command, 1, ORCM_RM_CMD_T))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            return;
        }
        if (OPAL_SUCCESS !=
            (rc = opal_dss.pack(buf, &alloc, 1, ORCM_ALLOC))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            return;
        }
        if (ORTE_SUCCESS !=
            (rc =
             orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                         ORCM_RML_TAG_RM,
                         orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            return;
        }
        break;

    case ORCM_CALIBRATE_COMMAND:
        opal_output(0, "%s: RUNNING CALIBRATION",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        orcm_diag.calibrate();
        break;

    default:
        opal_output(0, "%s: COMMAND UNKNOWN",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    }

    if (hnp_ip) {
        free(hnp_ip);
    }
    if (hnp_uri) {
        free(hnp_uri);
    }
    return;
}


/* Launch stepd procs and hnp procs */
static void set_handler_default(int sig)
{
    struct sigaction act;
    act.sa_handler = SIG_DFL;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(sig, &act, (struct sigaction *)0);
}

/* Launch hnp procs */
static int
slm_fork_hnp_procs(orte_jobid_t jobid, int port_num, int hnp, char *hnp_uri, orcm_alloc_t *alloc, int *stepd_pid)
{
    char *cmd;
    char **argv = NULL;
    int argc;
    char *param;
    sigset_t sigs;
    int rc;
    char *foo;
    int s_pid;
    char **hosts = NULL;
    int i, vpid = 1;


    /* find the orcmsd binary using the install_dirs support - this also
     * checks to ensure that we can see this executable and it *is* executable by us
     */
    cmd = opal_path_access("orcmsd", opal_install_dirs.bindir, X_OK);
    if (NULL == cmd) {

        /* guess we couldn't do it - best to abort */
        ORTE_ERROR_LOG(ORTE_ERR_FILE_NOT_EXECUTABLE);
        return ORTE_ERR_FILE_NOT_EXECUTABLE;
    }

    /* okay, setup an appropriate argv */
    opal_argv_append(&argc, &argv, "orcmsd");

    /* pass it a jobid to match my job family */
    opal_argv_append(&argc, &argv, "-omca");
    opal_argv_append(&argc, &argv, "sst_orcmsd_jobid");
    if (ORTE_SUCCESS !=
        (rc = orte_util_convert_jobid_to_string(&param, jobid))) {
        ORTE_ERROR_LOG(rc);
        opal_argv_free(argv);
        free(cmd);
        return rc;
    }
    opal_argv_append(&argc, &argv, param);
    free(param);

    if( hnp ) {
        /* setup to pass the vpid */
        opal_argv_append(&argc, &argv, "-omca");
        opal_argv_append(&argc, &argv, "sst_orcmsd_vpid");
        opal_argv_append(&argc, &argv, "0");

        /* tell the daemon it is to be the HNP */
        opal_argv_append(&argc, &argv, "--hnp");

        /* add my parents uri to send the vm ready from the session HNP */
        if ( NULL != alloc->parent_uri ) {
            opal_argv_append(&argc, &argv, "--parent-uri");
            opal_argv_append(&argc, &argv, alloc->parent_uri);
        }

        if (true == alloc->interactive) {
            opal_argv_append(&argc, &argv, "--persistent");
        }

        if (NULL != alloc->batchfile) {
            opal_argv_append(&argc, &argv, "--persistent");
            opal_argv_append(&argc, &argv, "--batchfile");
            opal_argv_append(&argc, &argv, alloc->batchfile);
        }

        opal_argv_append(&argc, &argv, "-omca");
        opal_argv_append(&argc, &argv, "oob_tcp_static_ipv4_ports");
        if (0 > asprintf(&param, "%d", port_num)) {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            opal_argv_free(argv);
            free(cmd);
            return ORTE_ERR_OUT_OF_RESOURCE;
        }
        opal_argv_append(&argc, &argv, param);
        free(param);

    } else {
        /* extract the hosts */
        if (NULL != alloc->nodes) {
            opal_output_verbose(2, orcm_globals.output,
                "REGEX ### nodes = %s", alloc->nodes);
            orte_regex_extract_node_names (alloc->nodes, &hosts); 
            for (i=0; hosts[i] != NULL; i++) {
                if (0 == strcmp(orte_process_info.nodename, hosts[i])) {
                    opal_output_verbose(2, orcm_globals.output,
                        "REGEX ### node name = %s vpid = %d \n", hosts[i], i);
                    vpid = i;
                }
            }
        }

        /* setup to pass the vpid */
        opal_argv_append(&argc, &argv, "-omca");
        opal_argv_append(&argc, &argv, "sst_orcmsd_vpid");
        if (0 > asprintf(&param, "%d", vpid)) {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            opal_argv_free(argv);
            opal_argv_free(hosts);
            free(cmd);
            return ORTE_ERR_OUT_OF_RESOURCE;
        }
        opal_argv_append(&argc, &argv, param);
        free(param);
        /* pass the uri of the hnp */
        opal_argv_append(&argc, &argv, "-omca");
        opal_argv_append(&argc, &argv, "orte_hnp_uri");
        opal_argv_append(&argc, &argv, hnp_uri);
        opal_argv_append(&argc, &argv, "-omca");
        opal_argv_append(&argc, &argv, "oob_tcp_static_ipv4_ports");
        opal_argv_append(&argc, &argv, "0");

    }

    /* if we have static ports, pass the node list */
    if (NULL != alloc->nodes) {
        opal_argv_append(&argc, &argv, "-omca");
        opal_argv_append(&argc, &argv, "sst_orcmsd_node_regex");
        opal_argv_append(&argc, &argv, alloc->nodes);
    }

    /* add any debug flags */
    if (orte_debug_flag) {
        opal_argv_append(&argc, &argv, "--debug");
    }
    if (orte_debug_daemons_flag) {
        opal_argv_append(&argc, &argv, "--debug-daemons");
    }
    if (orte_debug_daemons_file_flag) {
        if (!orte_debug_daemons_flag) {
            opal_argv_append(&argc, &argv, "--debug-daemons");
        }
        opal_argv_append(&argc, &argv, "--debug-daemons-file");
    }

    if (orte_debug_flag) {
        opal_argv_append(&argc, &argv, "-omca");
        opal_argv_append(&argc, &argv, "oob_base_verbose");
        opal_argv_append(&argc, &argv, "100");
        opal_argv_append(&argc, &argv, "-omca");
        opal_argv_append(&argc, &argv, "plm_base_verbose");
        opal_argv_append(&argc, &argv, "100");
        opal_argv_append(&argc, &argv, "-omca");
        opal_argv_append(&argc, &argv, "state_base_verbose");
        opal_argv_append(&argc, &argv, "100");
    }
    opal_argv_append(&argc, &argv, "-omca");
    opal_argv_append(&argc, &argv, "rmaps_base_oversubscribe");
    opal_argv_append(&argc, &argv, "1");

    foo = opal_argv_join(argv, ' ');
    if( hnp ) {
        opal_output_verbose(2, orcm_globals.output,
                "%s FORKING HNP : %s ...\n\n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), foo);
    } else {
        sleep(2);
        opal_output_verbose(2, orcm_globals.output,
                "%s FORKING STEPD DAEMONS: %s ...\n\n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), foo);
    }
    free(foo);

    /* Fork off the child */

    s_pid = fork();
    if (s_pid < 0) {
        ORTE_ERROR_LOG(ORTE_ERR_SYS_LIMITS_CHILDREN);
        free(cmd);
        opal_argv_free(argv);
        opal_argv_free(hosts);
        return ORTE_ERR_SYS_LIMITS_CHILDREN;
    }
    if (s_pid == 0) {

        /* I am the child - exec me */

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

        if (alloc->caller_gid) {
            rc = setgid(alloc->caller_gid);
            if (rc == -1) {
                opal_output(0, "%s : stepd gid %d error %s\n",
                    orte_process_info.nodename, alloc->caller_gid, strerror(errno));
                orte_show_help("help-orcmd.txt", "orcmd:execv-error",
                           true, cmd, strerror(errno));
                exit(1);
            }
        }
        if (alloc->caller_uid) {
            rc = setuid(alloc->caller_uid);
            if (rc == -1) {
                opal_output(0, "%s : stepd uid %d  error  %s\n",
                    orte_process_info.nodename, alloc->caller_uid, strerror(errno));
                orte_show_help("help-orcmd.txt", "orcmd:execv-error",
                           true, cmd, strerror(errno));
                exit(1);
            }
        }

        execv(cmd, argv);

        /* if I get here, the execv failed! */
        orte_show_help("help-orcmd.txt", "orcmd:execv-error",
                   true, cmd, strerror(errno));
        exit(1);
    } else {

        /* I am the parent - report the stepd pid back
         */
        if (NULL != stepd_pid) {
            *stepd_pid = s_pid;
        }
        orcmd_wpid_event_cb_t * req;
        req = OBJ_NEW(orcmd_wpid_event_cb_t);
        req->cbdata = (void *)alloc;
        req->rate.tv_sec=5;
        req->rate.tv_usec=0;
        req->pid = s_pid;
        opal_event_evtimer_set(orte_event_base, &req->ev,
                              orcmd_wpid_timer_recv, req);
        opal_event_evtimer_add(&req->ev, &req->rate);

        opal_argv_free(argv);
        opal_argv_free(hosts);
        free(cmd);
        /* all done - report success */
        return ORTE_SUCCESS;
    }
}

static int kill_local(pid_t pid, int signum)
{
    pid_t pgrp;

#if HAVE_SETPGID
    pgrp = getpgid(pid);
    if (-1 != pgrp) {
        /* target the lead process of the process
         * group so we ensure that the signal is
         * seen by all members of that group. This
         * ensures that the signal is seen by any
         * child processes our child may have
         * started
         */
        pid = pgrp;
    }
#endif
    if (0 != kill(pid, signum)) {
        if (ESRCH != errno) {
            opal_output_verbose(2, orcm_globals.output,
                                 "%s orcmd:SENT KILL %d TO STEPD PID %d GOT ERRNO %d",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), signum, (int)pid, errno);
            return errno;
        }
    }
    opal_output(0, "%s orcmd:SENT KILL %d TO STEPD PID %d SUCCESS",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), signum, (int)pid);
    opal_output_verbose(2, orcm_globals.output,
                         "%s orcmd:SENT KILL %d TO STEPD PID %d SUCCESS",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), signum, (int)pid);
    return 0;
}
