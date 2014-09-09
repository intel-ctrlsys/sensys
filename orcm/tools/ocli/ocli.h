/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_OCLI_H
#define ORCM_OCLI_H

#include "orcm/util/cli.h"

BEGIN_C_DECLS

/* initialize cli command tree */
/* * list of parents
 * * command name
 * * is this an option?
 * * number of arguments
 * * help string
 */
static orcm_cli_init_t cli_init[] = {
    /****** resource command ******/
    { { NULL }, "resource", 0, 0, "Resource Information" },
    // status subcommand
    { { "resource", NULL }, "status", 0, 0, "Resource Status" },
    // availability subcommand
    { { "resource", NULL }, "availability", 0, 0, "Resource Availability" },

    /****** queue command ******/
    { { NULL }, "queue", 0, 0, "Queue Information" },
    // status subcommand
    { { "queue", NULL }, "status", 0, 0, "Queue Status" },
    // status subcommand
    { { "queue", NULL }, "policy", 0, 0, "Queue Policies" },

    /****** queue command ******/
    { { NULL }, "session", 0, 0, "Session Management" },
    // status subcommand
    { { "session", NULL }, "status", 0, 0, "Session Status" },
    // cancel subcommand
    { { "session", NULL }, "cancel", 0, 1, "Session Cancel [arg: session id]" },

    /* End of list */
    { { NULL }, NULL, 0, 0, NULL }
};

/* list of unique command names from above
 * if you add a command above make sure to append any new strings to this list
 * this is used for ease of conversion of commands to array offset for
 * switch statement
 * NB: Order matters here, so add new ones to the end! */
const char *orcm_ocli_commands[] = { "resource", "queue", "session", "status",
                                     "availability", "policy", "cancel", "\0" };

END_C_DECLS

#endif /* ORCM_OCLI_H */
