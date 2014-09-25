/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm/tools/ocli/common.h"
#include "orcm/tools/ocli/ocli.h"

/******************
 * Local Functions
 ******************/
static int orcm_ocli_init(int argc, char *argv[]);
static int parse_args(int argc, char *argv[], char **result_cmd);
static void run_cmd(char *cmd);

/*****************************************
 * Global Vars for Command line Arguments
 *****************************************/
typedef struct {
    bool help;
    bool verbose;
    int output;
} orcm_ocli_globals_t;

orcm_ocli_globals_t orcm_ocli_globals;

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
    { NULL, '\0', NULL, NULL, 0, NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }
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

static int parse_args(int argc, char *argv[], char** result_cmd)
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

    /* get the commandline without mca params */
    opal_cmd_line_get_tail(&cmd_line, &tailc, &tailv);
    
    if (0 == tailc) {
        /* if the user hasn't specified any commands,
         * run interactive cli to help build it */
        OBJ_CONSTRUCT(&cli, orcm_cli_t);
        orcm_cli_create(&cli, cli_init);
        /* give help on top level commands */
        printf("*** WELCOME TO OCLI ***\n Possible commands:\n");
        OPAL_LIST_FOREACH(cmd, &cli.cmds, orcm_cli_cmd_t) {
            orcm_cli_print_cmd(cmd, NULL);
        }
        *result_cmd = NULL;
        /* run interactive cli */
        orcm_cli_get_cmd("ocli", &cli, result_cmd);
        if (!*result_cmd) {
            fprintf(stderr, "\nERR: NO COMMAND RETURNED\n");
        }
    } else {
        /* otherwise use the user specified command */
        *result_cmd = (opal_argv_join(tailv, ' '));
    }

    /* Now start parsing our specific arguments */
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

    /* Since this process can now handle MCA/GMCA parameters, make sure to
     * process them. */
    mca_base_cmd_line_process_args(&cmd_line, &environ, &environ);
    
    return ORCM_SUCCESS;
}

static int orcm_ocli_init(int argc, char *argv[])
{
    int ret;
    char *mycmd;

    /* Make sure to init util before parse_args
     * to ensure installdirs is setup properly
     * before calling mca_base_open(); */
    if( OPAL_SUCCESS != (ret = opal_init_util(&argc, &argv)) ) {
        return ret;
    }

    /* Parse Command Line Arguments */
    if (ORCM_SUCCESS != (ret = parse_args(argc, argv, &mycmd))) {
        return ret;
    }

    /* Setup OPAL Output handle from the verbose argument */
    if( orcm_ocli_globals.verbose ) {
        orcm_ocli_globals.output = opal_output_open(NULL);
        opal_output_set_verbosity(orcm_ocli_globals.output, 10);
    } else {
        orcm_ocli_globals.output = 0; /* Default=STDERR */
    }

    /* initialize orcm for use as a tool */
    ret = orcm_init(ORCM_TOOL);
    
    run_cmd(mycmd);
    free(mycmd);

    return ret;
}

static int ocli_command_to_int(char *command)
{
    /* this is used for nicer looking switch statements
     * just convert a string to the location of the string
     * in the pre-defined array found in ocli.h*/
    int i;
    
    if (!command) {
        return -1;
    }
    
    for (i = 0; i < opal_argv_count((char **)orcm_ocli_commands); i++) {
        if (0 == strcmp(command, orcm_ocli_commands[i])) {
            return i;
        }
    }
    return -1;
}

static void run_cmd(char *cmd) {
    char **cmdlist;
    char *fullcmd;
    int rc;
    
    cmdlist = opal_argv_split(cmd, ' ');
    if (0 == opal_argv_count(cmdlist)) {
        printf("No command parsed\n");
        return;
    }
    
    rc = ocli_command_to_int(cmdlist[0]);
    if (ORCM_ERROR == rc) {
        fullcmd = opal_argv_join(cmdlist, ' ');
        printf("Unknown command: %s\n", fullcmd);
        free(fullcmd);
        return;
    }
    
    /* call corresponding function to passed command */
    switch (rc) {
    case 0: //resource
        rc = ocli_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            fullcmd = opal_argv_join(cmdlist, ' ');
            printf("Unknown command: %s\n", fullcmd);
            free(fullcmd);
            break;
        }
            
        switch (rc) {
        case 3: //status
            if (ORCM_SUCCESS != (rc = orcm_ocli_resource_status(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 4: //availability
            if (ORCM_SUCCESS != (rc = orcm_ocli_resource_availability(cmdlist))) {
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
    case 1: // queue
        rc = ocli_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            fullcmd = opal_argv_join(cmdlist, ' ');
            printf("Unknown command: %s\n", fullcmd);
            free(fullcmd);
            break;
        }
            
        switch (rc) {
        case 3: //status
            if (ORCM_SUCCESS != (rc = orcm_ocli_queue_status(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 5: //policy
            if (ORCM_SUCCESS != (rc = orcm_ocli_queue_policy(cmdlist))) {
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
    case 2: // session
        rc = ocli_command_to_int(cmdlist[1]);
        if (-1 == rc) {
            fullcmd = opal_argv_join(cmdlist, ' ');
            printf("Unknown command: %s\n", fullcmd);
            free(fullcmd);
            break;
        }
            
        switch (rc) {
        case 3: //status
            if (ORCM_SUCCESS != (rc = orcm_ocli_session_status(cmdlist))) {
                ORTE_ERROR_LOG(rc);
            }
            break;
        case 6: //cancel
            if (ORCM_SUCCESS != (rc = orcm_ocli_session_cancel(cmdlist))) {
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
        printf("Illegal command: %s\n", fullcmd);
        free(fullcmd);
        break;
    }
    
    opal_argv_free(cmdlist);
}
