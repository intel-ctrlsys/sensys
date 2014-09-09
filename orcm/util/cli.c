/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
#include "orcm_config.h"
#include "orcm/constants.h"

#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "opal/util/argv.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/util/cli.h"

#define BONK putchar('\a');

static int make_opt(orcm_cli_t *cli, orcm_cli_init_t *e);
static int make_opt_subtree(orcm_cli_cmd_t *cmd, orcm_cli_init_t *e, int level);
static void print_subtree(orcm_cli_cmd_t *command, int level);
static int get_completions(orcm_cli_t *cli, char **input,
                           char ***completions, opal_list_t *options);
static int get_completions_subtree(orcm_cli_cmd_t *cmd, char **input,
                                   char ***completions, opal_list_t *options);
static int print_completions(orcm_cli_t *cli, char **input);
static int print_completions_subtree(orcm_cli_cmd_t *cmd, char **input);

int orcm_cli_create(orcm_cli_t *cli,
                    orcm_cli_init_t *table)
{
    int i;
    int rc;

    if (NULL == cli) {
        return ORCM_ERR_BAD_PARAM;
    }

    /* Ensure we got a table */

    if (NULL == table) {
        return ORCM_SUCCESS;
    }

    /* Loop through the table */
    for (i = 0; ; ++i) {

        /* Is this the end? */
        if (NULL == table[i].parent[0] &&
            NULL == table[i].cmd) {
            break;
        }

        /* Nope -- it's an entry.  Process it. */

        if (ORCM_SUCCESS != (rc = make_opt(cli, &table[i]))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }
    
    return ORCM_SUCCESS;

}

static int make_opt(orcm_cli_t *cli, orcm_cli_init_t *e)
{
    orcm_cli_cmd_t *cmd, *parent;

    /* if the parent is not NULL, then look for the matching
     * parent in the command tree and put this one under it */
    if (NULL != e->parent[0]) {
        OPAL_LIST_FOREACH(parent, &cli->cmds, orcm_cli_cmd_t) {
            if (0 == strcasecmp(parent->cmd, e->parent[0])) {
                /* recursive search down the subtree */
                return make_opt_subtree(parent, e, 1);
            }
        }
        return ORCM_ERR_NOT_FOUND;
    } else {
        /* just add it to the top-level cmd tree */
        cmd = OBJ_NEW(orcm_cli_cmd_t);
        if (NULL != e->cmd) {
            cmd->cmd = strdup(e->cmd);
        }
        if (NULL != e->help) {
            cmd->help = strdup(e->help);
        }
        opal_list_append(&cli->cmds, &cmd->super);
    }
    return ORCM_SUCCESS;
}

static int make_opt_subtree(orcm_cli_cmd_t *cmd, orcm_cli_init_t *e, int level) {
    orcm_cli_cmd_t *newcmd, *parent;
    opal_value_t *kv;
    
    /* we will keep descending down the subtree until we have nothing
     * more on the parent list to compare to */
    if (NULL == e->parent[level]) {
        /* if this is an option, put it there */
        if (e->option) {
            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup(e->cmd);
            kv->type = OPAL_INT;
            kv->data.integer = e->nargs;
            opal_list_append(&cmd->options, &kv->super);
        } else {
            /* add the commnad in the correct place */
            newcmd = OBJ_NEW(orcm_cli_cmd_t);
            if (NULL != e->cmd) {
                newcmd->cmd = strdup(e->cmd);
            }
            if (NULL != e->help) {
                newcmd->help = strdup(e->help);
            }
            opal_list_append(&cmd->subcmds, &newcmd->super);
        }
        return ORCM_SUCCESS;
    } else {
        /* have more work to do, descend down subtree */
        OPAL_LIST_FOREACH(parent, &cmd->subcmds, orcm_cli_cmd_t) {
            if (0 == strcasecmp(parent->cmd, e->parent[level])) {
                return make_opt_subtree(parent, e, level+1);
            }
        }
        return ORCM_ERR_NOT_FOUND;
    }
}

int orcm_cli_get_cmd(char *prompt,
                     orcm_cli_t *cli,
                     char **cmd)
{
    char c;
    opal_value_t *kv;
    opal_list_t *options;
    struct termios settings, initial_settings;
    char input[ORCM_MAX_CLI_LENGTH];
    bool space;
    size_t j, k;
    char **completions, **inputlist;
    int rc = ORCM_SUCCESS;

    /* prep the stack */
    memset(input, 0, ORCM_MAX_CLI_LENGTH);
    j = 0;
    space = false;

    /* prep the console */
    tcgetattr(STDIN_FILENO, &initial_settings);
    settings = initial_settings;

    /* Set the console mode to no-echo, raw input. */
    settings.c_cc[VTIME] = 1;
    settings.c_cc[VMIN] = 1;
    settings.c_iflag &= ~(IXOFF);
    settings.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &settings);

    /* output the prompt */
    fprintf(stdout, "%s> ", prompt);
    /* process command input */
    while ('\n' != (c = getchar())) {
        switch (c) {
        case '\t':   // auto-completion
            space = false;
            options = NULL;
            completions = NULL;
            /* break command line so far into array of strings */
            inputlist = opal_argv_split(input, ' ');
            /* get possible completions for the commandline so far */
            rc = get_completions(cli, inputlist, &completions, options);
            if (ORCM_ERR_NOT_FOUND == rc) {
                /* Bad command not in tree, Bonk */
                BONK;
                break;
            } else if (ORCM_SUCCESS == rc) {
                if (NULL == completions) {
                    if (NULL == options || opal_list_is_empty(options)) {
                        /* no completions and no options, Bonk */
                        BONK;
                        break;
                    } else {
                        /* no completions, list options */
                        printf("\nOPTIONS: ");
                        OPAL_LIST_FOREACH(kv, options, opal_value_t) {
                            printf("%s  ", kv->key);
                        }
                    }
                } else if (1 == opal_argv_count(completions)) {
                    /* only 1 possible completion, go ahead and complete it */
                    inputlist = opal_argv_split(input, ' ');
                    if (' ' == input[j-1]) {
                        k = 0;
                    } else if (0 != strncmp(inputlist[(opal_argv_count(inputlist) - 1)],
                                            completions[0],
                                            strlen(inputlist[(opal_argv_count(inputlist) - 1)]))) {
                        
                        putchar(' ');
                        input[j++] = ' ';
                        k = 0;
                    } else {
                        k = strlen(inputlist[(opal_argv_count(inputlist) - 1)]);
                    }
                    for (; k < strlen(completions[0]); k++) {
                        putchar(completions[0][k]);
                        input[j++] = completions[0][k];
                    }
                    putchar(' ');
                    input[j++] = ' ';
                    space = true;
                } else {
                    /* multiple completions, list possibilities */
                    if (j != 0 && ' ' != input[j-1]) {
                        putchar(' ');
                        input[j++] = ' ';
                        space = true;
                    }
                    printf("\n\t%s", opal_argv_join(completions, ' '));
                }
                printf("\n%s> %s", prompt, input);
                break;
            } else {
                goto process;
            }

        case ' ':    // space
            if (0 == j || space) {
                /* if there is already a space on the input line, Bonk */
                BONK;
                break;
            }
            putchar(c);
            input[j++] = c;
            space = true;
            /* check for validity of the last input since it wasn't
             * entered via auto-completion */
            break;

        case '\b':   // backspace
        case 0x7f:   // backspace
            space = false;
            if (0 == j) {
                /* error - bonk */
                BONK;
                break;
            }
            /* echo the erase sequence */
            putchar('\b'); putchar(' '); putchar('\b');
            /* remove the last character from input */
            j--;
            input[j] = '\0';
            /* check to see if we already have a space */
            if (0 < j && ' ' == input[j-1]) {
                space = true;
            }
            break;
                
        case 0x1b:   // arrows
            /* this is to ignore the garbage that comes out when you type arrows */
            c = getchar();
            switch(c) {
            case '[':
                c = getchar();
                switch(c) {
                case 'A':
                    /* up arrow */
                case 'B':
                    /* down arrow */
                case 'C':
                    /* right arrow */
                case 'D':
                    /* left arrow */
                default:
                    break;
                }
            default:
                break;
            }
            break;

        case '?':   // help special char
            /* list help message of possible completions */
            options = NULL;
            inputlist = opal_argv_split(input, ' ');
            printf("\nPossible commands:\n");
            print_completions(cli, inputlist);
            printf("\n%s> %s", prompt, input);
            break;

        default:   // everything else
            space = false;
            if (ORCM_MAX_CLI_LENGTH == j) {
                /* can't go that far - bonk */
                BONK;
                break;
            }
            /* echo it */
            putchar(c);
            /* add it to our array */
            input[j++] = c;
            break;
        }
    }

 process:
    /* restore the initial settings */
    tcsetattr(STDIN_FILENO, TCSANOW, &initial_settings);

    /* return the assembled command */
    *cmd = strdup(input);

    return rc;
}

static int get_completions(orcm_cli_t *cli, char **input,
                           char ***completions, opal_list_t *options)
{
    orcm_cli_cmd_t *sub_command;
    int i;
    bool found = false;
    
    i = opal_argv_count(input);
    /* no input, must be at top level
     * add all top level commands to compeltion list */
    if (0 == opal_argv_count(input)) {
        OPAL_LIST_FOREACH(sub_command, &cli->cmds, orcm_cli_cmd_t) {
            opal_argv_append_nosize(completions, sub_command->cmd);
        }
        return ORCM_SUCCESS;
    }
    
    /* look for top level command to descend into subtree with */
    OPAL_LIST_FOREACH(sub_command, &cli->cmds, orcm_cli_cmd_t) {
        if (0 == strncmp(sub_command->cmd, input[0], strlen(input[0]))) {
            /* if we match our input so far with a command... */
            if (strlen(input[0]) == strlen(sub_command->cmd)) {
                /* if input fully matches command descend */
                opal_argv_delete(&i, &input, 0, 1);
                /* recursive command to search subtree */
                return get_completions_subtree(sub_command, input,
                                               completions, options);
            } else {
                /* otherwise add this to our list of possible completions */
                opal_argv_append_nosize(completions, sub_command->cmd);
                found = true;
            }
        }
    }
    opal_argv_free(input);
    if (!found) {
        return ORCM_ERR_NOT_FOUND;
    }
    return ORCM_SUCCESS;
}

static int get_completions_subtree(orcm_cli_cmd_t *cmd, char **input,
                                   char ***completions, opal_list_t *options)
{
    orcm_cli_cmd_t *sub_command;
    int i;
    bool found = false;
    
    i = opal_argv_count(input);
    /* nothing on the input line passed to us
     * append all commands at this level */
    if (0 == i) {
        if (opal_list_is_empty(&cmd->subcmds)) {
            options = &cmd->options;
            if (NULL != *completions) {
                opal_argv_free(*completions);
            }
        } else {
            OPAL_LIST_FOREACH(sub_command, &cmd->subcmds, orcm_cli_cmd_t) {
                opal_argv_append_nosize(completions, sub_command->cmd);
            }
        }
        return ORCM_SUCCESS;
    }
    
    /* look for commands to match input with */
    OPAL_LIST_FOREACH(sub_command, &cmd->subcmds, orcm_cli_cmd_t) {
        if (0 == strncmp(sub_command->cmd, input[0], strlen(input[0]))) {
            /* if we match our input so far with a command... */
            if (strlen(input[0]) == strlen(sub_command->cmd)) {
                /* if input fully matches command descend recursively */
                opal_argv_delete(&i, &input, 0, 1);
                return get_completions_subtree(sub_command, input,
                                               completions, options);
            } else {
                /* otherwise add this to our list of possible completions */
                opal_argv_append_nosize(completions, sub_command->cmd);
                found = true;
            }
        }
    }
    opal_argv_free(input);
    if (!found) {
        return ORCM_ERR_NOT_FOUND;
    }
    return ORCM_SUCCESS;
}

static int print_completions(orcm_cli_t *cli, char **input)
{
    orcm_cli_cmd_t *sub_command;
    int i;
    bool found = false;
    
    i = opal_argv_count(input);
    /* if no input, print help for all commands at top level */
    if (0 == opal_argv_count(input)) {
        OPAL_LIST_FOREACH(sub_command, &cli->cmds, orcm_cli_cmd_t) {
            orcm_cli_print_cmd(sub_command, NULL);
        }
        return ORCM_SUCCESS;
    }
    
    /* search for matching commands */
    OPAL_LIST_FOREACH(sub_command, &cli->cmds, orcm_cli_cmd_t) {
        if (0 == strncmp(sub_command->cmd, input[0], strlen(input[0]))) {
            /* if we match our input so far with a command... */
            if (strlen(input[0]) == strlen(sub_command->cmd)) {
                /* if input fully matches command descend */
                opal_argv_delete(&i, &input, 0, 1);
                return print_completions_subtree(sub_command, input);
            } else {
                /* otherwise print help for possible completions */
                orcm_cli_print_cmd(sub_command, NULL);
                found = true;
            }
        }
    }
    opal_argv_free(input);
    if (!found) {
        return ORCM_ERR_NOT_FOUND;
    }
    return ORCM_SUCCESS;

}

static int print_completions_subtree(orcm_cli_cmd_t *cmd, char **input)
{
    orcm_cli_cmd_t *sub_command;
    int i;
    bool found = false;
    
    i = opal_argv_count(input);
    /* if no input, print help for all commands at this level */
    if (0 == i) {
        OPAL_LIST_FOREACH(sub_command, &cmd->subcmds, orcm_cli_cmd_t) {
            orcm_cli_print_cmd(sub_command, NULL);
        }
        return ORCM_SUCCESS;
    }
    
    OPAL_LIST_FOREACH(sub_command, &cmd->subcmds, orcm_cli_cmd_t) {
        if (0 == strncmp(sub_command->cmd, input[0], strlen(input[0]))) {
            /* if we match our input so far with a command... */
            if (strlen(input[0]) == strlen(sub_command->cmd)) {
                opal_argv_delete(&i, &input, 0, 1);
                /* if input fully matches command descend recursively */
                return print_completions_subtree(sub_command, input);
            } else {
                /* print help for possible completions */
                orcm_cli_print_cmd(sub_command, NULL);
                found = true;
            }
        }
    }
    opal_argv_free(input);
    if (!found) {
        return ORCM_ERR_NOT_FOUND;
    }
    return ORCM_SUCCESS;
}

void orcm_cli_print_cmd(orcm_cli_cmd_t *cmd, char *prefix)
{
    opal_value_t *kv;
    printf("%-2s %-20s %-10s\n",
           (NULL == prefix) ? "" : prefix,
           (NULL == cmd->cmd) ? "NULL" : cmd->cmd,
           (NULL == cmd->help) ? "NULL" : cmd->help);
    OPAL_LIST_FOREACH(kv, &cmd->options, opal_value_t) {
        printf("\tOPTION: %s nargs %d\n", kv->key, kv->data.integer);
    }
}

void orcm_cli_print_tree(orcm_cli_t *cli)
{
    orcm_cli_cmd_t *sub_command;
    
    /* recursive set of functions to print the whole command tree */
    
    OPAL_LIST_FOREACH(sub_command, &cli->cmds, orcm_cli_cmd_t) {
        printf("%s\n",
               (NULL == sub_command->cmd) ? "NULL" : sub_command->cmd);
        print_subtree(sub_command, 1);
    }
}

static void print_subtree(orcm_cli_cmd_t *command, int level)
{
    orcm_cli_cmd_t *sub_command;
    int i;
    
    OPAL_LIST_FOREACH(sub_command, &command->subcmds, orcm_cli_cmd_t) {
        for (i = 0; i < level; i++) {
            printf("  ");
        }
        /* use special unicode box chars for help displaying tree */
        printf("\u2514\u2500%s\n",
               (NULL == sub_command->cmd) ? "NULL" : sub_command->cmd);
        print_subtree(sub_command, level+1);
    }
}

/***   CLASS INSTANCES   ***/
static void cmdcon(orcm_cli_cmd_t *p)
{
    p->cmd = NULL;
    OBJ_CONSTRUCT(&p->options, opal_list_t);;
    p->help = NULL;
    OBJ_CONSTRUCT(&p->subcmds, opal_list_t);
}
static void cmddes(orcm_cli_cmd_t *p)
{
    if (NULL != p->cmd) {
        free(p->cmd);
    }
    OPAL_LIST_DESTRUCT(&p->options);
    if (NULL != p->help) {
        free(p->help);
    }
    OPAL_LIST_DESTRUCT(&p->subcmds);
}
OBJ_CLASS_INSTANCE(orcm_cli_cmd_t,
                   opal_list_item_t,
                   cmdcon, cmddes);

static void clicon(orcm_cli_t *p)
{
    OBJ_CONSTRUCT(&p->cmds, opal_list_t);
}
static void clides(orcm_cli_t *p)
{
    OPAL_LIST_DESTRUCT(&p->cmds);
}
OBJ_CLASS_INSTANCE(orcm_cli_t,
                   opal_object_t,
                   clicon, clides);
