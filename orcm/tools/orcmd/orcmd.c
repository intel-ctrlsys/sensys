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
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
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
#include "opal/runtime/opal.h"

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

    { "logical_group_config_file", 'l', "config-file", "config-file", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Logical group configuration file for this orcm chain" },

    /* End of list */
    { NULL, '\0', NULL, NULL, 0,
      NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }
};

static void orcmd_recv(int status, orte_process_name_t* sender,
                       opal_buffer_t* buffer, orte_rml_tag_t tag,
                       void* cbdata);


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
    mca_base_cmd_line_setup(&cmd_line);
    if (ORCM_SUCCESS != (ret = opal_cmd_line_parse(&cmd_line, false, argc, argv))) {
        fprintf(stderr, "Command line error, use -h to get help.  Error: %d\n", ret);
        exit(1);
    }
    
    /* Initialize the argv parsing handle */
    if (orcm_globals.help || orcm_globals.version) {
        if (ORCM_SUCCESS != (ret=opal_init_util(&argc, &argv))) {
            goto cleanup;
        }
    }

    if (orcm_globals.verbose) {
        orcm_globals.output = opal_output_open(NULL);
        opal_output_set_verbosity(orcm_globals.output, 10);
    } else {
        orcm_globals.output = 0; /* Default=STDERR */
    }

    /*
     * Since this process can now handle MCA/GMCA parameters, make sure to
     * process them.
     */
    if (OPAL_SUCCESS != mca_base_cmd_line_process_args(&cmd_line, &environ, &environ)) {
        opal_finalize_util();
        exit(1);
    }


    if (orcm_globals.help) {
        args = opal_cmd_line_get_usage_msg(&cmd_line);
        str = opal_show_help_string("help-orcmd.txt", "usage", false, args);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        opal_finalize_util();
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
        opal_finalize_util();
        exit(0);
    }

    /***************
     * Initialize
     ***************/
    if (ORCM_SUCCESS != (ret = orcm_init(ORCM_DAEMON))) {
        goto cleanup;
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
        if ((ORTE_PROC_MY_PARENT->jobid == ORTE_PROC_MY_SCHEDULER->jobid) 
            && (ORTE_PROC_MY_PARENT->vpid == ORTE_PROC_MY_SCHEDULER->vpid)) {
            opal_output(0, "\n******************************\n%s: ORCM version: %s AGGREGATOR: %s started and connected to SCHEDULER: %s\n******************************\n",
                ctmp, ORCM_VERSION, ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                ORTE_NAME_PRINT(ORTE_PROC_MY_PARENT));

        } else {
            opal_output(0, "\n******************************\n%s: ORCM version: %s AGGREGATOR: %s started and connected to AGGREGATOR: %s\n******************************\n",
                ctmp, ORCM_VERSION, ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                ORTE_NAME_PRINT(ORTE_PROC_MY_PARENT));
        }
    } else {
        opal_output(0, "\n******************************\n%s: ORCM version: %s DAEMON: %s started and connected to AGGREGATOR: %s\n******************************\n",
                    ctmp,
                    ORCM_VERSION,
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                    ORTE_NAME_PRINT(ORTE_PROC_MY_DAEMON));

        /* inform the scheduler */
        buf = OBJ_NEW(opal_buffer_t);
        /* pack the alloc command flag */
        if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &command,1, ORCM_RM_CMD_T))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(buf);
            goto cleanup;
        }
        if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &state, 1, OPAL_INT8))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(buf);
            goto cleanup;
        }
        if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, ORTE_PROC_MY_NAME, 1, ORTE_NAME))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(buf);
            goto cleanup;
        }

        /* send hwloc topo to scheduler */
        if (NULL != opal_hwloc_topology) {
            have_hwloc_topology = true;
            if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &have_hwloc_topology, 1, OPAL_BOOL))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buf);
                goto cleanup;
            }
            if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &opal_hwloc_topology, 1, OPAL_HWLOC_TOPO))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buf);
                goto cleanup;
            }
        } else {
            have_hwloc_topology = false;
            if (OPAL_SUCCESS != (ret = opal_dss.pack(buf, &have_hwloc_topology, 1, OPAL_BOOL))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buf);
                goto cleanup;
            }
        }

        if (ORTE_SUCCESS != (ret = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                                                          ORCM_RML_TAG_RM,
                                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(buf);
            goto cleanup;
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

cleanup:
    OBJ_DESTRUCT(&cmd_line);
    return ret;
}

/* process incoming messages in order of receipt */
static void orcmd_recv(int status, orte_process_name_t* sender,
                       opal_buffer_t* buffer, orte_rml_tag_t tag,
                       void* cbdata)
{
    orcm_rm_cmd_flag_t command;
    int n, rc;

    n = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &command, &n, ORCM_RM_CMD_T))) {
        ORTE_ERROR_LOG (rc);
        return;
    }

    switch(command) {
    case ORCM_CALIBRATE_COMMAND:
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
