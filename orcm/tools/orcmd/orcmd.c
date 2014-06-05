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

#include "opal/util/cmd_line.h"
#include "opal/util/opal_environ.h"
#include "opal/util/show_help.h"
#include "opal/mca/base/mca_base_var.h"
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

static struct {
    bool help;
    bool version;
    bool verbose;
    bool master;
} orcm_globals;

#define HNP_PORT_NUM 12345

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
static int slm_fork_hnp_procs(orte_jobid_t jobid, uid_t uid, gid_t gid,
                       char *nodes, int port_num, int hnp, char *hnp_uri);

int main(int argc, char *argv[])
{
    int ret;
    opal_cmd_line_t cmd_line;
    char *ctmp;
    time_t now;
    opal_buffer_t *buf;
    orcm_rm_cmd_flag_t command = ORCM_NODESTATE_UPDATE_COMMAND;
    orcm_node_state_t state = ORCM_NODE_STATE_UP;

    /* process the cmd line arguments to get any MCA params on them */
    opal_cmd_line_create(&cmd_line, cmd_line_init);
    mca_base_cmd_line_setup(&cmd_line);
    if (ORCM_SUCCESS != (ret = opal_cmd_line_parse(&cmd_line, false, argc, argv)) ||
        orcm_globals.help) {
        char *args = NULL;
        args = opal_cmd_line_get_usage_msg(&cmd_line);
        fprintf(stderr, "Usage: %s [OPTION]...\n%s\n", argv[0], args);
        free(args);
        return ret;
    }
    
    if (orcm_globals.version) {
        fprintf(stderr, "orcm %s\n", ORCM_VERSION);
        exit(0);
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

#define ORCM_URI_MSG_LGTH   256
    static int death_pipe[2];
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
    char hnp_uri[ORCM_URI_MSG_LGTH];
    char *hnp_ip;
    struct hostent *hnp_hostent = NULL;
    struct in_addr **addr_list;

    n = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &command, &n, ORCM_RM_CMD_T))) {
        ORTE_ERROR_LOG (rc);
        return;
    }

    switch(command) {
    case ORCM_LAUNCH_STEPD_COMMAND:
        opal_output(0,
                "%s: ORCM daemon got STEPD request, launching STEPD",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

        n = 1;
        if (OPAL_SUCCESS !=
            (rc = opal_dss.unpack(buffer, &alloc, &n, ORCM_ALLOC))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        fprintf(stderr, "IS HNP : %s, %s ...\n\n",
            ORTE_NAME_PRINT(&alloc->hnp),
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        if (OPAL_EQUAL ==
            orte_util_compare_name_fields(ORTE_NS_CMP_ALL, &alloc->hnp,
                          ORTE_PROC_MY_NAME)) {
            hnp = 1;
        }

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
            sprintf(hnp_uri, "%i.0;tcp://%s:%i[$.$]", jobid, hnp_ip,
                port_num);
            free(hnp_ip);
        }

        slm_fork_hnp_procs(jobid, alloc->caller_uid,
                     alloc->caller_gid, alloc->nodes, port_num, hnp, hnp_uri);

        
        system("hostname");
        sleep(5);
        command = ORCM_VM_READY_COMMAND;
        buf = OBJ_NEW(opal_buffer_t);
        /* pack the complete command flag */
        if (OPAL_SUCCESS !=
            (rc = opal_dss.pack(buf, &command, 1, ORCM_RM_CMD_T))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        if (OPAL_SUCCESS !=
            (rc = opal_dss.pack(buf, &alloc, 1, ORCM_ALLOC))) {
            ORTE_ERROR_LOG(rc);
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

    case ORCM_CANCEL_STEPD_COMMAND:
        opal_output(0,
                "%s: ORCM daemon got cancel stepd command, executing hostname",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        orte_rml.recv_cancel(ORTE_NAME_WILDCARD,
                     ORTE_RML_TAG_ABORT);
        close(death_pipe[1]);

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

    case ORCM_CALIBRATE:
        opal_output(0, "%s: RUNNING CALIBRATION",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        orcm_diag.calibrate();
        break;

    default:
        opal_output(0, "%s: COMMAND UNKNOWN",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
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
slm_fork_hnp_procs(orte_jobid_t jobid, uid_t uid, gid_t gid,
         char *nodes, int port_num, int hnp, char * hnp_uri)
{
    char *cmd;
    char **argv = NULL;
    int argc;
    char *param;
    sigset_t sigs;
    int rc;
    char *foo;
    int stepd_pid;
    int num_procs;


    /* we also have to give the HNP a pipe it can watch to know when
     * we terminated. Since the HNP is going to be a child of us, it
     * can't just use waitpid to see when we leave - so it will watch
     * the pipe instead
     */
    if (pipe(death_pipe) < 0) {
        ORTE_ERROR_LOG(ORTE_ERR_SYS_LIMITS_PIPES);
        return ORTE_ERR_SYS_LIMITS_PIPES;
    }

    /* find the orted binary using the install_dirs support - this also
     * checks to ensure that we can see this executable and it *is* executable by us
     */
    cmd = opal_path_access("orted", opal_install_dirs.bindir, X_OK);
    if (NULL == cmd) {

        /* guess we couldn't do it - best to abort */
        ORTE_ERROR_LOG(ORTE_ERR_FILE_NOT_EXECUTABLE);
        return ORTE_ERR_FILE_NOT_EXECUTABLE;
    }

    /* okay, setup an appropriate argv */
    opal_argv_append(&argc, &argv, "orted");

    if( hnp ) {
        /* tell the daemon it is to be the HNP */
        opal_argv_append(&argc, &argv, "--hnp");

        /* tell the daemon to report back its uri so we can connect to it */
        opal_argv_append(&argc, &argv, "--report-uri");
        opal_argv_append(&argc, &argv, "1");

        /* pass it a jobid to match my job family */
        opal_argv_append(&argc, &argv, "-mca");
        opal_argv_append(&argc, &argv, "ess_base_jobid");
        if (ORTE_SUCCESS !=
            (rc = orte_util_convert_jobid_to_string(&param, jobid))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        opal_argv_append(&argc, &argv, param);
        free(param);
        opal_argv_append(&argc, &argv, "-mca");
        opal_argv_append(&argc, &argv, "oob_tcp_static_ipv4_ports");
        if (0 > asprintf(&param, "%d", port_num)) {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            return ORTE_ERR_OUT_OF_RESOURCE;
        }
        opal_argv_append(&argc, &argv, param);
        free(param);

        /* RHC: I'm not sure we want the step daemons monitoring a pipe - raises
         * questions about security. If the root-level daemon dies, we will
         * likely want the job to continue running until/unless we reboot
         * the node */
        /* give the daemon a pipe it can watch to tell when we have died */
        opal_argv_append(&argc, &argv, "--singleton-died-pipe");
        asprintf(&param, "%d", death_pipe[0]);
        opal_argv_append(&argc, &argv, param);
        free(param);

    } else {
        /* pass the uri of the hnp */
        asprintf(&param, "\"%s\"", hnp_uri);
        opal_argv_append(&argc, &argv, "-mca");
        opal_argv_append(&argc, &argv, "orte_hnp_uri");
        opal_argv_append(&argc, &argv, param);
        free(param);
        opal_argv_append(&argc, &argv, "-mca");
        opal_argv_append(&argc, &argv, "oob_tcp_static_ipv4_ports");
        opal_argv_append(&argc, &argv, "0");

        /* pass the daemon jobid */
        opal_argv_append(&argc, &argv, "-mca");
        opal_argv_append(&argc, &argv, "orte_ess_jobid");
        opal_argv_append(&argc, &argv, "0");

        /* setup to pass the vpid */
        opal_argv_append(&argc, &argv, "-mca");
        opal_argv_append(&argc, &argv, "orte_ess_vpid");
        opal_argv_append(&argc, &argv, "0");

        /* setup to pass the num_procs */
        num_procs = orte_process_info.num_procs;
        opal_argv_append(&argc, &argv, "-mca");
        opal_argv_append(&argc, &argv, "orte_ess_num_procs");
        asprintf(&param, "%lu", num_procs);
        opal_argv_append(&argc, &argv, param);
        free(param);

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


    /* if we have static ports, pass the node list */
    if (NULL != nodes) {
        opal_argv_append(&argc, &argv, "--nodes");
        opal_argv_append(&argc, &argv, nodes);
    }

    if (orte_debug_daemons_file_flag) {
        opal_argv_append(&argc, &argv, "-mca");
        opal_argv_append(&argc, &argv, "oob_base_verbose");
        opal_argv_append(&argc, &argv, "100");
    }

    foo = opal_argv_join(argv, ' ');
    if( hnp ) {
        opal_output(0, "%s FORKING HNP: %s ...\n\n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), foo);
    } else {
        sleep(2);
        opal_output(0, "%s FORKING STEPD DAEMONS: %s ...\n\n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), foo);
    }
    free(foo);

    /* Fork off the child */

    stepd_pid = fork();
    if (stepd_pid < 0) {
        ORTE_ERROR_LOG(ORTE_ERR_SYS_LIMITS_CHILDREN);
        close(death_pipe[0]);
        close(death_pipe[1]);
        free(cmd);
        opal_argv_free(argv);
        return ORTE_ERR_SYS_LIMITS_CHILDREN;
    }
    if (stepd_pid == 0) {
        close(death_pipe[1]);

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

/*
        rc = setuid(uid);
        rc = setgid(gid);

        if (rc == -1) {
            fprintf(stderr, "%s : stepd uid gid error %s\n",
                orte_process_info.nodename, strerror(errno));
            exit(1);
        }
*/
        execv(cmd, argv);

        /* if I get here, the execv failed! */
        orte_show_help("help-ess-base.txt", "ess-base:execv-error",
                   true, cmd, strerror(errno));
        exit(1);
    } else {

        /* I am the parent - wait to hear something back and
         * report results
         */
        close(death_pipe[0]);    /* parent closes the death_pipe's read */
        opal_argv_free(argv);

        /* all done - report success */
        return ORTE_SUCCESS;
    }
}
