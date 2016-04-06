/*
 * Copyright (c) 2014-2016 Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/tools/octl/common.h"
#include "orcm/tools/octl/octl.h"
#include "orcm/util/logical_group.h"

/******************
 * Local Functions
 ******************/
static int orcm_octl_work(int argc, char *argv[]);
static int run_cmd(int argc, char *cmdlist[]);
static void octl_print_illegal_command(char *cmd);
static void octl_print_error(int rc);
static void octl_print_error_illegal_command(char **cmdlist);
static int octl_interactive_cli(void);
static void octl_init_cli(orcm_cli_t *cli);
static int run_cmd_from_cli(char *cmd);

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

    { "logical_group_config_file", 'l', "config-file", "config-file", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Logical group configuration file for this orcm chain" },

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

        break;
    }

    return erri;
}

static int orcm_octl_work(int argc, char *argv[])
{
    opal_cmd_line_t cmd_line;
    orcm_octl_globals_t tmp = { false,    /* help */
        false,    /* version */
        false,    /* verbose */
        -1};      /* output */

    orcm_octl_globals = tmp;
    char *args = NULL;
    char *str = NULL;
    int tailc = 0;
    char **tailv = NULL;
    int ret = 0;


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
        ret = octl_interactive_cli();
    } else {
        /* otherwise use the user specified command */
        ret = run_cmd(tailc, tailv);
    }
    OBJ_DESTRUCT(&cmd_line);
    opal_argv_free(tailv);
    return ret;
}

static int octl_interactive_cli(void)
{
    bool interactive = true;
    orcm_cli_t cli;
    char *mycmd = NULL;
    int ret = 0;

    octl_init_cli(&cli);

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

        if (NULL == mycmd) {
            fprintf(stderr, "\nNo command specified\n");
            continue;
        }

        if (0 == strcmp(mycmd, "quit")) {
            printf("\nExiting...\n");
            interactive = false;
            free(mycmd);
            continue;
        }

        ret = run_cmd_from_cli(mycmd);
        if (ORCM_ERR_TAKE_NEXT_OPTION == ret) {
            printf("\nUse \'%s<tab>\' or \'%s<?>\' for help.\n", mycmd, mycmd);
        }

        free(mycmd);
    }

    OBJ_DESTRUCT(&cli);
    return ret;
}

static int run_cmd_from_cli(char *cmd)
{
    int argc;
    char **cmdlist = NULL;

    cmdlist = opal_argv_split(cmd, ' ');
    argc = opal_argv_count(cmdlist);
    return run_cmd(argc, cmdlist);
}

static void octl_init_cli(orcm_cli_t *cli)
{
    OBJ_CONSTRUCT(cli, orcm_cli_t);
    orcm_cli_create(cli, cli_init);
    /* give help on top level commands */
    printf("*** WELCOME TO OCTL ***\n Possible commands:\n");
    orcm_cli_print_cmd_help(cli, NULL);
}

static int octl_cmd_to_enum(char* command)
{
    int cmd_enum = NUM_TOKENS;
    int idx = 0;

    if (NULL != command) {
        for (; idx < NUM_TOKENS; idx++) {
            if (0 == strncmp(command, orcm_octl_cmds[idx], strlen(command) + 1)) {
                cmd_enum = idx;
                break;
            }
        }
    }

    return cmd_enum;
}

static int run_cmd_resource(char** cmdlist, int sub_cmd)
{
    int rc = ORCM_SUCCESS;
    switch (sub_cmd) {
        case cmd_status:
            rc = orcm_octl_resource_status(cmdlist);
            break;
        case cmd_add:
            rc = orcm_octl_resource_add(cmdlist);
            break;
        case cmd_remove:
            rc = orcm_octl_resource_remove(cmdlist);
            break;
        case cmd_drain:
            rc = orcm_octl_resource_drain(cmdlist);
            break;
        case cmd_resume:
            rc = orcm_octl_resource_resume(cmdlist);
            break;
        case cmd_null:
            rc = ORCM_ERR_TAKE_NEXT_OPTION;
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_queue(char** cmdlist, int sub_cmd)
{
    int rc = ORCM_SUCCESS;
    switch (sub_cmd) {
        case cmd_status:
            rc = orcm_octl_queue_status(cmdlist);
            break;
        case cmd_policy:
            rc = orcm_octl_queue_policy(cmdlist);
            break;
        case cmd_define:
            rc = orcm_octl_queue_define(cmdlist);
            break;
        case cmd_add:
            rc = orcm_octl_queue_add(cmdlist);
            break;
        case cmd_remove:
            rc = orcm_octl_queue_remove(cmdlist);
            break;
        case cmd_acl:
            rc = orcm_octl_queue_acl(cmdlist);
            break;
        case cmd_priority:
            rc = orcm_octl_queue_priority(cmdlist);
            break;
        case cmd_null:
            rc = ORCM_ERR_TAKE_NEXT_OPTION;
            break;
        default:
            rc = ORCM_ERROR;
    }
    return rc;
}

static int session_power_next_cmd(char* next_cmd, int cmd)
{
    int rc = octl_cmd_to_enum(next_cmd);
    switch (rc) {
        case cmd_budget:
            rc = (cmd_set == cmd ? ORCM_SET_POWER_BUDGET_COMMAND : ORCM_GET_POWER_BUDGET_COMMAND);
            break;
        case cmd_mode:
            rc = (cmd_set == cmd ? ORCM_SET_POWER_MODE_COMMAND : ORCM_GET_POWER_MODE_COMMAND);
            break;
        case cmd_window:
            rc = (cmd_set == cmd ? ORCM_SET_POWER_WINDOW_COMMAND : ORCM_GET_POWER_WINDOW_COMMAND);
            break;
        case cmd_overage:
            rc = (cmd_set == cmd ? ORCM_SET_POWER_OVERAGE_COMMAND : ORCM_GET_POWER_OVERAGE_COMMAND);
            break;
        case cmd_underage:
            rc = (cmd_set == cmd ? ORCM_SET_POWER_UNDERAGE_COMMAND :
                                   ORCM_GET_POWER_UNDERAGE_COMMAND);
            break;
        case cmd_overage_time:
            rc = (cmd_set == cmd ? ORCM_SET_POWER_OVERAGE_TIME_COMMAND :
                                   ORCM_GET_POWER_OVERAGE_TIME_COMMAND);
            break;
        case cmd_underage_time:
            rc = (cmd_set == cmd ? ORCM_SET_POWER_UNDERAGE_TIME_COMMAND :
                                   ORCM_GET_POWER_UNDERAGE_TIME_COMMAND);
            break;
        case cmd_frequency:
            rc = (cmd_set == cmd ? ORCM_SET_POWER_FREQUENCY_COMMAND :
                                   ORCM_GET_POWER_FREQUENCY_COMMAND);
            break;
        case cmd_strict:
            rc = (cmd_set == cmd ? ORCM_SET_POWER_STRICT_COMMAND : ORCM_GET_POWER_STRICT_COMMAND);
            break;
        case cmd_modes:
            rc = (cmd_get == cmd ? ORCM_GET_POWER_MODES_COMMAND : ORCM_ERROR);
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_session(char** cmdlist, int sub_command)
{
    int rc = ORCM_SUCCESS;
    switch (sub_command) {
        case cmd_status:
            rc = orcm_octl_session_status(cmdlist);
            break;
        case cmd_cancel:
            rc = orcm_octl_session_cancel(cmdlist);
            break;
        case cmd_set:
        case cmd_get:
            if (3 > opal_argv_count(cmdlist)) {
                return ORCM_ERR_TAKE_NEXT_OPTION;
            }
            if (ORCM_ERROR != (rc = session_power_next_cmd(cmdlist[2], sub_command))) {
                if (cmd_set == sub_command) {
                    rc = orcm_octl_session_set(rc, cmdlist);
                } else {
                    rc = orcm_octl_session_get(rc, cmdlist);
                }
            }
            break;
        case cmd_null:
            rc = ORCM_ERR_TAKE_NEXT_OPTION;
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_diag(char** cmdlist, int sub_cmd)
{
    int rc = ORCM_SUCCESS;
    switch (sub_cmd) {
        case cmd_cpu:
            rc = orcm_octl_diag_cpu(cmdlist);
            break;
        case cmd_eth:
            rc = orcm_octl_diag_eth(cmdlist);
            break;
        case cmd_mem:
            rc = orcm_octl_diag_mem(cmdlist);
            break;
        case cmd_null:
            rc = ORCM_ERR_TAKE_NEXT_OPTION;
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_power(char** cmdlist, int sub_cmd)
{
    int rc = ORCM_SUCCESS;

    if (cmd_null == sub_cmd) {
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }

    if (cmd_set != sub_cmd && cmd_get != sub_cmd) {
        return ORCM_ERROR;
    }

    if (3 > opal_argv_count(cmdlist)) {
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }

    if (ORCM_ERROR == (rc = session_power_next_cmd(cmdlist[2], sub_cmd))) {
        return ORCM_ERROR;
    }

    if (cmd_set == sub_cmd) {
        return orcm_octl_power_set(rc, cmdlist);
    }

    return orcm_octl_power_get(rc, cmdlist);
}

static int run_cmd_sensor_set(char** cmdlist)
{
    int rc = octl_cmd_to_enum(cmdlist[2]);
    if (cmd_sample_rate == rc) {
        rc = orcm_octl_sensor_sample_rate_set(ORCM_SET_SENSOR_SAMPLE_RATE_COMMAND, cmdlist);
    } else {
        rc = ORCM_ERROR;
    }
    return rc;
}

static int run_cmd_sensor_get(char** cmdlist)
{
    int rc = octl_cmd_to_enum(cmdlist[2]);
    switch (rc) {
        case cmd_policy:
            rc = orcm_octl_sensor_policy_get(ORCM_GET_SENSOR_POLICY_COMMAND, cmdlist);
            break;
        case cmd_sample_rate:
            rc = orcm_octl_sensor_sample_rate_get(ORCM_GET_SENSOR_SAMPLE_RATE_COMMAND, cmdlist);
            break;
        case cmd_inventory:
            rc = orcm_octl_sensor_inventory_get(ORCM_GET_DB_SENSOR_INVENTORY_COMMAND, cmdlist);
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_sensor_store(char** cmdlist)
{
    int rc = octl_cmd_to_enum(cmdlist[2]);
    switch (rc) {
        case cmd_none:
            rc = orcm_octl_sensor_store(0, cmdlist);
            break;
        case cmd_environment_only:
            rc = orcm_octl_sensor_store(1, cmdlist);
            break;
        case cmd_event_only:
            rc = orcm_octl_sensor_store(2, cmdlist);
            break;
        case cmd_all:
            rc = orcm_octl_sensor_store(3, cmdlist);
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_sensor(char** cmdlist, int sub_cmd)
{
    int rc = ORCM_SUCCESS;
    if (cmd_null == sub_cmd || ((cmd_set == sub_cmd || cmd_get == sub_cmd ||
        cmd_store == sub_cmd) && 3 > opal_argv_count(cmdlist))) {
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }
    switch (sub_cmd) {
        case cmd_set:
            rc = run_cmd_sensor_set(cmdlist);
            break;
        case cmd_get:
            rc = run_cmd_sensor_get(cmdlist);
            break;
        case cmd_enable:
            rc = orcm_octl_sensor_change_sampling(0, cmdlist);
            break;
        case cmd_disable:
            rc = orcm_octl_sensor_change_sampling(1, cmdlist);
            break;
        case cmd_reset:
            rc = orcm_octl_sensor_change_sampling(2, cmdlist);
            break;
        case cmd_store:
            rc = run_cmd_sensor_store(cmdlist);
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_notifier_set(int sub_cmd, char** cmdlist)
{
    int rc = ORCM_SUCCESS;
    switch (sub_cmd) {
        case cmd_policy:
            rc = orcm_octl_set_notifier_policy(ORCM_SET_NOTIFIER_POLICY_COMMAND, cmdlist);
            break;
        case cmd_smtp_policy:
            rc = orcm_octl_set_notifier_smtp(ORCM_SET_NOTIFIER_SMTP_COMMAND, cmdlist);
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_notifier_get(int sub_cmd, char** cmdlist)
{
    int rc = ORCM_SUCCESS;
    switch(sub_cmd) {
        case cmd_policy:
            rc = orcm_octl_get_notifier_policy(ORCM_GET_NOTIFIER_POLICY_COMMAND, cmdlist);
            break;
        case cmd_smtp_policy:
            rc = orcm_octl_get_notifier_smtp(ORCM_GET_NOTIFIER_SMTP_COMMAND, cmdlist);
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_notifier(char** cmdlist, int sub_cmd)
{
    int rc = ORCM_SUCCESS;
    int next_cmd = cmd_null;

    if (cmd_null == sub_cmd) {
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }

    if (cmd_set != sub_cmd && cmd_get != sub_cmd) {
        return ORCM_ERROR;
    }

    if (3 > opal_argv_count(cmdlist)) {
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }
    next_cmd = octl_cmd_to_enum(cmdlist[2]);

    rc = (cmd_set == sub_cmd ? run_cmd_notifier_set(next_cmd, cmdlist) :
                               run_cmd_notifier_get(next_cmd, cmdlist));
    return rc;
}

static int run_cmd_grouping(int argc, char** cmdlist, int sub_cmd)
{
    int rc = ORCM_SUCCESS;
    switch (sub_cmd) {
        case cmd_add:
            rc = orcm_octl_logical_group_add(argc, cmdlist);
            break;
        case cmd_remove:
            rc = orcm_octl_logical_group_remove(argc, cmdlist);
            break;
        case cmd_list:
            rc = orcm_octl_logical_group_list(argc, cmdlist);
            break;
        case cmd_null:
            rc = ORCM_ERR_TAKE_NEXT_OPTION;
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_workflow(char** cmdlist, int sub_cmd)
{
    int rc = ORCM_SUCCESS;

    switch (sub_cmd) {
        case cmd_add:
            rc = orcm_octl_workflow_add(cmdlist);
            break;
        case cmd_remove:
            rc = orcm_octl_workflow_remove(cmdlist);
            break;
        case cmd_list:
            rc = orcm_octl_workflow_list(cmdlist);
            break;
        case cmd_null:
            rc = ORCM_ERR_TAKE_NEXT_OPTION;
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_query_node(char** cmdlist)
{
    int rc = ORCM_SUCCESS;

    if (3 > opal_argv_count(cmdlist)) {
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }

    rc = octl_cmd_to_enum(cmdlist[2]);
    rc = (cmd_status != rc ? ORCM_ERROR :
          orcm_octl_query_node(ORCM_GET_DB_QUERY_NODE_COMMAND,cmdlist));
    return rc;
}

static int run_cmd_query_event(char** cmdlist)
{
    int rc = ORCM_SUCCESS;

    if (3 > opal_argv_count(cmdlist)) {
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }

    rc = octl_cmd_to_enum(cmdlist[2]);
    switch (rc) {
        case cmd_data:
            rc = orcm_octl_query_event_data(ORCM_GET_DB_QUERY_EVENT_DATA_COMMAND,cmdlist);
            break;
        case cmd_sensor_data:
            rc = orcm_octl_query_event_snsr_data(ORCM_GET_DB_QUERY_EVENT_SNSR_DATA_COMMAND,cmdlist);
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_query(char** cmdlist, int sub_cmd)
{
    int rc = ORCM_SUCCESS;
    switch (sub_cmd) {
        case cmd_sensor:
            rc = orcm_octl_query_sensor(ORCM_GET_DB_QUERY_SENSOR_COMMAND,cmdlist);
            break;
        case cmd_history:
            rc = orcm_octl_query_sensor(ORCM_GET_DB_QUERY_HISTORY_COMMAND,cmdlist);
            break;
        case cmd_log:
            rc = orcm_octl_query_log(ORCM_GET_DB_QUERY_LOG_COMMAND,cmdlist);
            break;
        case cmd_idle:
            rc = orcm_octl_query_idle(ORCM_GET_DB_QUERY_IDLE_COMMAND,cmdlist);
            break;
        case cmd_node:
            rc = run_cmd_query_node(cmdlist);
            break;
        case cmd_event:
            rc = run_cmd_query_event(cmdlist);
            break;
        case cmd_null:
            rc = ORCM_ERR_TAKE_NEXT_OPTION;
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd_chasis_id(char** cmdlist, int sub_cmd)
{
    int rc = ORCM_SUCCESS;
    switch (sub_cmd) {
        case cmd_enable:
            rc = orcm_octl_chassis_id_on(cmdlist);
            break;
        case cmd_disable:
            rc = orcm_octl_chassis_id_off(cmdlist);
            break;
        case cmd_state:
            rc = orcm_octl_chassis_id_state(cmdlist);
            break;
        case cmd_null:
            rc = ORCM_ERR_TAKE_NEXT_OPTION;
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }
    return rc;
}

static int run_cmd(int argc, char *cmdlist[])
{
    int rc = ORCM_SUCCESS;
    int sub_cmd = cmd_null;

    if (0 == argc) {
        orcm_octl_error("empty-cmd");
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }

    rc = octl_cmd_to_enum(cmdlist[0]);
    if (2 <= argc) {
        sub_cmd = octl_cmd_to_enum(cmdlist[1]);
    }

    /* call corresponding function to passed command */
    switch (rc) {
        case cmd_resource:
            rc = run_cmd_resource(cmdlist, sub_cmd);
            break;
        case cmd_queue:
            rc = run_cmd_queue(cmdlist, sub_cmd);
            break;
        case cmd_session:
            rc = run_cmd_session(cmdlist, sub_cmd);
            break;
        case cmd_diag:
            rc = run_cmd_diag(cmdlist, sub_cmd);
            break;
        case cmd_power:
            rc = run_cmd_power(cmdlist, sub_cmd);
            break;
        case cmd_sensor:
            rc = run_cmd_sensor(cmdlist, sub_cmd);
            break;
        case cmd_notifier:
            rc = run_cmd_notifier(cmdlist, sub_cmd);
            break;
        case cmd_grouping:
            rc = run_cmd_grouping(argc, cmdlist, sub_cmd);
            break;
        case cmd_workflow:
            rc = run_cmd_workflow(cmdlist, sub_cmd);
            break;
        case cmd_query:
            rc = run_cmd_query(cmdlist, sub_cmd);
            break;
        case cmd_chassis_id:
            rc = run_cmd_chasis_id(cmdlist, sub_cmd);
            break;
        default:
            rc = ORCM_ERROR;
            break;
    }

    if (ORCM_ERROR == rc) {
        octl_print_error_illegal_command(cmdlist);
    } else {
        octl_print_error(rc);
    }

    return rc;
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
        if (ORCM_ERR_TAKE_NEXT_OPTION == rc) {
            fprintf(stdout, "ERROR: invalid arguments!\n");
        } else {
            const char *errmsg = ORTE_ERROR_NAME(rc);
            if (NULL != errmsg) {
                fprintf(stdout, "\nERROR: %s\n", errmsg);
            } else {
                fprintf(stdout, "\nERROR: Internal\n");
            }
        }
    }
}

static void octl_print_error_illegal_command(char **cmdlist)
{
    char *cmd = NULL;
    if (NULL != cmdlist) {
        cmd = opal_argv_join(cmdlist, ' ');
        if (NULL != cmd) {
            octl_print_illegal_command(cmd);
            free(cmd);
        }
    }
}
