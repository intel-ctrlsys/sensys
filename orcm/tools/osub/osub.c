/*
 * Copyright (c) 2004-2010 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006-2013 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007      Sun Microsystems, Inc.  All rights reserved.
 * Copyright (c) 2007      Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * Copyright (c) 2010      Oracle and/or its affiliates.  All rights reserved.
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
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif  /*  HAVE_STDLIB_H */
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif  /* HAVE_SYS_STAT_H */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif  /* HAVE_SYS_WAIT_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */

#include "opal/dss/dss.h"
#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/mca/event/event.h"
#include "opal/util/basename.h"
#include "opal/util/cmd_line.h"
#include "opal/util/output.h"
#include "opal/util/opal_environ.h"
#include "opal/util/show_help.h"
#include "opal/mca/base/base.h"
#include "opal/runtime/opal.h"
#if OPAL_ENABLE_FT_CR == 1
#include "opal/runtime/opal_cr.h"
#endif

#include "orte/runtime/orte_wait.h"
#include "orte/util/error_strings.h"
#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/rml/rml_types.h"
#include "orte/mca/rml/base/rml_contact.h"

#include "orcm/runtime/runtime.h"

#include "orcm/mca/scd/base/base.h"

/******************
 * Local Functions
 ******************/
static int orcm_osub_init(int argc, char *argv[]);
static int parse_args(int argc, char *argv[]);
static int osub_exec_shell(char *shell,  char **env, orcm_alloc_t *alloc);

/*****************************************
 * Global Vars for Command line Arguments
 *****************************************/
typedef struct {
    bool help;
    bool verbose;
    int output;
    char *account;
    char *name;
    int gid;
    int max_nodes;
    int max_pes;
    int min_nodes;
    int min_pes;
    char *starttime;
    char *walltime;
    bool exclusive;
    bool interactive;
    char *nodefile;
    char *resources;
    char *batchfile;
    char *ishell;
} orcm_osub_globals_t;

orcm_osub_globals_t orcm_osub_globals;

opal_cmd_line_init_t cmd_line_opts[] = {
    { NULL,
      'h', NULL, "help",
      0,
      &orcm_osub_globals.help, OPAL_CMD_LINE_TYPE_BOOL,
      "This help message" },

    { NULL,
      'v', NULL, "verbose",
      0,
      &orcm_osub_globals.verbose, OPAL_CMD_LINE_TYPE_BOOL,
      "Be Verbose" },

    { NULL,
      'a', NULL, "account",
      1,
      &orcm_osub_globals.account, OPAL_CMD_LINE_TYPE_STRING,
      "Account to be charged" },

    { NULL,
      '\0', NULL, "project",
      1,
      &orcm_osub_globals.name, OPAL_CMD_LINE_TYPE_STRING,
      "User assigned project name" },

    { NULL,
      'g', NULL, "gid",
      1,
      &orcm_osub_globals.gid, OPAL_CMD_LINE_TYPE_INT,
      "Group id to run session under" },

    { NULL,
      'N', NULL, "max-node",
      1,
      &orcm_osub_globals.max_nodes, OPAL_CMD_LINE_TYPE_INT,
      "Max nodes allowed in allocation" },

    { NULL,
      'P', NULL, "max-pe",
      1,
      &orcm_osub_globals.max_pes, OPAL_CMD_LINE_TYPE_INT,
      "Max PEs allowed in allocation" },

    { NULL,
      'n', NULL, "node",
      1,
      &orcm_osub_globals.min_nodes, OPAL_CMD_LINE_TYPE_INT,
      "Minimum number of nodes required for allocation" },

    { NULL,
      'p', NULL, "pe",
      1,
      &orcm_osub_globals.min_pes, OPAL_CMD_LINE_TYPE_INT,
      "Minimum number of PEs required for allocation" },

    { NULL,
      's', NULL, "start",
      1,
      &orcm_osub_globals.starttime, OPAL_CMD_LINE_TYPE_STRING,
      "Earliest Date/Time required to start job" },

    { NULL,
      'w', NULL, "walltime",
      1,
      &orcm_osub_globals.walltime, OPAL_CMD_LINE_TYPE_STRING,
      "Maximum duration before job is terminated" },

    { NULL,
      'e', NULL, "exclusive",
      0,
      &orcm_osub_globals.exclusive, OPAL_CMD_LINE_TYPE_BOOL,
      "Do not share allocated nodes with other sessions" },

    { NULL,
      'i', NULL, "interactive",
      0,
      &orcm_osub_globals.interactive, OPAL_CMD_LINE_TYPE_BOOL,
      "Do not share allocated nodes with other sessions" },

    { NULL,
      'b', NULL, "batchrun",
      1,
      &orcm_osub_globals.batchfile, OPAL_CMD_LINE_TYPE_STRING,
      "Path to batch script file listing applications to run" },

    { NULL,
      'f', NULL, "nodefile",
      1,
      &orcm_osub_globals.nodefile, OPAL_CMD_LINE_TYPE_STRING,
      "Path to file listing names of candidate nodes" },

    { NULL,
      'c', NULL, "constraints",
      1,
      &orcm_osub_globals.resources, OPAL_CMD_LINE_TYPE_STRING,
      "Resource constraints to be applied" },

    /* End of list */
    { NULL,
      '\0', NULL, NULL,
      0,
      NULL, OPAL_CMD_LINE_TYPE_NULL,
      NULL }
};

int
main(int argc, char *argv[])
{
    orcm_alloc_t alloc, *aptr;
    orte_rml_recv_cb_t xfer;
    orte_rml_recv_cb_t xbuffer;
    opal_buffer_t *buf;
    int rc, n;
    orcm_scd_cmd_flag_t command=ORCM_SESSION_REQ_COMMAND;
    orcm_alloc_id_t id;
    struct timeval tv;
    char *hnp_uri;


    /* initialize, parse command line, and setup frameworks */
    if (ORCM_SUCCESS != (rc=orcm_osub_init(argc, argv))) {
            ORTE_ERROR_LOG(rc);
            return rc;
    }

    /* create an allocation request */
    OBJ_CONSTRUCT(&alloc, orcm_alloc_t);

    alloc.priority = 1;                                // session priority
    alloc.account = orcm_osub_globals.account;         // account to be charged
    alloc.name = orcm_osub_globals.name;               // user-assigned project name
    alloc.gid = orcm_osub_globals.gid;                 // group id to be run under
    alloc.max_nodes = orcm_osub_globals.max_nodes;     // max number of nodes
    alloc.max_pes = orcm_osub_globals.max_pes;         // max number of processing elements
    alloc.min_nodes = orcm_osub_globals.min_nodes;     // min number of nodes required
    alloc.min_pes = orcm_osub_globals.min_pes;         // min number of pe's required
    alloc.exclusive = orcm_osub_globals.exclusive;     // true if nodes to be exclusively allocated (i.e., not shared across sessions)
    alloc.interactive = false; // true if in interactive mode
    alloc.nodes = NULL;                                // regex of nodes to be used
    alloc.parent_name = ORTE_NAME_PRINT(ORTE_PROC_MY_NAME); // my_daemon_name
    alloc.parent_uri = NULL;                           // my_daemon uri address
    alloc.batchfile = NULL; // batchfile 


    /* alloc.constraints = orcm_osub_globals.resources */ ; // list of resource constraints to be applied when selecting hosts
    alloc.hnpname = NULL; //my hnp name
    alloc.hnpuri = NULL; //my hnp uri

    alloc.caller_uid = getuid();   // caller uid, not from args
    alloc.caller_gid = getgid();   // caller gid, not from args

    if (NULL == orcm_osub_globals.starttime || 0 == strlen(orcm_osub_globals.starttime)) {
        gettimeofday(&tv,NULL);
        /* desired start time for allocation deafults to now */
        alloc.begin = tv.tv_sec;
    } else {
        /* TODO: eventually parse the string to figure out what user means, for now its now */
        gettimeofday(&tv,NULL);
        alloc.begin = tv.tv_sec;
    }

    if (NULL == orcm_osub_globals.walltime || 0 == strlen(orcm_osub_globals.walltime)) {
        /* desired walltime default to 10 min */
        alloc.walltime = 600;
    } else {
        /* get this in seconds for now, but will be parsed for more complexity later */
        alloc.walltime = (time_t)strtol(orcm_osub_globals.walltime, NULL, 10);                               // max execution time
    }

    if (true == orcm_osub_globals.interactive && 
        NULL != orcm_osub_globals.batchfile) {
            opal_output(0, "osub: parameter error '--interactive' and '--batchrun <batchscript>' are mutually exclusive\n");
            rc = ORCM_ERR_BAD_PARAM;
            ORTE_ERROR_LOG(rc);
            return rc;
    }

    /* 
     * interactive mode operations
     */
    if (true == orcm_osub_globals.interactive) {
        alloc.interactive = true;
        /* post a receive */
        OBJ_CONSTRUCT(&xbuffer, orte_rml_recv_cb_t);
        xbuffer.active = true;
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORCM_RML_TAG_VM_READY,
                                ORTE_RML_NON_PERSISTENT,
                                orte_rml_recv_callback, &xbuffer);
        /*setup parent_uri to receive contact info */
        alloc.parent_uri = orte_rml.get_contact_info();
    } else {
        /* 
         * default settings is batch run
         */
        if (NULL != orcm_osub_globals.batchfile) {
            alloc.batchfile=orcm_osub_globals.batchfile;
        } else {
            opal_output(0, "osub: missing required parameter '--batchrun <batchscript>'\n");
            rc = ORCM_ERR_BAD_PARAM;
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SCD,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);

    /* send it to the scheduler */
    buf = OBJ_NEW(opal_buffer_t);
    /* pack the alloc command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    aptr = &alloc;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &aptr, 1, ORCM_ALLOC))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                                                      ORCM_RML_TAG_SCD,
                                                      orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    /* get our allocated jobid */
    n=1;
    ORTE_WAIT_FOR_COMPLETION(xfer.active);
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &id, &n, 
                                              ORCM_ALLOC_ID_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
    printf("RECEIVED ALLOC ID %d", (int)id);

    if (true == alloc.interactive) {
        alloc.id = id;
        ORTE_WAIT_FOR_COMPLETION(xbuffer.active);

        /* unpack the command */
        n = 1;
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(&xbuffer.data, &command, &n, 
                                                  ORCM_RM_CMD_T))) {
            ORTE_ERROR_LOG(rc);
            return rc;
         }

         /* now process the command locally */
         switch(command) {

         case ORCM_VM_READY_COMMAND:
             n = 1;
             if (ORTE_SUCCESS != (rc = opal_dss.unpack(&xbuffer.data, &hnp_uri, &n, OPAL_STRING))) {
                 ORTE_ERROR_LOG(rc);
                 return rc;
             }

             /* exec the session */
             /* save the environment for launch purposes. This MUST be
              * done so that we can pass it to any local procs we
              * spawn - otherwise, those local procs won't see any
              * non-MCA envars that were set in the enviro when the
              * orted was executed - e.g., by .csh
              */
             orte_launch_environ = opal_argv_copy(environ);
    
             /* purge any ess flag set in the environ when we were launched */
             opal_unsetenv(OPAL_MCA_PREFIX"ess", &orte_launch_environ);

             /*
              * set hnp uri 
              */
             if (ORTE_SUCCESS != (rc = opal_setenv("ORCM_MCA_HNP_URI", hnp_uri, 
                 true, &orte_launch_environ))) {
                 ORTE_ERROR_LOG(rc);
                 return rc;
             }

             /*
              * set the prompt
              */
             if (ORTE_SUCCESS != (rc = opal_setenv("PS1", "orcmshell%", 
                 true, &orte_launch_environ))) {
                 ORTE_ERROR_LOG(rc);
                 return rc;
             }

             /*
              * exec the shell
              */
             osub_exec_shell(orcm_osub_globals.ishell, orte_launch_environ, &alloc);
             break;

         default:
            ORTE_UPDATE_EXIT_STATUS(ORTE_ERROR_DEFAULT_EXIT_CODE);
         }
    }

    if (ORTE_SUCCESS != orcm_finalize()) {
        opal_output(0, "Failed orcm_finalize\n");
        exit(1);
    }

    return ORTE_SUCCESS;
}

static int parse_args(int argc, char *argv[]) 
{
    int ret;
    opal_cmd_line_t cmd_line;
    orcm_osub_globals_t tmp = { false,    /* help */
                                false,    /* verbose */
                                -1,       /* output */
                                NULL,     /* account */
                                NULL,     /* name */
                                -1,        /* gid */
                                0,        /* max_nodes */
                                0,        /* max_pes */
                                1,        /* min_nodes */
                                1,        /* min_pes */
                                NULL,     /* starttime */
                                NULL,     /* walltime */
                                false,    /* exclusive */
                                false,    /* interactive */
                                NULL,     /* nodefile */
                                NULL,     /* resources */
                                NULL,     /* batchfile*/
                                "/bin/sh"};     /* ishell*/

    orcm_osub_globals = tmp;

    /* Parse the command line options */
    opal_cmd_line_create(&cmd_line, cmd_line_opts);
    
    mca_base_cmd_line_setup(&cmd_line);
    ret = opal_cmd_line_parse(&cmd_line, false, argc, argv);
    
    if (OPAL_SUCCESS != ret) {
        if (OPAL_ERR_SILENT != ret) {
            opal_output(0, "%s: command line error (%s)\n", argv[0],
                    opal_strerror(ret));
        }
        return ret;
    }

    /**
     * Now start parsing our specific arguments
     */
    if (orcm_osub_globals.help) {
        char *str, *args = NULL;
        args = opal_cmd_line_get_usage_msg(&cmd_line);
        str = opal_show_help_string("help-osub.txt", "usage", true,
                                    args);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        free(args);
        /* If we show the help message, that should be all we do */
        exit(0);
    }

    /* if user hasn't supplied a group to run under, use effective gid of caller */
    /* TODO: double check if user is in group */
    /* do we also need to support the name as well as id? */
    if (-1 == orcm_osub_globals.gid) {
        orcm_osub_globals.gid = getgid();
    }

    if (orcm_osub_globals.max_nodes < orcm_osub_globals.min_nodes) {
       orcm_osub_globals.max_nodes = orcm_osub_globals.min_nodes;
    } 
    if (orcm_osub_globals.max_pes < orcm_osub_globals.min_pes) {
       orcm_osub_globals.max_pes = orcm_osub_globals.min_pes;
    } 

    /*
     * Since this process can now handle MCA/GMCA parameters, make sure to
     * process them.
     */
    mca_base_cmd_line_process_args(&cmd_line, &environ, &environ);

    return ORTE_SUCCESS;
}

static int orcm_osub_init(int argc, char *argv[]) 
{
    int ret;

    /*
     * Make sure to init util before parse_args
     * to ensure installdirs is setup properly
     * before calling mca_base_open();
     */
    if( ORTE_SUCCESS != (ret = opal_init_util(&argc, &argv)) ) {
        return ret;
    }

    /*
     * Parse Command Line Arguments
     */
    if (ORTE_SUCCESS != (ret = parse_args(argc, argv))) {
        return ret;
    }

    /*
     * Setup OPAL Output handle from the verbose argument
     */
    if( orcm_osub_globals.verbose ) {
        orcm_osub_globals.output = opal_output_open(NULL);
        opal_output_set_verbosity(orcm_osub_globals.output, 10);
    } else {
        orcm_osub_globals.output = 0; /* Default=STDERR */
    }

    ret = orcm_init(ORCM_TOOL);

    return ret;
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

static int osub_exec_shell(char *shell,  char **env, orcm_alloc_t *alloc)
{
    char **argv = NULL;
    int argc = 0;
    int rc = -1;
    int w = 0;
    int status = 0;
    orcm_alloc_id_t id = alloc->id;
    pid_t pid;
    orcm_rm_cmd_flag_t command;
    opal_buffer_t *buf;
    pid = fork();
    if (pid < 0) {
        ORTE_ERROR_LOG(ORTE_ERR_SYS_LIMITS_CHILDREN);
        return rc;
    }

    if (pid == 0) {
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
        printf("%s IShell Start: %s \n",
               ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), shell);

        if (ORTE_SUCCESS != (rc = opal_argv_append(&argc, &argv, shell))) {
                 ORTE_ERROR_LOG(rc);
                 return rc;
        }

        if (ORTE_SUCCESS != (rc = opal_argv_append(&argc, &argv, "-i"))) {
                 ORTE_ERROR_LOG(rc);
                 return rc;
        }

        rc = execve(argv[0], argv, env);
        printf("%s IShell execve - %d errno - %d\n",
               ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), rc, errno);
        exit(rc);
    } else {
        w = waitpid (pid, &status, 0);
        if (w == pid) {
            printf("%s:  session: %d completed notify scheduler \n",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),(int) id);
            command = ORCM_SESSION_CANCEL_COMMAND;
            buf = OBJ_NEW(opal_buffer_t);
            /* pack the complete command flag */
            if (OPAL_SUCCESS !=
                (rc = opal_dss.pack(buf, &command, 1, ORCM_SCD_CMD_T))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(buf);
                return rc;
            }
            if (OPAL_SUCCESS !=
                (rc = opal_dss.pack(buf, &id, 1, ORCM_ALLOC_ID_T))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(buf);
                return rc;
            }
            if (ORTE_SUCCESS !=
                (rc =
                 orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                             ORCM_RML_TAG_SCD,
                             orte_rml_send_callback, NULL))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(buf);
                return rc;
            }
        }
    }
    return ORTE_SUCCESS;
}
