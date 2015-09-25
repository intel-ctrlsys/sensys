/*
 * Copyright (c) 2014-2015 Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/tools/octl/common.h"
#include "orcm/tools/octl/octl.h"
#include "orcm/util/logical_group.h"

/***
Remove 'implicit' warnings...
****/
int orcm_octl_sensor_inventory_get(int command, char** argv);

/******************
 * Local Functions
 ******************/
static int orcm_octl_work(int argc, char *argv[]);
static int run_cmd(char *cmd);
static int octl_facility_cleanup(void);
static void octl_print_illegal_command(char *cmd);
static void octl_print_error(int rc);

/*****************************************
 * Global Vars for Command line Arguments
 *****************************************/
typedef struct {
    bool help;
    bool version;
    bool verbose;
    int output;
} orcm_octl_globals_t;

orcm_octl_globals_t orcm_octl_globals;

opal_cmd_line_init_t cmd_line_opts[] = {
    { NULL,
      'h', NULL, "help",
      0,
      &orcm_octl_globals.help, OPAL_CMD_LINE_TYPE_BOOL,
      "This help message" },

    { NULL,
      'v', NULL, "verbose",
      0,
      &orcm_octl_globals.verbose, OPAL_CMD_LINE_TYPE_BOOL,
      "Be Verbose" },

    { NULL,
        'V', NULL, "version",
        0,
        &orcm_octl_globals.version, OPAL_CMD_LINE_TYPE_BOOL,
        "Show version information" },

    /* End of list */
    { NULL, '\0', NULL, NULL, 0, NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }
};

int
main(int argc, char *argv[])
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        /* initialize, parse command line, and setup frameworks */
        erri = orcm_octl_work(argc, argv);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (ORCM_SUCCESS != (erri = orcm_finalize())) {
            fprintf(stderr, "Failed orcm_finalize\n");
            break;
        }

        if (ORCM_SUCCESS != (erri = octl_facility_cleanup())) {
            fprintf(stderr, "Failed octl facility cleanup\n");
            break;
        }

        break;
    }

    return erri;
}

static int orcm_octl_work(int argc, char *argv[])
{
    opal_cmd_line_t cmd_line;
    bool interactive = false;
    orcm_octl_globals_t tmp = { false,    /* help */
        false,    /* version */
        false,    /* verbose */
        -1};      /* output */

    orcm_octl_globals = tmp;
    char *args = NULL;
    char *str = NULL;
    orcm_cli_t cli;
    int tailc;
    char **tailv = NULL;
    int ret;
    char *mycmd;

    /* Make sure to init util before parse_args
     * to ensure installdirs is setup properly
     * before calling mca_base_open(); */
    if( OPAL_SUCCESS != (ret = opal_init_util(&argc, &argv)) ) {
        return ret;
    }

    /* Parse the command line options */
    opal_cmd_line_create(&cmd_line, cmd_line_opts);
    mca_base_cmd_line_setup(&cmd_line);
    opal_cmd_line_parse(&cmd_line, true, argc, argv);

    /* Setup OPAL Output handle from the verbose argument */
    if( orcm_octl_globals.verbose ) {
        orcm_octl_globals.output = opal_output_open(NULL);
        opal_output_set_verbosity(orcm_octl_globals.output, 10);
    } else {
        orcm_octl_globals.output = 0; /* Default=STDERR */
    }

    if (orcm_octl_globals.help) {
        args = opal_cmd_line_get_usage_msg(&cmd_line);
        str = opal_show_help_string("help-octl.txt", "usage", false, args);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        free(args);
        /* If we show the help message, that should be all we do */
        exit(0);
    }

    if (orcm_octl_globals.version) {
        str = opal_show_help_string("help-octl.txt", "version", false,
                                    ORCM_VERSION,
                                    PACKAGE_BUGREPORT);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        exit(0);
    }

    /* Since this process can now handle MCA/GMCA parameters, make sure to
     * process them. */
    if (OPAL_SUCCESS != (ret = mca_base_cmd_line_process_args(&cmd_line, &environ, &environ))) {
        exit(ret);
    }

    /* get the commandline without mca params */
    opal_cmd_line_get_tail(&cmd_line, &tailc, &tailv);

    /* initialize orcm for use as a tool */
    if (ORCM_SUCCESS != (ret = orcm_init(ORCM_TOOL))) {
        fprintf(stderr, "Failed to initialize\n");
        exit(ret);
    }

    if (0 == tailc) {
        /* if the user hasn't specified any commands,
         * run interactive cli to help build it */
        interactive = true;
        OBJ_CONSTRUCT(&cli, orcm_cli_t);
        orcm_cli_create(&cli, cli_init);
        /* give help on top level commands */
        printf("*** WELCOME TO OCTL ***\n Possible commands:\n");
        orcm_cli_print_cmd_help(&cli, NULL);

        while (true == interactive) {
            mycmd = NULL;
            /* run interactive cli */
            ret = orcm_cli_get_cmd("octl", &cli, &mycmd);
            if (ORCM_SUCCESS != ret) {
                octl_print_illegal_command(mycmd);
                if (NULL != mycmd) {
                    free(mycmd);
                }
                continue;
            }
            if (!mycmd) {
                fprintf(stderr, "\nNo command specified\n");
                continue;
            }
            if ((0 == strcmp(mycmd,"quit")) ||
               (0 == strcmp(mycmd,"exit"))) {
                printf("\nExiting...\n");
                interactive = false;
                free(mycmd);
                continue;
            }
            ret = run_cmd(mycmd);
            if (ORCM_ERROR == ret) {
                printf("\nUse \'%s<tab>\' or \'%s<?>\' for help.\n", mycmd, mycmd);
            }
            free(mycmd);
        }
        OBJ_DESTRUCT(&cli);
    } else {
        /* otherwise use the user specified command */
        mycmd = (opal_argv_join(tailv, ' '));
        ret = run_cmd(mycmd);
        free(mycmd);
    }
    opal_argv_free(tailv);
    return ret;
}

static int octl_command_to_int(char *command)
{
    /* this is used for nicer looking switch statements
     * just convert a string to the location of the string
     * in the pre-defined array found in octl.h*/
    int i;

    if (!command) {
        return -1;
    }

    for (i = 0; i < opal_argv_count((char **)orcm_octl_commands); i++) {
        if (0 == strcmp(command, orcm_octl_commands[i])) {
            return i;
        }
    }
    return -1;
}

static int run_cmd(char *cmd)
{
    char **cmdlist = NULL;
    int rc;
    int sz_cmdlist = 0;

    cmdlist = opal_argv_split(cmd, ' ');
    sz_cmdlist = opal_argv_count(cmdlist);
    if (0 == sz_cmdlist) {
        fprintf(stderr, "No command parsed\n");
        opal_argv_free(cmdlist);
        return ORCM_ERROR;
    }

    rc = octl_command_to_int(cmdlist[0]);
    if (-1 == rc) {
        octl_print_illegal_command(cmd);
        opal_argv_free(cmdlist);
        return ORCM_ERROR;
    }

    /* call corresponding function to passed command */
    switch (rc) {
    case 0: //resource
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            rc = ORCM_ERROR;
            break;
        }

        switch (rc) {
        case 4: //status
            rc = orcm_octl_resource_status(cmdlist);
            break;
        case 5: //add
            rc = orcm_octl_resource_add(cmdlist);
            break;
        case 6: //remove
            rc = orcm_octl_resource_remove(cmdlist);
            break;
        case 7: //drain
            rc = orcm_octl_resource_drain(cmdlist);
            break;
        case 18: //resume
            rc = orcm_octl_resource_resume(cmdlist);
            break;
        default:
            rc = ORCM_ERROR;
            break;
        }
        break;
    case 1: // queue
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            rc = ORCM_ERROR;
            break;
        }

        switch (rc) {
        case 4: //status
            rc = orcm_octl_queue_status(cmdlist);
            break;
        case 8: //policy
            rc = orcm_octl_queue_policy(cmdlist);
            break;
        case 9: //define
            rc = orcm_octl_queue_define(cmdlist);
            break;
        case 5: //add
            rc = orcm_octl_queue_add(cmdlist);
            break;
        case 6: //remove
            rc = orcm_octl_queue_remove(cmdlist);
            break;
        case 10: //acl
            rc = orcm_octl_queue_acl(cmdlist);
            break;
        case 11: //priority
            rc = orcm_octl_queue_priority(cmdlist);
            break;
        default:
            rc = ORCM_ERROR;
            break;
        }
        break;
    case 2: // session
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            rc = ORCM_ERROR;
            break;
        }

        switch (rc) {
        case 4: //status
            rc = orcm_octl_session_status(cmdlist);
            break;
        case 12: //cancel
            rc = orcm_octl_session_cancel(cmdlist);
            break;
        case 16: //set
            rc = octl_command_to_int(cmdlist[2]);
            if (-1 == rc) {
                break;
            }

            switch(rc) {
            case 20: //budget
                rc = orcm_octl_session_set(ORCM_SET_POWER_BUDGET_COMMAND, cmdlist);
                break;
            case 21: //mode
                rc = orcm_octl_session_set(ORCM_SET_POWER_MODE_COMMAND, cmdlist);
                break;
            case 22: //window
                rc = orcm_octl_session_set(ORCM_SET_POWER_WINDOW_COMMAND, cmdlist);
                break;
            case 23: //overage
                rc = orcm_octl_session_set(ORCM_SET_POWER_OVERAGE_COMMAND, cmdlist);
                break;
            case 24: //underage
                rc = orcm_octl_session_set(ORCM_SET_POWER_UNDERAGE_COMMAND, cmdlist);
                break;
            case 25: //overage_time
                rc = orcm_octl_session_set(ORCM_SET_POWER_OVERAGE_TIME_COMMAND, cmdlist);
                break;
            case 26: //underage_time
                rc = orcm_octl_session_set(ORCM_SET_POWER_UNDERAGE_TIME_COMMAND, cmdlist);
                break;
            case 27: //frequency
                rc = orcm_octl_session_set(ORCM_SET_POWER_FREQUENCY_COMMAND, cmdlist);
                break;
            case 29: //strict
                rc = orcm_octl_session_set(ORCM_SET_POWER_STRICT_COMMAND, cmdlist);
                break;
            default:
                rc = ORCM_ERROR;
                break;
            }
            break;
        case 17: //get
            rc = octl_command_to_int(cmdlist[2]);
            if (-1 == rc) {
                break;
            }

            switch(rc) {
            case 20: //budget
                rc = orcm_octl_session_get(ORCM_GET_POWER_BUDGET_COMMAND, cmdlist);
                break;
            case 21: //mode
                rc = orcm_octl_session_get(ORCM_GET_POWER_MODE_COMMAND, cmdlist);
                break;
            case 22: //window
                rc = orcm_octl_session_get(ORCM_GET_POWER_WINDOW_COMMAND, cmdlist);
                break;
            case 23: //overage
                rc = orcm_octl_session_get(ORCM_GET_POWER_OVERAGE_COMMAND, cmdlist);
                break;
            case 24: //underage
                rc = orcm_octl_session_get(ORCM_GET_POWER_UNDERAGE_COMMAND, cmdlist);
                break;
            case 25: //overage_time
                rc = orcm_octl_session_get(ORCM_GET_POWER_OVERAGE_TIME_COMMAND, cmdlist);
                break;
            case 26: //underage_time
                rc = orcm_octl_session_get(ORCM_GET_POWER_UNDERAGE_TIME_COMMAND, cmdlist);
                break;
            case 27: //frequency
                rc = orcm_octl_session_get(ORCM_GET_POWER_FREQUENCY_COMMAND, cmdlist);
                break;
            case 28: //modes
                rc = orcm_octl_power_get(ORCM_GET_POWER_MODES_COMMAND, cmdlist);
                break;
            case 29: //strict
                rc = orcm_octl_session_get(ORCM_GET_POWER_STRICT_COMMAND, cmdlist);
                break;
            default:
                rc = ORCM_ERROR;
                break;
            }
        break;
        default:
            rc = ORCM_ERROR;
            break;
        }
        break;
    case 3: // diag
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            rc = ORCM_ERROR;
            break;
        }

        switch (rc) {
            case 13: //cpu
                rc = orcm_octl_diag_cpu(cmdlist);
                break;
            case 19: //eth
                rc = orcm_octl_diag_eth(cmdlist);
                break;
            case 14: //mem
                rc = orcm_octl_diag_mem(cmdlist);
                break;
            default:
                rc = ORCM_ERROR;
                break;
        }
        break;
    case 15: // power
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            rc = ORCM_ERROR;
            break;
        }

        switch (rc) {
        case 16: //set
            rc = octl_command_to_int(cmdlist[2]);
            if (-1 == rc) {
                break;
            }

            switch(rc) {
            case 20: //budget
                rc = orcm_octl_power_set(ORCM_SET_POWER_BUDGET_COMMAND, cmdlist);
                break;
            case 21: //mode
                rc = orcm_octl_power_set(ORCM_SET_POWER_MODE_COMMAND, cmdlist);
                break;
            case 22: //window
                rc = orcm_octl_power_set(ORCM_SET_POWER_WINDOW_COMMAND, cmdlist);
                break;
            case 23: //overage
                rc = orcm_octl_power_set(ORCM_SET_POWER_OVERAGE_COMMAND, cmdlist);
                break;
            case 24: //underage
                rc = orcm_octl_power_set(ORCM_SET_POWER_UNDERAGE_COMMAND, cmdlist);
                break;
            case 25: //overage_time
                rc = orcm_octl_power_set(ORCM_SET_POWER_OVERAGE_TIME_COMMAND, cmdlist);
                break;
            case 26: //underage_time
                rc = orcm_octl_power_set(ORCM_SET_POWER_UNDERAGE_TIME_COMMAND, cmdlist);
                break;
            case 27: //frequency
                rc = orcm_octl_power_set(ORCM_SET_POWER_FREQUENCY_COMMAND, cmdlist);
                break;
            case 29: //strict
                rc = orcm_octl_power_set(ORCM_SET_POWER_STRICT_COMMAND, cmdlist);
                break;
            default:
                rc = ORCM_ERROR;
                break;
            }
            break;
        case 17: //get
            rc = octl_command_to_int(cmdlist[2]);
            if (-1 == rc) {
                break;
            }

            switch(rc) {
            case 20: //budget
                rc = orcm_octl_power_get(ORCM_GET_POWER_BUDGET_COMMAND, cmdlist);
                break;
            case 21: //mode
                rc = orcm_octl_power_get(ORCM_GET_POWER_MODE_COMMAND, cmdlist);
                break;
            case 22: //window
                rc = orcm_octl_power_get(ORCM_GET_POWER_WINDOW_COMMAND, cmdlist);
                break;
            case 23: //overage
                rc = orcm_octl_power_get(ORCM_GET_POWER_OVERAGE_COMMAND, cmdlist);
                break;
            case 24: //underage
                rc = orcm_octl_power_get(ORCM_GET_POWER_UNDERAGE_COMMAND, cmdlist);
                break;
            case 25: //overage_time
                rc = orcm_octl_power_get(ORCM_GET_POWER_OVERAGE_TIME_COMMAND, cmdlist);
                break;
            case 26: //underage_time
                rc = orcm_octl_power_get(ORCM_GET_POWER_UNDERAGE_TIME_COMMAND, cmdlist);
                break;
            case 27: //frequency
                rc = orcm_octl_power_get(ORCM_GET_POWER_FREQUENCY_COMMAND, cmdlist);
                break;
            case 28: //modes
                rc = orcm_octl_power_get(ORCM_GET_POWER_MODES_COMMAND, cmdlist);
                break;
            case 29: //strict
                rc = orcm_octl_power_get(ORCM_GET_POWER_STRICT_COMMAND, cmdlist);
                break;
            default:
                rc = ORCM_ERROR;
                break;
            }
            break;
        }
        break;
    case 30: // sensor
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            rc = ORCM_ERROR;
            break;
        }

        switch (rc) {
            case 16: //set
                rc = octl_command_to_int(cmdlist[2]);
                if (-1 == rc) {
                    break;
                }

                switch(rc) {
                case 8: //policy
                    rc = orcm_octl_sensor_policy_set(ORCM_SET_SENSOR_POLICY_COMMAND, cmdlist);
                    break;
                case 31: //sample-rate
                    rc = orcm_octl_sensor_sample_rate_set(ORCM_SET_SENSOR_SAMPLE_RATE_COMMAND, cmdlist);
                    break;
                default:
                    rc = ORCM_ERROR;
                    break;
                }
                break;

            case 17: //get
                rc = octl_command_to_int(cmdlist[2]);
                if (-1 == rc) {
                    break;
                }

                switch(rc) {
                case 8: //policy
                    rc = orcm_octl_sensor_policy_get(ORCM_GET_SENSOR_POLICY_COMMAND, cmdlist);
                    break;
                case 31: //sample-rate
                    rc = orcm_octl_sensor_sample_rate_get(ORCM_GET_SENSOR_SAMPLE_RATE_COMMAND, cmdlist);
                    break;
                case 37: //inventory
                    rc = orcm_octl_sensor_inventory_get(ORCM_GET_DB_SENSOR_INVENTORY_COMMAND, cmdlist);
                    break;
                default:
                    rc = ORCM_ERROR;
                    break;
                }
                break;

            default:
                rc = ORCM_ERROR;
                break;
        }
        break;
    case 32: //grouping
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            rc = ORCM_ERROR;
            break;
        }
        switch (rc)
        {
        case 5: //add
            rc = orcm_octl_logical_group_add(sz_cmdlist, cmdlist);
            break;
        case 6: //remove
            rc = orcm_octl_logical_group_remove(sz_cmdlist, cmdlist);
            break;
        case 33: //list
            rc = orcm_octl_logical_group_list(sz_cmdlist, cmdlist);
            break;
        default:
            rc = ORCM_ERROR;
            break;
        }
        break;
    case 35: //Analytics
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            rc = ORCM_ERROR;
            break;
        }
        switch (rc)
        {
        case 36: //workflow
            rc = octl_command_to_int(cmdlist[2]);
            if (-1 == rc) {
                rc = ORCM_ERROR;
                break;
            }
            switch(rc)
            {
            case 5://add
                rc = orcm_octl_analytics_workflow_add(cmdlist[3]);
                break;
            case 6://remove
                rc = orcm_octl_analytics_workflow_remove(cmdlist);
                break;
            case 17://get
                rc = orcm_octl_analytics_workflow_list(cmdlist);
                break;
            default:
                rc = ORCM_ERROR;
                break;
            }
            break;

        default:
            rc = ORCM_ERROR;
            break;
        }
        break;

    default:
        rc = ORCM_ERROR;
        break;
    }

    if (ORCM_ERROR == rc) {
        octl_print_illegal_command(cmd);
    } else if (ORCM_SUCCESS != rc) {
        octl_print_error(rc);
    }
    opal_argv_free(cmdlist);
    return rc;
}

static int octl_facility_cleanup(void)
{
    int erri = orcm_logical_group_finalize();
    return erri;
}

static void octl_print_illegal_command(char *cmd)
{
    if (NULL != cmd) {
        fprintf(stderr, "\nERROR: Illegal command: %s\n", cmd);
    }
    return;
}

static void octl_print_error(int rc)
{
    if (ORCM_SUCCESS != rc) {
        const char *errmsg = ORTE_ERROR_NAME(rc);
        if (NULL != errmsg) {
            fprintf(stderr, "\nERROR: %s\n", errmsg);
        } else {
            fprintf(stderr, "\nERROR: Internal\n");
        }
    }
}
