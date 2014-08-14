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


int orcm_cli_get_cmd(char *prompt,
                     orcm_cli_t *cli,
                     char **cmd)
{
    char c;
    orcm_cli_cmd_t *c2, *c3;
    opal_value_t *kv;
    char **options;
    struct termios settings, initial_settings;
    char input[ORCM_MAX_CLI_LENGTH];
    int argc;
    bool space;
    size_t i, j, k;
    bool found;
    char **completions;

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
            found = false;
            space = false;
            argc = 0;
            /* init the search */
            options = NULL;
            if (0 == j) {
                /* if they are at the start of the command, then print
                 * out all allowed possibilities */
                putchar('\n');
                OPAL_LIST_FOREACH(c2, &cli->cmds, orcm_cli_cmd_t) {
                    opal_argv_append(&argc, &options, c2->cmd);
                }
                /* if there is nothing, then that's an error */
                if (0 == argc) {
                    BONK;
                    goto process;
                }
                if (1 == argc) {
                    /* only one possibility - output the rest of the characters
                     * followed by a space */
                    for (i=0; i < strlen(options[0]); i++) {
                        putchar(options[0][i]);
                        /* and add it to the cmd */
                        input[j++] = options[0][i];
                    }
                    putchar(' ');
                    input[j++] = ' ';
                    opal_argv_free(options);
                    break;
                }
                /* if there are multiple choices, then output them
                 * on the line below */
                putchar('\n');
                for (i=0; NULL != options[i]; i++) {
                    printf("%s  ", options[i]);
                }
                printf("\n%s> %s", prompt, input);
                opal_argv_free(options);
                found = true;
                break;
            }

            /* if we are partway thru the command, then we break the
             * command into its component parts so we can cycle across known
             * possibilities and see if any match the currently-input
             * set of characters. */
            options = opal_argv_split(input, ' ');
            /* find the base command */
            c3 = NULL;
            OPAL_LIST_FOREACH(c2, &cli->cmds, orcm_cli_cmd_t) {
                if (strlen(c2->cmd) < strlen(options[0])) {
                    continue;
                }
                if (0 == strncmp(c2->cmd, options[0], strlen(options[0]))) {
                    c3 = c2;
                    found = true;
                }
            }
            if (NULL == c3) {
                /* the input doesn't match any defined command - bonk */
                BONK;
                break;
            }

            /* if they only gave us the one command, then we can treat that now */
            if (1 == (argc = opal_argv_count(options))) {
                /* if they didn't give us the full command, then complete
                 * it for them */
                if (j < strlen(c3->cmd)) {
                    for (i=j; i < strlen(c3->cmd); i++) {
                        putchar(c3->cmd[i]);
                        input[j++] = c3->cmd[i];
                    }
                    putchar(' ');
                    input[j++] = ' ';
                    opal_argv_free(options);
                    found = true;
                    break;
                }
                /* they have given us one complete command, and are now
                 * asking for all its subcmds and options - so pass them
                 * along */
                if (0 == opal_list_get_size(&c3->options) &&
                    0 == opal_list_get_size(&c3->subcmds)) {
                    /* this command doesn't have any options or subcmds - bonk */
                    BONK;
                    break;
                }
                if (0 < opal_list_get_size(&c3->options)) {
                    printf("\nOPTIONS: ");
                    OPAL_LIST_FOREACH(kv, &c3->options, opal_value_t) {
                        printf("%s  ", kv->key);
                    }
                }
                if (0 < opal_list_get_size(&c3->subcmds)) {
                    printf("\n\t");
                    OPAL_LIST_FOREACH(c2, &c3->subcmds, orcm_cli_cmd_t) {
                        printf("%s  ", c2->cmd);
                    }
                }
                printf("\n%s> %s", prompt, input);
                found = true;
                break;
            }

            /* we have the top-level command in c3, so now walk
             * the remaining entries to find out where we are
             * in the tree */
            i = 1;
            while (i < (size_t)opal_argv_count(options)) {
                found = false;
                completions = NULL;
                /* check if this entry is the start of a subcommand */
                OPAL_LIST_FOREACH(c2, &c3->subcmds, orcm_cli_cmd_t) {
                    if (0 == strncmp(c2->cmd, options[i], strlen(options[i]))) {
                        /* got a match - there are no args required
                         * here, so just move to the next entry if
                         * this word is complete */
                        if (strlen(options[i]) == strlen(c2->cmd)) {
                            i++;
                            opal_argv_free(completions);
                        } else {
                            /* add this to our list of possible completions */
                            opal_argv_append_nosize(&completions, c2->cmd);
                        }
                        found = true;
                        break;
                    }
                }
                if (NULL != completions) {
                    /* if we have a list of possible completions, then we
                     * didn't find a completed subcommand - so see if we
                     * can complete it */
                    if (1 == opal_argv_count(completions)) {
                        /* only one option, so complete it for them */
                        for (k = strlen(options[i]); k < strlen(completions[0]); k++) {
                            putchar(completions[0][k]);
                            input[j++] = completions[0][k];
                        }
                        putchar(' ');
                        input[j++] = ' ';
                    } else {
                        /* if there are multiple completions, then print them out */
                        putchar('\n');
                        for (k=0; NULL != completions[k]; k++) {
                            printf("%s  ", completions[k]);
                        }
                        printf("\n%s> %s", prompt, input);
                    }
                    /* break out of the options loop */
                    i = opal_argv_count(options) + 1;
                    opal_argv_free(completions);
                    break;
                }
                /* if completions are NULL, then either we didn't find a matching
                 * subcommand, or we found the subcommand but it was complete. In the
                 * latter case, since i was incremented, we can just look at the next position */
                if (i < (size_t)opal_argv_count(options)) {
                    OPAL_LIST_FOREACH(kv, &c3->options, opal_value_t) {
                        if (0 == strncmp(kv->key, options[i], strlen(options[i]))) {
                            found = true;
                            if (strlen(kv->key) == strlen(options[i])) {
                                /* it is a complete option, so see if we have all
                                 * the arguments */
                                i += kv->data.integer;
                                if (argc < (int)i) {
                                    /* nope - we are missing some, but we cannot
                                     * autocomplete arguments */
                                    BONK;
                                    i = opal_argv_count(options) + 1;  // break out of the completion loop
                                } else {
                                    /* have all we need - move to the next option */
                                    i++;
                                    break;
                                }
                            } else {
                                /* add it to our possible completions */
                                opal_argv_append_nosize(&completions, kv->key);
                            }
                            break;
                        }
                    }
                    if (NULL != completions) {
                        /* if we have a list of possible completions, then we
                         * didn't find a completed option - so see if we
                         * can complete it */
                        if (1 == opal_argv_count(completions)) {
                            /* only one option, so complete it for them */
                            for (k = strlen(options[i]); k < strlen(completions[0]); k++) {
                                putchar(completions[0][k]);
                                input[j++] = completions[0][k];
                            }
                            putchar(' ');
                            input[j++] = ' ';
                            opal_argv_free(completions);
                            i = opal_argv_count(options) + 1;  // break out of the completion loop
                            break;
                        } else {
                            /* if there are multiple completions, then print them out */
                            putchar('\n');
                            for (k=0; NULL != completions[k]; k++) {
                                printf("%s  ", completions[k]);
                            }
                            printf("\n%s> %s", prompt, input);
                        }
                        found = true;
                        i = opal_argv_count(options) + 1;  // break out of the completion loop
                        opal_argv_free(completions);
                        break;
                    }
                }
                if (!found) {
                    /* no match was found */
                    break;
                }
            }
            /* if the last item in the list wasn't found, then that's an error */
            if (!found) {
                BONK;
            }
            if ((size_t)opal_argv_count(options) == i) {
                /* we found everything on the list, so they must want
                 * a suggestion of any remaining options */
                if (0 < opal_list_get_size(&c3->options)) {
                    printf("\nOPTIONS: ");
                    OPAL_LIST_FOREACH(kv, &c3->options, opal_value_t) {
                        printf("%s  ", kv->key);
                    }
                }
                if (0 < opal_list_get_size(&c3->subcmds)) {
                    printf("\n\t");
                    OPAL_LIST_FOREACH(c2, &c3->subcmds, orcm_cli_cmd_t) {
                        printf("%s  ", c2->cmd);
                    }
                }
                printf("\n%s> %s", prompt, input);
            }
            opal_argv_free(options);
            break;

        case ' ':
            /* it is always possible somebody just typed an extra
             * space here, so we should ignore it if they did */
            if (0 == j || space) {
                /* if they do it more than once, bonk them */
                BONK;
                break;
            }
            putchar(c);
            input[j++] = c;
            space = true;
            /* check for validity of the last input since it wasn't
             * entered via auto-completion */
            break;

        case '\b':
        case 0x7f:
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

        default:
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
        }
    }

 process:
    /* restore the initial settings */
    tcsetattr(STDIN_FILENO, TCSANOW, &initial_settings);

    /* return the assembled command */
    *cmd = strdup(input);

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
        opal_output(0, "\tOPTION: %s nargs %d", kv->key, kv->data.integer);
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
