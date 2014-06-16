/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_UTIL_CLI_H
#define ORCM_UTIL_CLI_H

#include "orcm_config.h"

#include "opal/class/opal_object.h"
#include "opal/class/opal_list.h"

#define ORCM_MAX_CLI_LENGTH  1024

typedef struct {
    char *parent;  // name of the parent command - NULL indicates top-level cmd
    char *cmd;     // name of this command, must include the dashes if an option that needs them
    bool option;   // is this an option or a sublevel command
    int nargs;     // number of arguments this option or command takes
    char *help;    // help message to display at a help request
} orcm_cli_init_t;
OBJ_CLASS_DECLARATION(orcm_cli_init_t);

typedef struct {
    opal_list_item_t super;
    char *cmd;
    /* opal_value_t list of options - the key is the
     * name of the option, and the value is the value
     * of any argument passed to it */
    opal_list_t options;
    /* help string to be output if requested */
    char *help;
    /* list of orcm_cli_cmd_t cmds under this one */
    opal_list_t subcmds;
} orcm_cli_cmd_t;
OBJ_CLASS_DECLARATION(orcm_cli_cmd_t);

typedef struct {
    opal_object_t super;
    opal_list_t cmds;  // list of orcm_cli_cmd_t objects
} orcm_cli_t;
OBJ_CLASS_DECLARATION(orcm_cli_t);

ORCM_DECLSPEC int orcm_cli_create(orcm_cli_t *cli,
                                  orcm_cli_init_t *input);

ORCM_DECLSPEC int orcm_cli_get_cmd(char *prompt,
                                   orcm_cli_t *cli,
                                   char **cmd);

ORCM_DECLSPEC void orcm_cli_print_cmd(orcm_cli_cmd_t *cmd,
                                      char *prefix);

#endif
