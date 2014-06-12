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

/* define a stack object for tracking where we are
 * in constructing the input command. We'll use this
 * to enable us to backup if/when the user types
 * delete characters */
typedef struct {
    opal_list_item_t super;
    orcm_cli_cmd_t *cmd;  // cmd we are currently working on
    opal_value_t *kv;     // option we are currently working on
    char input[ORCM_MAX_CLI_LENGTH];  // current input string
    int j;                // current position in the input string
} orcm_cli_stack_t;
static void stcon(orcm_cli_stack_t *p)
{
    p->cmd = NULL;
    p->kv = NULL;
    memset(p->input, 0, ORCM_MAX_CLI_LENGTH);
    p->j = 0;
}
OBJ_CLASS_INSTANCE(orcm_cli_stack_t,
                   opal_list_item_t,
                   stcon, NULL);

static int make_opt(orcm_cli_t *cli, orcm_cli_init_t *e);
static orcm_cli_stack_t* backup(opal_list_t *stack,
                                orcm_cli_stack_t *p);
static orcm_cli_stack_t* build_option(opal_list_t *stack,
                                      orcm_cli_stack_t *p);
static bool print_options(char **options,
                          orcm_cli_stack_t *p);

int orcm_cli_create(orcm_cli_t *cli,
                    orcm_cli_init_t *table)
{
    int i;
    int rc;

    if (NULL == cli) {
        return ORCM_ERR_BAD_PARAM;
    }

    OBJ_CONSTRUCT(cli, orcm_cli_t);

    /* Ensure we got a table */

    if (NULL == table) {
        return ORCM_SUCCESS;
    }

    /* Loop through the table */

    for (i = 0; ; ++i) {

        /* Is this the end? */

        if (NULL == table[i].parent &&
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

int orcm_cli_get_cmd(char *prompt,
                     orcm_cli_t *cli,
                     char **cmd)
{
    char c;
    opal_list_t stack;
    orcm_cli_stack_t *active, *parent;
    orcm_cli_cmd_t *c2, *csav;
    opal_value_t *kv, *ksav;
    char *tmp, **options;
    struct termios settings, initial_settings;

    /* prep the stack */
    OBJ_CONSTRUCT(&stack, opal_list_t);
    active = OBJ_NEW(orcm_cli_stack_t);
    opal_list_append(&stack, &active->super);

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
            /* init the search */
            csav = NULL;
            ksav = NULL;
            options = NULL;
            if (0 == active->j) {
                /* if they are at the start of the word, then print
                 * out all allowed possibilities */
                putchar('\n');
                /* if we are at the top of the stack, print all
                 * the cmds in the incoming orcm_cli_t list */
                if (&active->super == opal_list_get_first(&stack)) {
                    OPAL_LIST_FOREACH(c2, &cli->cmds, orcm_cli_cmd_t) {
                        opal_argv_append_nosize(&options, c2->cmd);
                        csav = c2;
                    }
                } else {
                    /* print all our parent's subcmds and options */
                    parent = (orcm_cli_stack_t*)opal_list_get_prev(&active->super);
                    OPAL_LIST_FOREACH(c2, &parent->cmd->subcmds, orcm_cli_cmd_t) {
                        opal_argv_append_nosize(&options, c2->cmd);
                        csav = c2;
                    }
                    OPAL_LIST_FOREACH(kv, &parent->cmd->options, opal_value_t) {
                        (void)asprintf(&tmp, "--%s", kv->key);
                        opal_argv_append_nosize(&options, tmp);
                        free(tmp);
                        ksav = kv;
                    }
                }
                if (print_options(options, active)) {
                    /* the command is complete - can only happen if there
                     * was only one choice (i.e., only one entry in options),
                     * which means that either csav or ksav is pointing to it.
                     * record the option  or command it corresponds to in case
                     * we need to backup at some point */
                    if (NULL != csav) {
                        active->cmd = csav;
                    } else if (NULL != ksav) {
                        active->kv = ksav;
                    } else {
                        /* error - shouldn't be possible */
                        ORTE_ERROR_LOG(ORTE_ERROR);
                        return ORTE_ERROR;
                    }
                    /* move to the next */
                    active = OBJ_NEW(orcm_cli_stack_t);
                    opal_list_append(&stack, &active->super);
                }
                break;
            }
            /* if we are inside a word, then we cycle across known
             * possibilities to see if any match the currently-input
             * set of characters. Note that we don't have to be concerned
             * here about options as there is no way we could be partway thru
             * an option - options get handled in their own subroutine.
             *
             * if we are at the top of the stack,  we need to cycle across the
             * cmds in the incoming orcm_cli_t list to see if we can find a match */
            if (&active->super == opal_list_get_first(&stack)) {
                OPAL_LIST_FOREACH(c2, &cli->cmds, orcm_cli_cmd_t) {
                    if (0 == strncmp(c2->cmd, active->input, active->j)) {
                        opal_argv_append_nosize(&options, c2->cmd);
                        csav = c2;
                    }
                }
            } else {
                /* cycle across the subcmds of our parent as these match our level */
                parent = (orcm_cli_stack_t*)opal_list_get_prev(&active->super);
                OPAL_LIST_FOREACH(c2, &parent->cmd->subcmds, orcm_cli_cmd_t) {
                    if (0 == strncmp(c2->cmd, active->input, active->j)) {
                        opal_argv_append_nosize(&options, c2->cmd);
                        csav = c2;
                    }
                }
            }
            if (print_options(options, active)) {
                /* can only happen if there was only one choice (i.e., only
                 * one entry in options),  which means that either csav or
                 * ksav is pointing to it.  record the option  or command it
                 * corresponds to in case we need to backup at some point */
                if (NULL != csav) {
                    active->cmd = csav;
                } else {
                    /* error - shouldn't be possible */
                    ORTE_ERROR_LOG(ORTE_ERROR);
                    break;
                }
                /* move to the next */
                active = OBJ_NEW(orcm_cli_stack_t);
                opal_list_append(&stack, &active->super);
            }
            break;

        case '-':
            if (0 < active->j) {
                /* this is occurring inside of a cmd name,
                 * so that's okay so long as it is a valid
                 * command - which we will check later */
                active->input[active->j] = c;
                active->j++;
                /* echo it */
                putchar(c);
            } else {
                /* first check that this isn't happening at the top of the stack
                 * as that wouldn't be allowed - options can only be provided
                 * as part of a higher-level command */
                if (&active->super == opal_list_get_first(&stack)) {
                    /* bad boy - bonk */
                    putchar('\a');
                    break;
                }
                /* echo it */
                putchar(c);
                /* record it */
                active->input[active->j] = c;
                active->j++;
                /* have to handle dashes separately as they
                 * could be the start of an mca param. The build_option
                 * function will only return once the option has been
                 * completed (return=NULL), or the user backs out of
                 * the option (return cmd above the option) */
                if (NULL == (active = build_option(&stack, active))) {
                    active = OBJ_NEW(orcm_cli_stack_t);
                    opal_list_append(&stack, &active->super);
                } else if (&active->super == opal_list_get_first(&stack)) {
                    /* if the top-of-stack was returned, then the
                     * user typed a \n at the end of the option
                     * to indicate completion of the entire command
                     * sequence - so we are done! */
                    goto process;
                }
            }
            break;

        case ' ':
            /* it is always possible somebody just typed an extra
             * space here, so we should ignore it if they did */
            if (0 == active->j) {
                break;
            }
            /* user has completed entry of a command without using
             * auto-complete, so we should check to ensure the cmd
             * is a valid one. First, we have to find the command */
            csav = NULL;
            if (&active->super == opal_list_get_first(&stack)) {
                /* we are working on the top-level command, so check
                 * the input orcm_cli_t for valid commands */
                OPAL_LIST_FOREACH(c2, &cli->cmds, orcm_cli_cmd_t) {
                    if (0 == strcmp(c2->cmd, active->input)) {
                        csav = c2;
                        break;
                    }
                }
            } else {
                if (NULL == active->cmd) {
                    /* we don't know how to go up */
                    putchar('\n');
                    putchar('\a');
                    ORTE_ERROR_LOG(ORTE_ERROR);
                    break;
                }
                /* cycle across the subcmds of our parent */
                OPAL_LIST_FOREACH(c2, &active->cmd->subcmds, orcm_cli_cmd_t) {
                    if (0 == strcmp(c2->cmd, active->input)) {
                        csav = c2;
                        break;
                    }
                }
            }
            if (NULL == csav) {
                /* not a supported command - bonk them */
                putchar('\a');
                break;
            }
            /* save so we can climb back up later */
            active->cmd = csav;
            /* setup for the next step */
            active = OBJ_NEW(orcm_cli_stack_t);
            opal_list_append(&stack, &active->super);
            break;

        case '\b':
        case 0x7f:
            /* backup the input - if this goes past the beginning
             * of the active input, then the function will release
             * that object and return a pointer to the one above it.
             * Otherwise, it will return the same pointer so we can
             * continue editing it. If we get a NULL, it means we
             * backed all the way up the stack to the top, so we
             * need to create a new starting point */
            if (NULL == (active = backup(&stack, active))) {
                active = OBJ_NEW(orcm_cli_stack_t);
                opal_list_append(&stack, &active->super);
            }
            break;

        default:
            /* echo it */
            putchar(c);
            /* add it to our array */
            active->input[active->j] = c;
            active->j++;
        }
    }

 process:
    /* restore the initial settings */
    tcsetattr(STDIN_FILENO, TCSANOW, &initial_settings);

    /* return the assembled command */
    options = NULL;
    OPAL_LIST_FOREACH(active, &stack, orcm_cli_stack_t) {
        opal_argv_append_nosize(&options, active->input);
    }
    if (NULL != options) {
        *cmd = opal_argv_join(options, ' ');
        opal_argv_free(options);
    } else {
        *cmd = NULL;
    }

    return ORCM_SUCCESS;
}

static orcm_cli_stack_t* build_option(opal_list_t *stack,
                                      orcm_cli_stack_t *p)
{
    orcm_cli_stack_t *active, *parent;
    char c;
    //   int dash;
    
    /* the options are contained in the parent, so let's
     * get that pointer now as we'll need it later */
    parent = (orcm_cli_stack_t*)opal_list_get_prev(&p->super);

     /* collect the option until we hit a space */
    while ('\n' != (c = getchar())) {
        switch (c) {
        case '\t':
            /* try to complete - if we can complete,
             * then see if arguments are required and
             * collect the specified number */

            /* otherwise, we keep going */
            break;

        case ' ':
            /* see if this was a valid option */

            /* if so, then see if arguments are required and
             * collect the specified number */

            /* return a NULL if all arguments are collected */

            /* return our parent if they backed out of the arguments */

            break;

        case '\b':
        case 0x7f:
            /* backup the input - if this goes past the beginning
             * of the active input, then the function will release
             * that object and return a pointer to the one above it.
             * Otherwise, it will return the same pointer so we can
             * continue editing it. If we get a NULL, it means we
             * backed all the way up the stack to the top, so we
             * need to create a new starting point */
            if (NULL == (active = backup(stack, p))) {
                active = OBJ_NEW(orcm_cli_stack_t);
                opal_list_append(stack, &active->super);
            }
            break;

        default:
            /* echo */
            putchar(c);
            /* add to array */
            p->input[p->j] = c;
            p->j++;
        }
    }

    return active;
}

static orcm_cli_stack_t* backup(opal_list_t *stack,
                                orcm_cli_stack_t *p)
{
    orcm_cli_stack_t *ret;
    opal_list_item_t *item;

    /* echo the erase sequence */
    putchar('\b'); putchar(' '); putchar('\b');
    /* remove the last character from input */
    if (0 < p->j) {
        p->input[p->j] = '\0';
        p->j--;
        ret = p;
    } else {
        opal_output(0, "POP STACK");
        if (opal_list_get_begin(stack) == (item = opal_list_get_prev(&p->super))) {
            ret = NULL;
        } else {
            ret = (orcm_cli_stack_t*)item;
        }
        opal_list_remove_item(stack, &p->super);
        OBJ_RELEASE(p);
    }
    return ret;
}

static bool print_options(char **options,
                          orcm_cli_stack_t *p)
{
    int argc = opal_argv_count(options);
    size_t i;
    bool ret;

    if (0 == argc) {
        /* bad input - bonk them */
        putchar('\a');
        ret = false;
    } else if (1 == argc) {
        /* only one possibility - output the rest of the characters
         * followed by a space */
        for (i=p->j; i < strlen(options[0]); i++) {
            putchar(options[0][i]);
            /* and add it to the cmd */
            p->input[i] = options[0][i];
        }
        putchar(' ');
        ret = true;  // word complete
    } else {
        /* multiple options - print them on a line below */
        putchar('\n');
        for (i=0; NULL != options[i]; i++) {
            printf("%s  ", options[i]);
        }
        printf("\n%s", p->input);
        ret = false;
    }
    opal_argv_free(options);
    return ret;
}


static int make_opt(orcm_cli_t *cli, orcm_cli_init_t *e)
{
    orcm_cli_cmd_t *cmd, *parent;
    opal_value_t *kv;

    /* if the parent is not NULL, then look for the matching
     * parent in the command tree and put this one under it */
    if (NULL != e->parent) {
        OPAL_LIST_FOREACH(parent, &cli->cmds, orcm_cli_cmd_t) {
            if (0 == strcasecmp(parent->cmd, e->parent)) {
                /* if this is an option, put it there */
                if (e->option) {
                    kv = OBJ_NEW(opal_value_t);
                    kv->key = strdup(e->cmd);
                    kv->type = OPAL_INT;
                    kv->data.integer = e->nargs;
                    opal_list_append(&parent->options, &kv->super);
                } else {
                    cmd = OBJ_NEW(orcm_cli_cmd_t);
                    if (NULL != e->cmd) {
                        cmd->cmd = strdup(e->cmd);
                    }
                    if (NULL != e->help) {
                        cmd->help = strdup(e->help);
                    }
                    opal_list_append(&parent->subcmds, &cmd->super);
                }
                return ORCM_SUCCESS;;
            }
        }
        /* if the parent wasn't found, then that's an error */
        ORTE_ERROR_LOG(ORCM_ERR_NOT_FOUND);
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

void orcm_cli_print_cmd(orcm_cli_cmd_t *cmd, char *prefix)
{
    opal_value_t *kv;
    orcm_cli_cmd_t *sub;
    char *pf2;

    (void)asprintf(&pf2, "%s\t", (NULL == prefix) ? "" : prefix);
    opal_output(0, "%sCMD: %s HELP %s",
                (NULL == prefix) ? "" : prefix,
                (NULL == cmd->cmd) ? "NULL" : cmd->cmd,
                (NULL == cmd->help) ? "NULL" : cmd->help);
    OPAL_LIST_FOREACH(kv, &cmd->options, opal_value_t) {
        opal_output(0, "\tOPTION: --%s nargs %d", kv->key, kv->data.integer);
    }
    OPAL_LIST_FOREACH(sub, &cmd->subcmds, orcm_cli_cmd_t) {
        orcm_cli_print_cmd(sub, pf2);
    }
    free(pf2);
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
