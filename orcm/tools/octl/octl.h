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
    // drain subcommand
    { { "resource", NULL }, "resume", 0, 1, "Resource Resume" },

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
    //set subcommand
    { { "session", NULL }, "set", 0, 0, "Set Session Power Policy" },
    // budget subcommand
    { { "session", "set", NULL }, "budget", 0, 2, "Set Session Power Budget" },
    //get subcommand
    { { "session", NULL }, "get", 0, 0, "Get Session Power Policy" },
    // mode subcommand
    { { "session", "set", NULL }, "mode", 0, 2, "Set Session Power Mode" },
    // window subcommand
    { { "session", "set", NULL }, "window", 0, 2, "Set Session Power Window" },
    // overage subcommand
    { { "session", "set", NULL }, "overage", 0, 2, "Set Session Power Overage Limit" },
    // underage subcommand
    { { "session", "set", NULL }, "underage", 0, 2, "Set Session Power Underage Limit" },
    // overage_time subcommand
    { { "session", "set", NULL }, "overage_time", 0, 2, "Set Session Power Overage Time Limit" },
    // underage_time subcommand
    { { "session", "set", NULL }, "underage_time", 0, 2, "Set Session Power Underage Time Limit" },
    // freq subcommand
    { { "session", "set", NULL }, "frequency", 0, 2, "Set Session Manual Frequency" },
    // strict subcommand
    { { "session", "set", NULL }, "strict", 0, 2, "Set Session Strictness Policy For Frequency Settings" },
    // budget subcommand
    { { "session", "get", NULL }, "budget", 0, 1, "Get Session Power Budget" },
    // mode subcommand
    { { "session", "get", NULL }, "mode", 0, 1, "Get Session Power Mode" },
    // modes subcommand
    { { "session", "get", NULL }, "modes", 0, 0, "Get List of Power Modes" },
    // window subcommand
    { { "session", "get", NULL }, "window", 0, 1, "Get Session Power Window" },
    // overage subcommand
    { { "session", "get", NULL }, "overage", 0, 1, "Get Session Power Overage Limit" },
    // underage subcommand
    { { "session", "get", NULL }, "underage", 0, 1, "Get Session Power Underage Limit" },
    // overage_time subcommand
    { { "session", "get", NULL }, "overage_time", 0, 1, "Get Session Power Overage Time Limit" },
    // underage_time subcommand
    { { "session", "get", NULL }, "underage_time", 0, 1, "Get Session Power Underage Time Limit" },
    // freq subcommand
    { { "session", "get", NULL }, "frequency", 0, 1, "Get Session Manual Frequency" },
    // strict subcommand
    { { "session", "get", NULL }, "strict", 0, 1, "Get Session Strictness Policy For Frequency Settings" },


    /****** diag command ******/
    { { NULL }, "diag", 0, 0, "Diagnostics" },
    // cpu subcommand
    { { "diag", NULL }, "cpu", 0, 1, "CPU diagnostics" },
    // eth subcommand
    { { "diag", NULL }, "eth", 0, 1, "Ethernet diagnostics" },
    // mem subcommand
    { { "diag", NULL }, "mem", 0, 1, "Memory diagnostics" },

    /****** power command ******/
    { { NULL }, "power", 0, 0, "Global Power Policy" },
    { { "power", NULL }, "set", 0, 0, "Set Power Policy" },
    // budget subcommand
    { { "power", "set", NULL }, "budget", 0, 1, "Set Global Power Budget" },
    // mode subcommand
    { { "power", "set", NULL }, "mode", 0, 1, "Set Default Power Mode" },
    // window subcommand
    { { "power", "set", NULL }, "window", 0, 1, "Set Default Power Window" },
    // overage subcommand
    { { "power", "set", NULL }, "overage", 0, 1, "Set Default Power Overage Limit" },
    // underage subcommand
    { { "power", "set", NULL }, "underage", 0, 1, "Set Default Power Underage Limit" },
    // overage_time subcommand
    { { "power", "set", NULL }, "overage_time", 0, 1, "Set Default Power Overage Time Limit" },
    // underage_time subcommand
    { { "power", "set", NULL }, "underage_time", 0, 1, "Set Default Power Underage Time Limit" },
    // freq subcommand
    { { "power", "set", NULL }, "frequency", 0, 1, "Set Default Manual Frequency" },
    // strict subcommand
    { { "power", "set", NULL }, "strict", 0, 1, "Set Strictness Policy For Frequency Settings" },
    //get subcommand
    { { "power", NULL }, "get", 0, 0, "Get Power Policy" },
    // budget subcommand
    { { "power", "get", NULL }, "budget", 0, 0, "Get Global Power Budget" },
    // mode subcommand
    { { "power", "get", NULL }, "mode", 0, 0, "Get Default Power Mode" },
    // modes subcommand
    { { "power", "get", NULL }, "modes", 0, 0, "Get List of Power Modes" },
    // window subcommand
    { { "power", "get", NULL }, "window", 0, 0, "Get Default Power Window" },
    // overage subcommand
    { { "power", "get", NULL }, "overage", 0, 0, "Get Default Power Overage Limit" },
    // underage subcommand
    { { "power", "get", NULL }, "underage", 0, 0, "Get Default Power Underage Limit" },
    // overage_time subcommand
    { { "power", "get", NULL }, "overage_time", 0, 0, "Get Default Power Overage Time Limit" },
    // underage_time subcommand
    { { "power", "get", NULL }, "underage_time", 0, 0, "Get Default Power Underage Time Limit" },
    // freq subcommand
    { { "power", "get", NULL }, "frequency", 0, 0, "Get Default Manual Frequency" },
    // strict subcommand
    { { "power", "get", NULL }, "strict", 0, 0, "Get Strictness Policy For Frequency Settings" },

    /* End of list */
    { { NULL }, NULL, 0, 0, NULL }
};

/* list of unique command names from above
 * if you add a command above make sure to append any new strings to this list
 * this is used for ease of conversion of commands to array offset for
 * switch statement
 * NB: Order matters here, so add new ones to the end before the NULL! */
const char *orcm_octl_commands[] = { "resource",          //0
                                     "queue",             //1
                                     "session",           //2
                                     "diag",              //3
                                     "status",            //4
                                     "add",               //5
                                     "remove",            //6
                                     "drain",             //7
                                     "policy",            //8
                                     "define",            //9
                                     "acl",               //10
                                     "priority",          //11
                                     "cancel",            //12
                                     "cpu",               //13
                                     "mem",               //14
                                     "power",             //15
                                     "set",               //16
                                     "get",               //17
                                     "resume",            //18
                                     "eth",               //19
                                     "budget",            //20
                                     "mode",              //21
                                     "window",            //22
                                     "overage",           //23
                                     "underage",          //24
                                     "overage_time",      //25
                                     "underage_time",     //26
                                     "frequency",         //27
                                     "modes",             //28
                                     "strict",            //29
                                     "\0" };

END_C_DECLS

#endif /* ORCM_OCTL_H */
