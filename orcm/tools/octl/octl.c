/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm/tools/octl/common.h"
#include "orcm/tools/octl/octl.h"

/******************
 * Local Functions
 ******************/
static int orcm_octl_init(int argc, char *argv[]);
static int run_cmd(char *cmd);

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
    int ret, rc;

    /* initialize, parse command line, and setup frameworks */
    ret = orcm_octl_init(argc, argv);

    if (ORTE_SUCCESS != (rc = orcm_finalize())) {
        fprintf(stderr, "Failed orcm_finalize\n");
        exit(rc);
    }

    return ret;
}

static int orcm_octl_init(int argc, char *argv[])
{
    opal_cmd_line_t cmd_line;
    orcm_octl_globals_t tmp = { false,    /* help */
        false,    /* version */
        false,    /* verbose */
        -1};      /* output */
    
    orcm_octl_globals = tmp;
    char *args = NULL;
    char *str = NULL;
    orcm_cli_t cli;
    orcm_cli_cmd_t *cmd;
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
        OBJ_CONSTRUCT(&cli, orcm_cli_t);
        orcm_cli_create(&cli, cli_init);
        /* give help on top level commands */
        printf("*** WELCOME TO OCTL ***\n Possible commands:\n");
        OPAL_LIST_FOREACH(cmd, &cli.cmds, orcm_cli_cmd_t) {
            orcm_cli_print_cmd(cmd, NULL);
        }
        mycmd = NULL;
        /* run interactive cli */
        orcm_cli_get_cmd("octl", &cli, &mycmd);
        if (!mycmd) {
            fprintf(stderr, "\nNo command specified\n");
        }
    } else {
        /* otherwise use the user specified command */
        mycmd = (opal_argv_join(tailv, ' '));
    }
    
    ret = run_cmd(mycmd);
    free(mycmd);
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

static int run_cmd(char *cmd) {
    char **cmdlist = NULL;
    char *fullcmd = NULL;
    int rc;
    
    cmdlist = opal_argv_split(cmd, ' ');
    if (0 == opal_argv_count(cmdlist)) {
        fprintf(stderr, "No command parsed\n");
        opal_argv_free(cmdlist);
        return ORCM_ERROR;
    }
    
    rc = octl_command_to_int(cmdlist[0]);
    if (-1 == rc) {
        fullcmd = opal_argv_join(cmdlist, ' ');
        fprintf(stderr, "Unknown command: %s\n", fullcmd);
        free(fullcmd);
        opal_argv_free(cmdlist);
        return ORCM_ERROR;
    }
    
    /* call corresponding function to passed command */
    switch (rc) {
    case 0: //resource
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            fullcmd = opal_argv_join(cmdlist, ' ');
            fprintf(stderr, "Unknown command: %s\n", fullcmd);
            free(fullcmd);
            rc = ORCM_ERROR;
            break;
        }
            
        switch (rc) {
        case 4: //status
            if (ORCM_SUCCESS != (rc = orcm_octl_resource_status(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 5: //add
            if (ORCM_SUCCESS != (rc = orcm_octl_resource_add(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 6: //remove
            if (ORCM_SUCCESS != (rc = orcm_octl_resource_remove(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 7: //drain
            if (ORCM_SUCCESS != (rc = orcm_octl_resource_drain(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 18: //resume
            if (ORCM_SUCCESS != (rc = orcm_octl_resource_resume(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        default:
            fullcmd = opal_argv_join(cmdlist, ' ');
            fprintf(stderr, "Illegal command: %s\n", fullcmd);
            free(fullcmd);
            break;
        }
        break;
    case 1: // queue
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            fullcmd = opal_argv_join(cmdlist, ' ');
            fprintf(stderr, "Unknown command: %s\n", fullcmd);
            free(fullcmd);
            rc = ORCM_ERROR;
            break;
        }
            
        switch (rc) {
        case 4: //status
            if (ORCM_SUCCESS != (rc = orcm_octl_queue_status(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 8: //policy
            if (ORCM_SUCCESS != (rc = orcm_octl_queue_policy(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 9: //define
            if (ORCM_SUCCESS != (rc = orcm_octl_queue_define(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 5: //add
            if (ORCM_SUCCESS != (rc = orcm_octl_queue_add(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 6: //remove
            if (ORCM_SUCCESS != (rc = orcm_octl_queue_remove(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 10: //acl
            if (ORCM_SUCCESS != (rc = orcm_octl_queue_acl(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 11: //priority
            if (ORCM_SUCCESS != (rc = orcm_octl_queue_priority(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        default:
            fullcmd = opal_argv_join(cmdlist, ' ');
            fprintf(stderr, "Illegal command: %s\n", fullcmd);
            free(fullcmd);
            break;
        }
        break;
    case 2: // session
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            fullcmd = opal_argv_join(cmdlist, ' ');
            fprintf(stderr, "Unknown command: %s\n", fullcmd);
            free(fullcmd);
            rc = ORCM_ERROR;
            break;
        }
            
        switch (rc) {
        case 4: //status
            if (ORCM_SUCCESS != (rc = orcm_octl_session_status(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 12: //cancel
            if (ORCM_SUCCESS != (rc = orcm_octl_session_cancel(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 16: //set
            rc = octl_command_to_int(cmdlist[2]);
            if (-1 == rc) {
                fullcmd = opal_argv_join(cmdlist, ' ');
                printf("Unknown command: %s\n", fullcmd);
                free(fullcmd);
                break;
            }

            switch(rc) {
            case 20: //budget
                if (ORCM_SUCCESS != (rc = orcm_octl_session_set(ORCM_SET_POWER_BUDGET_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 21: //mode
                if (ORCM_SUCCESS != (rc = orcm_octl_session_set(ORCM_SET_POWER_MODE_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 22: //window
                if (ORCM_SUCCESS != (rc = orcm_octl_session_set(ORCM_SET_POWER_WINDOW_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 23: //overage
                if (ORCM_SUCCESS != (rc = orcm_octl_session_set(ORCM_SET_POWER_OVERAGE_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 24: //underage
                if (ORCM_SUCCESS != (rc = orcm_octl_session_set(ORCM_SET_POWER_UNDERAGE_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 25: //overage_time
                if (ORCM_SUCCESS != (rc = orcm_octl_session_set(ORCM_SET_POWER_OVERAGE_TIME_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 26: //underage_time
                if (ORCM_SUCCESS != (rc = orcm_octl_session_set(ORCM_SET_POWER_UNDERAGE_TIME_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 27: //frequency
                if (ORCM_SUCCESS != (rc = orcm_octl_session_set(ORCM_SET_POWER_FREQUENCY_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 29: //strict
                if (ORCM_SUCCESS != (rc = orcm_octl_session_set(ORCM_SET_POWER_STRICT_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            default:
                fullcmd = opal_argv_join(cmdlist, ' ');
                printf("Illegal command: %s\n", fullcmd);
                free(fullcmd);
                break;
            }
            break;
        case 17: //get
            rc = octl_command_to_int(cmdlist[2]);
            if (-1 == rc) {
                fullcmd = opal_argv_join(cmdlist, ' ');
                printf("Unknown command: %s\n", fullcmd);
                free(fullcmd);
                break;
            }

            switch(rc) {
            case 20: //budget
                if (ORCM_SUCCESS != (rc = orcm_octl_session_get(ORCM_GET_POWER_BUDGET_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 21: //mode
                if (ORCM_SUCCESS != (rc = orcm_octl_session_get(ORCM_GET_POWER_MODE_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 22: //window
                if (ORCM_SUCCESS != (rc = orcm_octl_session_get(ORCM_GET_POWER_WINDOW_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 23: //overage
                if (ORCM_SUCCESS != (rc = orcm_octl_session_get(ORCM_GET_POWER_OVERAGE_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 24: //underage
                if (ORCM_SUCCESS != (rc = orcm_octl_session_get(ORCM_GET_POWER_UNDERAGE_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 25: //overage_time
                if (ORCM_SUCCESS != (rc = orcm_octl_session_get(ORCM_GET_POWER_OVERAGE_TIME_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 26: //underage_time
                if (ORCM_SUCCESS != (rc = orcm_octl_session_get(ORCM_GET_POWER_UNDERAGE_TIME_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 27: //frequency
                if (ORCM_SUCCESS != (rc = orcm_octl_session_get(ORCM_GET_POWER_FREQUENCY_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 28: //modes
                if (ORCM_SUCCESS != (rc = orcm_octl_power_get(ORCM_GET_POWER_MODES_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 29: //strict
                if (ORCM_SUCCESS != (rc = orcm_octl_session_get(ORCM_GET_POWER_STRICT_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            default:
                fullcmd = opal_argv_join(cmdlist, ' ');
                printf("Illegal command: %s\n", fullcmd);
                free(fullcmd);
                break;
            }
        break;
        default:
            fullcmd = opal_argv_join(cmdlist, ' ');
            fprintf(stderr, "Illegal command: %s\n", fullcmd);
            free(fullcmd);
            break;
        }
        break;
    case 3: // diag
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            fullcmd = opal_argv_join(cmdlist, ' ');
            fprintf(stderr, "Unknown command: %s\n", fullcmd);
            free(fullcmd);
            rc = ORCM_ERROR;
            break;
        }
        
        switch (rc) {
            case 13: //cpu
                if (ORCM_SUCCESS != (rc = orcm_octl_diag_cpu(cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 19: //eth
                if (ORCM_SUCCESS != (rc = orcm_octl_diag_eth(cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 14: //mem
                if (ORCM_SUCCESS != (rc = orcm_octl_diag_mem(cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            default:
                fullcmd = opal_argv_join(cmdlist, ' ');
                fprintf(stderr, "Illegal command: %s\n", fullcmd);
                free(fullcmd);
                break;
        }
        break;
    case 15: // power
        rc = octl_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            fullcmd = opal_argv_join(cmdlist, ' ');
            fprintf(stderr, "Unknown command: %s\n", fullcmd);
            free(fullcmd);
            rc = ORCM_ERROR;
            break;
        }
        
        switch (rc) {
        case 16: //set
            rc = octl_command_to_int(cmdlist[2]);
            if (-1 == rc) {
                fullcmd = opal_argv_join(cmdlist, ' ');
                printf("Unknown command: %s\n", fullcmd);
                free(fullcmd);
                break;
            }

            switch(rc) {
            case 20: //budget
                if (ORCM_SUCCESS != (rc = orcm_octl_power_set(ORCM_SET_POWER_BUDGET_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 21: //mode
                if (ORCM_SUCCESS != (rc = orcm_octl_power_set(ORCM_SET_POWER_MODE_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 22: //window
                if (ORCM_SUCCESS != (rc = orcm_octl_power_set(ORCM_SET_POWER_WINDOW_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 23: //overage
                if (ORCM_SUCCESS != (rc = orcm_octl_power_set(ORCM_SET_POWER_OVERAGE_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 24: //underage
                if (ORCM_SUCCESS != (rc = orcm_octl_power_set(ORCM_SET_POWER_UNDERAGE_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 25: //overage_time
                if (ORCM_SUCCESS != (rc = orcm_octl_power_set(ORCM_SET_POWER_OVERAGE_TIME_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 26: //underage_time
                if (ORCM_SUCCESS != (rc = orcm_octl_power_set(ORCM_SET_POWER_UNDERAGE_TIME_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 27: //frequency
                if (ORCM_SUCCESS != (rc = orcm_octl_power_set(ORCM_SET_POWER_FREQUENCY_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 29: //strict
                if (ORCM_SUCCESS != (rc = orcm_octl_power_set(ORCM_SET_POWER_STRICT_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            default:
                fullcmd = opal_argv_join(cmdlist, ' ');
                fprintf(stderr, "Illegal command: %s\n", fullcmd);
                free(fullcmd);
                break;
            }
            break;
        case 17: //get
            rc = octl_command_to_int(cmdlist[2]);
            if (-1 == rc) {
                fullcmd = opal_argv_join(cmdlist, ' ');
                printf("Unknown command: %s\n", fullcmd);
                free(fullcmd);
                break;
            }

            switch(rc) {
            case 20: //budget
                if (ORCM_SUCCESS != (rc = orcm_octl_power_get(ORCM_GET_POWER_BUDGET_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 21: //mode
                if (ORCM_SUCCESS != (rc = orcm_octl_power_get(ORCM_GET_POWER_MODE_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 22: //window
                if (ORCM_SUCCESS != (rc = orcm_octl_power_get(ORCM_GET_POWER_WINDOW_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 23: //overage
                if (ORCM_SUCCESS != (rc = orcm_octl_power_get(ORCM_GET_POWER_OVERAGE_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 24: //underage
                if (ORCM_SUCCESS != (rc = orcm_octl_power_get(ORCM_GET_POWER_UNDERAGE_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 25: //overage_time
                if (ORCM_SUCCESS != (rc = orcm_octl_power_get(ORCM_GET_POWER_OVERAGE_TIME_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 26: //underage_time
                if (ORCM_SUCCESS != (rc = orcm_octl_power_get(ORCM_GET_POWER_UNDERAGE_TIME_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 27: //frequency
                if (ORCM_SUCCESS != (rc = orcm_octl_power_get(ORCM_GET_POWER_FREQUENCY_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 28: //modes
                if (ORCM_SUCCESS != (rc = orcm_octl_power_get(ORCM_GET_POWER_MODES_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            case 29: //strict
                if (ORCM_SUCCESS != (rc = orcm_octl_power_get(ORCM_GET_POWER_STRICT_COMMAND, cmdlist))) {
                    ORTE_ERROR_LOG(rc);
                }
                break;
            default:
                fullcmd = opal_argv_join(cmdlist, ' ');
                printf("Illegal command: %s\n", fullcmd);
                free(fullcmd);
                break;
            }
            break;
        }
        break;
    default:
        fullcmd = opal_argv_join(cmdlist, ' ');
        fprintf(stderr, "Illegal command: %s\n", fullcmd);
        free(fullcmd);
        rc = ORCM_ERROR;
        break;
    }
    
    opal_argv_free(cmdlist);
    return rc;
}
