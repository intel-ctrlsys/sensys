/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_OCTL_H
#define ORCM_OCTL_H

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
    // addition subcommand
    { { "resource", NULL }, "add", 0, 1, "Resource Addition" },
    // removal subcommand
    { { "resource", NULL }, "remove", 0, 1, "Resource Removal" },
    // drain subcommand
    { { "resource", NULL }, "drain", 0, 1, "Resource Drain" },

    /****** queue command ******/
    { { NULL }, "queue", 0, 0, "Queue Information" },
    // status subcommand
    { { "queue", NULL }, "status", 0, 0, "Queue Status" },
    // policy subcommand
    { { "queue", NULL }, "policy", 0, 0, "Queue Policies" },
    // define subcommand
    { { "queue", NULL }, "define", 0, 2, "Queue Definintion" },
    // addition subcommand
    { { "queue", NULL }, "add", 0, 2, "Add Resource to Queue" },
    // remmoval subcommand
    { { "queue", NULL }, "remove", 0, 2, "Remove Resource from Queue" },
    // acl subcommand
    { { "queue", NULL }, "acl", 0, 2, "Queue Access Control" },
    // priority subcommand
    { { "queue", NULL }, "priority", 0, 2, "Queue Priority Modification" },

    /****** session command ******/
    { { NULL }, "session", 0, 0, "Session Management" },
    // status subcommand
    { { "session", NULL }, "status", 0, 0, "Session Status" },
    // cancel subcommand
    { { "session", NULL }, "cancel", 0, 1, "Session Cancel [arg: session id]" },
    
    /****** diag command ******/
    { { NULL }, "diag", 0, 0, "Diagnostics" },
    // cpu subcommand
    { { "diag", NULL }, "cpu", 0, 1, "CPU diagnostics" },
    // mem subcommand
    { { "diag", NULL }, "mem", 0, 1, "Memory diagnostics" },

    /* End of list */
    { { NULL }, NULL, 0, 0, NULL }
};

/* list of unique command names from above
 * if you add a command above make sure to append any new strings to this list
 * this is used for ease of conversion of commands to array offset for
 * switch statement
 * NB: Order matters here, so add new ones to the end! */
const char *orcm_octl_commands[] = { "resource", //0
                                     "queue",    //1
                                     "session",  //2
                                     "diag",     //3
                                     "status",   //4
                                     "add",      //5
                                     "remove",   //6
                                     "drain",    //7
                                     "policy",   //8
                                     "define",   //9
                                     "acl",      //10
                                     "priority", //11
                                     "cancel",   //12
                                     "cpu",      //13
                                     "mem",      //14
                                     "\0" };

END_C_DECLS

#endif /* ORCM_OCTL_H */
