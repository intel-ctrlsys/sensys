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

#include "orcm/runtime/runtime.h"

#include "orcm/mca/scd/base/base.h"

/******************
 * Local Functions
 ******************/
static int orcm_osub_init(int argc, char *argv[]);
static int parse_args(int argc, char *argv[]);

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
      'e', NULL, "interactive",
      0,
      &orcm_osub_globals.interactive, OPAL_CMD_LINE_TYPE_BOOL,
      "Do not share allocated nodes with other sessions" },

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
    opal_buffer_t *buf;
    int rc, n;
    orcm_scd_cmd_flag_t command=ORCM_SESSION_REQ_COMMAND;
    orcm_alloc_id_t id;
    struct timeval tv;

    /* initialize, parse command line, and setup frameworks */
    orcm_osub_init(argc, argv);

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
    alloc.interactive = orcm_osub_globals.interactive; // true if in interactive mode
    alloc.nodes = '\0';                                // regex of nodes to be used
    alloc.parent_name = ORTE_NAME_PRINT(ORTE_PROC_MY_NAME); // my_daemon_name
    alloc.parent_uri = '\0';                           // my_daemon uri address
    /* alloc.constraints = orcm_osub_globals.resources */ ; // list of resource constraints to be applied when selecting hosts
    alloc.hnpname = '\0'; //my hnp name
    alloc.hnpuri = '\0'; //my hnp uri

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
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &id, &n, ORCM_ALLOC_ID_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
    opal_output(0, "RECEIVED ALLOC ID %d", (int)id);

    if (ORTE_SUCCESS != orcm_finalize()) {
        fprintf(stderr, "Failed orcm_finalize\n");
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
                                '\0',     /* account */
                                '\0',     /* name */
                                -1,       /* gid */
                                0,        /* max_nodes */
                                0,        /* max_pes */
                                1,        /* min_nodes */
                                1,        /* min_pes */
                                '\0',     /* starttime */
                                '\0',     /* walltime */
                                false,    /* exclusive */
                                false,    /* interactive */
                                '\0',     /* nodefile */
                                '\0'};    /* resources */

    orcm_osub_globals = tmp;
    char str[10];

    /* Parse the command line options */
    opal_cmd_line_create(&cmd_line, cmd_line_opts);
    
    mca_base_cmd_line_setup(&cmd_line);
    ret = opal_cmd_line_parse(&cmd_line, false, argc, argv);
    
    if (OPAL_SUCCESS != ret) {
        if (OPAL_ERR_SILENT != ret) {
            fprintf(stderr, "%s: command line error (%s)\n", argv[0],
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
        sprintf(str, "%u", getgid());
        orcm_osub_globals.gid = (int)strtol(str, NULL, 10);
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
