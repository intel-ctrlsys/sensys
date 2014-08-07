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

#include "orte/runtime/orte_wait.h"
#include "orte/util/error_strings.h"
#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"

#include "orcm/runtime/runtime.h"

#include "orcm/mca/diag/base/base.h"

#define ORCM_MAX_LINE_LENGTH  4096

/******************
 * Local Functions
 ******************/
static int orcm_odiag_init(int argc, char *argv[]);
static int parse_args(int argc, char *argv[]);

/*****************************************
 * Global Vars for Command line Arguments
 *****************************************/
typedef struct {
    bool help;
    bool verbose;
    bool mem_diag;
    bool cpu_diag;
    int  output;
} orcm_odiag_globals_t;

orcm_odiag_globals_t orcm_odiag_globals;

opal_cmd_line_init_t cmd_line_opts[] = {
    { NULL,
      'h', NULL, "help",
      0,
      &orcm_odiag_globals.help, OPAL_CMD_LINE_TYPE_BOOL,
      "This help message" },

    { NULL,
      'v', NULL, "verbose",
      0,
      &orcm_odiag_globals.verbose, OPAL_CMD_LINE_TYPE_BOOL,
      "Be Verbose" },
    
    { NULL,
      'm', NULL, "mem",
      0,
      &orcm_odiag_globals.mem_diag, OPAL_CMD_LINE_TYPE_BOOL,
      "Enable memory diagnostic check" },

    { NULL,
      'c', NULL, "cpu",
      0,
      &orcm_odiag_globals.cpu_diag, OPAL_CMD_LINE_TYPE_BOOL,
      "Enable CPU diagnostic check" },

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
    int rc;
    opal_list_t config;
    
    /* initialize, parse command line, and setup frameworks */
    orcm_odiag_init(argc, argv);
    
    if (orcm_odiag_globals.cpu_diag) {
        fprintf(stdout, "ORCM Diagnostic Checking cpu:                           [NOTRUN]\n");
    }

    if (orcm_odiag_globals.mem_diag) {
        putenv("OMPI_MCA_diag=memtest");        
        /* open/select the diag framework */
        if (ORTE_SUCCESS != (rc = mca_base_framework_open(&orcm_diag_base_framework, 0))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }

        if (ORTE_SUCCESS != (rc = orcm_diag_base_select())) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }

        OBJ_CONSTRUCT(&config, opal_list_t);
        fprintf(stdout, "ORCM Diagnostic Checking memory:                        ");
        rc = orcm_diag.diag_check(&config);
        
        if ( ORCM_SUCCESS == rc ) {
            fprintf(stdout, "[  OK  ]\n");
        } else if ( ORCM_ERR_COMPARE_FAILURE == rc ) {
            fprintf(stdout, "[ FAIL ]\n");
        } else {
            fprintf(stdout, "[NOTRUN]\n");
        }          
    }

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
    orcm_odiag_globals_t tmp = { false,    /* help */
                                 false,    /* verbose */
                                 false,     /* enable memory diagnostics */
                                 false,    /* enable CPU diagnostics */
                                 -1 };     /* output */

    orcm_odiag_globals = tmp;

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
    if (orcm_odiag_globals.help) {
        char *str, *args = NULL;
        args = opal_cmd_line_get_usage_msg(&cmd_line);
        str = opal_show_help_string("help-odiag.txt", "usage", true,
                                    args);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        free(args);
        /* If we show the help message, that should be all we do */
        exit(0);
    }
    
    if ( !orcm_odiag_globals.mem_diag && !orcm_odiag_globals.cpu_diag ) {
        char *str, *args = NULL;
        args = opal_cmd_line_get_usage_msg(&cmd_line);
        str = opal_show_help_string("help-odiag.txt", "file", true,
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

    return ORTE_SUCCESS;
}

static int orcm_odiag_init(int argc, char *argv[]) 
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
    if( orcm_odiag_globals.verbose ) {
        orcm_odiag_globals.output = opal_output_open(NULL);
        opal_output_set_verbosity(orcm_odiag_globals.output, 10);
    } else {
        orcm_odiag_globals.output = 0; /* Default=STDERR */
    }

    ret = orcm_init(ORCM_TOOL);

    return ret;
}


