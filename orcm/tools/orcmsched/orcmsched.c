/*
 * Copyright (c) 2009-2010 Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"

/* add the orcm definitions */
#include "orcm/constants.h"
#include "orcm/runtime/runtime.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
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


#include "opal/mca/event/event.h"
#include "opal/mca/base/base.h"
#include "opal/util/output.h"
#include "opal/util/cmd_line.h"
#include "opal/util/opal_environ.h"
#include "opal/util/os_path.h"
#include "opal/util/printf.h"
#include "opal/util/argv.h"
#include "opal/runtime/opal.h"
#include "opal/util/daemon_init.h"
#include "opal/dss/dss.h"

#include "orte/constants.h"
#include "orte/util/show_help.h"
#include "orte/util/proc_info.h"
#include "orte/util/session_dir.h"
#include "orte/util/name_fns.h"
#include "orte/util/nidmap.h"
#include "orte/runtime/orte_locks.h"
#include "orte/runtime/orte_quit.h"
#include "orte/mca/rml/base/rml_contact.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/odls/odls.h"
#include "orte/mca/odls/base/base.h"
#include "orte/mca/plm/plm.h"
#include "orte/mca/plm/base/plm_private.h"
#include "orte/mca/ras/ras.h"
#include "orte/mca/routed/routed.h"

#include "orcm/runtime/runtime.h"
/*
 * Globals
 */
static bool orcmsched_spin_flag=false;

static struct {
    bool help;
    bool daemonize;
    char *hnp_uri;
} my_globals;

/*
 * define the orcmsched context table for obtaining parameters
 */
static opal_cmd_line_init_t cmd_line_opts[] = {
    /* Various "obvious" options */
    { NULL, 'h', NULL, "help", 0,
      &my_globals.help, OPAL_CMD_LINE_TYPE_BOOL,
      "This help message" },

    { NULL, 's', NULL, "spin", 0,
      &orcmsched_spin_flag, OPAL_CMD_LINE_TYPE_BOOL,
      "Have the scheduler spin until we can connect a debugger to it" },

    { "orte_debug", 'd', NULL, "debug", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Debug the OpenRTE" },
        
    { NULL, '\0', NULL, "daemonize", 0,
      &my_globals.daemonize, OPAL_CMD_LINE_TYPE_BOOL,
      "Daemonize the scheduler into the background" },

    { "orcm_sched_kill_dvm", '\0', NULL, "kill-dvm", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Kill daemons in DVM upon termination [default: no]" },
    
    /* End of list */
    { NULL, '\0', NULL, NULL, 0,
      NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }
};

int main(int argc, char *argv[])
{
    int ret = 0;
    opal_cmd_line_t *cmd_line = NULL;
    int i;
    char *ctmp;
    time_t now;

    /* initialize the globals */
    memset(&my_globals, 0, sizeof(my_globals));
    
    /* setup to check common command line options that just report and die */
    cmd_line = OBJ_NEW(opal_cmd_line_t);
    opal_cmd_line_create(cmd_line, cmd_line_opts);
    mca_base_cmd_line_setup(cmd_line);
    if (ORTE_SUCCESS != (ret = opal_cmd_line_parse(cmd_line, false,
                                                   argc, argv))) {
        fprintf(stderr, "Cannot process cmd line - use --help for assistance\n");
        return ret;
    }

    /*
     * Since this process can now handle MCA/GMCA parameters, make sure to
     * process them.
     */
    mca_base_cmd_line_process_args(cmd_line, &environ, &environ);
    
    /* check for help request */
    if (my_globals.help) {
        char *args = NULL;
        args = opal_cmd_line_get_usage_msg(cmd_line);
        orte_show_help("help-orcmsched.txt", "usage", true, args);
        free(args);
        return 1;
    }

    /* see if they want us to spin until they can connect a debugger to us */
    i=0;
    while (orcmsched_spin_flag) {
        i++;
        if (1000 < i) i=0;        
    }
    
    opal_progress_set_event_flag(OPAL_EVLOOP_ONCE);

    /* init the ORCM library */
    if (ORCM_SUCCESS != (ret = orcm_init(ORCM_SCHED))) {
        fprintf(stderr, "Failed to init: error %d\n", ret);
        exit(1);
    }
    
    /* detach from controlling terminal
     * otherwise, remain attached so output can get to us
     */
    if (my_globals.daemonize) {
        opal_daemon_init(NULL);
    } else {
        /* set the local debug verbosity */
        orcm_debug_output = 5;
    }

    /* print out a message alerting that we are alive */
    now = time(NULL);
    /* convert the time to a simple string */
    ctmp = ctime(&now);
    /* strip the trailing newline */
    ctmp[strlen(ctmp)-1] = '\0';

    opal_output(0, "%s: ORCM SCHEDULER %s started",
                ctmp, ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    while (orte_event_base_active) {
        opal_event_loop(orte_event_base, OPAL_EVLOOP_ONCE);
    }

    /* print out a message alerting that we are shutting down */
    now = time(NULL);
    /* convert the time to a simple string */
    ctmp = ctime(&now);
    /* strip the trailing newline */
    ctmp[strlen(ctmp)-1] = '\0';

    opal_output(0, "%s: ORCM SCHEDULER %s shutting down with status %d (%s)",
                ctmp, ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                orte_exit_status,
                (0 < orte_exit_status) ? "SYS" : ORTE_ERROR_NAME(orte_exit_status));

    /* Finalize and clean up ourselves */
    orcm_finalize();
    return ret;
}
