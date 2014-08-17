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
#include "orcm/util/cli.h"

/******************
 * Local Functions
 ******************/
static int orcm_ocli_init(int argc, char *argv[]);
static int parse_args(int argc, char *argv[]);

/*****************************************
 * Global Vars for Command line Arguments
 *****************************************/
typedef struct {
    bool help;
    bool verbose;
    int output;
} orcm_ocli_globals_t;

orcm_ocli_globals_t orcm_ocli_globals;

static orcm_cli_init_t cli_init[] = {
    /****** resource command ******/
    {NULL, "resource", 0, 1, "Resource Information"},
    // status subcommand
    {"resource", "status", 0, 0, "Resource Status"},
    // availability subcommand
    {"resource", "availability", 0, 0, "Resource Availability"},

    /****** queue command ******/
    {NULL, "queue", 0, 1, "Queue Information"},
    // status subcommand
    {"queue", "status", 0, 0, "Queue Status"},
    // status subcommand
    {"queue", "policy", 0, 0, "Queue Policies"},
    // status subcommand
    {"queue", "session", 0, 0, "Session Status"},
    
    /****** next level test command ******/
    {"status", "test", 0, 0, "test"},
    
    {NULL, NULL, 0, 0, NULL}  // end of array tag
};

opal_cmd_line_init_t cmd_line_opts[] = {
    { NULL,
      'h', NULL, "help",
      0,
      &orcm_ocli_globals.help, OPAL_CMD_LINE_TYPE_BOOL,
      "This help message" },

    { NULL,
      'v', NULL, "verbose",
      0,
      &orcm_ocli_globals.verbose, OPAL_CMD_LINE_TYPE_BOOL,
      "Be Verbose" },

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
    /* initialize, parse command line, and setup frameworks */
    orcm_ocli_init(argc, argv);

    if (ORTE_SUCCESS != orcm_finalize()) {
        fprintf(stderr, "Failed orcm_finalize\n");
        exit(1);
    }

    return ORCM_SUCCESS;
}

static int parse_args(int argc, char *argv[]) 
{
    int ret;
    opal_cmd_line_t cmd_line;
    orcm_ocli_globals_t tmp = { false,    /* help */
                                false,    /* verbose */
                                -1};      /* output */

    orcm_ocli_globals = tmp;
    char *args = NULL;
    char *str = NULL;
    orcm_cli_t cli;
    orcm_cli_cmd_t *cmd;
    char *mycmd;
    int tailc;
    char **tailv;

    /* Parse the command line options */
    opal_cmd_line_create(&cmd_line, cmd_line_opts);
    
    mca_base_cmd_line_setup(&cmd_line);
    ret = opal_cmd_line_parse(&cmd_line, true, argc, argv);
    
    if (OPAL_SUCCESS != ret) {
        if (OPAL_ERR_SILENT != ret) {
            fprintf(stderr, "%s: command line error (%s)\n", argv[0],
                    opal_strerror(ret));
        }
        return ret;
    }

    opal_cmd_line_get_tail(&cmd_line, &tailc, &tailv);
    
    if (0 == tailc) {
        /* if the user hasn't specified any commands, run cli to help build it */
        orcm_cli_create(&cli, cli_init);
        OPAL_LIST_FOREACH(cmd, &cli.cmds, orcm_cli_cmd_t) {
            orcm_cli_print_cmd(cmd, NULL);
        }
        
        orcm_cli_get_cmd("ocli", &cli, &mycmd);
        fprintf(stderr, "\nCMD: %s\n", mycmd);
    } else {
        /* otherwise use the user specified command */
        fprintf(stderr, "\nCMD: %s\n", opal_argv_join(tailv, ' '));
    }

    /**
     * Now start parsing our specific arguments
     */
    if (orcm_ocli_globals.help) {
        args = opal_cmd_line_get_usage_msg(&cmd_line);
        str = opal_show_help_string("help-ocli.txt", "usage", true,
                                    args);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        free(args);
        /* If we show the help message, that should be all we do */
        exit(0);
    }

    /*
     * Since this process can now handle MCA/GMCA parameters, make sure to
     * process them.
     */
    mca_base_cmd_line_process_args(&cmd_line, &environ, &environ);

    return ORCM_SUCCESS;
}

static int orcm_ocli_init(int argc, char *argv[])
{
    int ret;

    /*
     * Make sure to init util before parse_args
     * to ensure installdirs is setup properly
     * before calling mca_base_open();
     */
    if( OPAL_SUCCESS != (ret = opal_init_util(&argc, &argv)) ) {
        return ret;
    }

    /*
     * Parse Command Line Arguments
     */
    if (ORCM_SUCCESS != (ret = parse_args(argc, argv))) {
        return ret;
    }

    /*
     * Setup OPAL Output handle from the verbose argument
     */
    if( orcm_ocli_globals.verbose ) {
        orcm_ocli_globals.output = opal_output_open(NULL);
        opal_output_set_verbosity(orcm_ocli_globals.output, 10);
    } else {
        orcm_ocli_globals.output = 0; /* Default=STDERR */
    }

    ret = orcm_init(ORCM_TOOL);

    return ret;
}
