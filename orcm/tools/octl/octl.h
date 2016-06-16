/*
 * Copyright (c) 2014-2016 Intel, Inc. All rights reserved
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_OCTL_H
#define ORCM_OCTL_H

#include "orcm/util/cli.h"
#include <wordexp.h>

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

    /****** sensor command ******/
    { { NULL }, "sensor", 0, 0, "sensor" },
    // sensor set
    { { "sensor", NULL }, "set", 0, 0, "Set Sensor Commands" },
    // sensor policy subcommand
    { { "sensor", "set", NULL }, "sample-rate", 0, 3, "Set Sensor Sample Rate: set sample-rate "
    "<sensor-name> <sample-rate> <node-name>" },
    { { "sensor", NULL }, "get", 0, 0, "Get Sensor Commands" },
    // sensor policy subcommand
    { { "sensor", "get", NULL }, "sample-rate", 0, 2, "Get Sensor Sample Rate: get sample-rate "
    "<sensor-name> <node-name>" },
    // sensor inventory subcommand
    { { "sensor", "get", NULL }, "inventory", 0, 2, "Get the current sensor inventory for a specified "
    "node: get inventory <node-name> [<filter>]" },
    // sensor enable sampling subcommand
    { { "sensor", NULL }, "enable", 0, 2, "Enable sampling for the current datagroup or sensor for "
    "a node-list: enable <node-list> <datagroup|\"all\"[:{sensor_label|\"all\"}]>" },
    // sensor disable sampling subcommand
    { { "sensor", NULL }, "disable", 0, 2, "Disable sampling for the current datagroup or sensor "
    "for a node-list: disable <node-list> <datagroup|\"all\"[:{sensor_label|\"all\"}]>" },
    // sensor reset sampling subcommand
    { { "sensor", NULL }, "reset", 0, 2, "Reset sampling to service load defaults for the current "
    "datagroup or sensor for a node-list: reset <node-list> <datagroup|\"all\"[:{sensor_label|\"all\"}]>" },
    // sensor storage policy commands
    { { "sensor", NULL }, "store", 0, 0, "Sensor store Commands" },
    { { "sensor", "store", NULL }, "raw_data", 0, 1, "store raw_data aggregators" },
    { { "sensor", "store", NULL }, "event_data", 0, 1, "store event_data aggregators" },
    { { "sensor", "store", NULL }, "all", 0, 1, "store all aggregators" },
    { { "sensor", "store", NULL }, "none", 0, 1, "store none aggregators" },

    /****** notifier commands ******/
    { { NULL }, "notifier", 0, 0, "notifier" },
    // sensor set
    { { "notifier", NULL }, "set", 0, 0, "Set Notifier Commands" },
    // notifier set policy subcommand
    { { "notifier", "set", NULL }, "policy", 0, 3, "notifier set policy <severity> <action> <nodelist>" },
    // notifier set smtp subcommand
    { { "notifier", "set", NULL }, "smtp-policy", 0, 3, "notifier set smtp-policy <key> <value> <nodelist>" },
    { { "notifier", NULL }, "get", 0, 0, "Get Notifier Commands" },
    // notifier get policy subcommand
    { { "notifier", "get", NULL }, "policy", 0, 2, "notifier get policy <nodelist>" },
    // notifier get smtp subcommand
    { { "notifier", "get", NULL }, "smtp-policy", 0, 2, "notifier get smtp-policy <nodelist>" },

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

    /****** logical group command ******/
    { { NULL }, "grouping", 0, 0, "Logical Grouping Information" },
    { { "grouping", NULL }, "add", 0, 2, "Add a tag-value pair to the groupings: add <tag> "
    "<node-regex>" },
    { { "grouping", NULL }, "remove", 0, 2, "Remove a tag-value pair to the groupings: "
    "remove <tag> <node-regex>" },
    { { "grouping", NULL }, "list", 0, 2, "List tag-value pair. Use * for either tag or "
    "node: list <tag> <node-regex>" },

    /****** workflow commands ******/
    { { NULL }, "workflow", 0, 0, "Workflow information" },
    { { "workflow", NULL }, "add", 0, 1, "workflow add file.txt [aggregators]" },
    { { "workflow", NULL }, "remove", 0, 2, "workflow remove aggregators workflow_name workflow_id" },
    { { "workflow", NULL }, "list", 0, 0, "workflow list aggregators" },

    /****** Query commands ******/
    { { NULL}, "query", 0, 0, "Query data from DB" },
    { { "query", NULL}, "history", 0, 0, "Returns all the data logged by the provided nodes during "
    "specified time:" },
    { { "query", NULL}, "", 0, 0, "query history [start-date [end-date]] <nodelist>" },
    { { "query", NULL}, "sensor", 0, 0, "Returns the logged data corresponding to the given sensor, "
    "time and node list:" },
    { { "query", NULL}, "", 0, 0, "query sensor <sensor-list> [start-date [end-date]] "
    "<upper-bound lower-bound> [node-list]" },
    { { "query", NULL}, "log", 0, 0, "Returns the logged data coming from the syslog to the given "
    "nodes and search word:" },
    { { "query", NULL}, "", 0, 0, "query log <text-in-log> [start-date [end-date]] [nodelist] "
    "<nodelist>" },
    { { "query", NULL}, "idle", 0, 0, "Returns the nodes in those that has been idle for the given "
    "time or more:" },
    { { "query", NULL}, "", 0, 0, "query idle [interval] <node-list>" },
    { { "query", NULL}, "event", 0, 1, "Event data/sensor-data commands" },
    { { "query", "event", NULL}, "data", 0, 0, "Returns events from database" },
    { { "query", "event", NULL}, "", 0, 0, "query event data [start-date [end-date]] <node-list>" },
    { { "query", "event", NULL}, "sensor-data", 0, 0, "Returns the sensor data around an event" },
    { { "query", "event", NULL}, "", 0, 0, "query event sensor-data <event-id> [interval] <sensor-list> [node-list]" },
    { { "query", NULL}, "node", 0, 0, "Node status command." },
    { { "query", "node", NULL}, "status", 0, 0, "Returns the status logged in the data base for the "
    "nodes in the database: query node status <node-list>" },

    /****** Chassis ID commands ******/
    { { NULL}, "chassis-id", 0, 0, "Enable/Disable chassis identify LED." },
    { { "chassis-id", NULL}, "state", 0, 0, "Shows the state of the chassis identify LED." },
    { { "chassis-id", NULL}, "", 0, 0, "chassis-id disable <node>." },
    { { "chassis-id", NULL}, "enable", 0, 0, "Turn chassis identify LED ON for <seconds> seconds on the specified <node>. "
    "If no <seconds> are specified, the chassis will turn ON indefinitely." },
    { { "chassis-id", NULL}, "", 0, 0, "chassis-id enable <seconds> <node>." },
    { { "chassis-id", NULL}, "disable", 0, 0, "Turn chassis identify LED OFF." },
    { { "chassis-id", NULL}, "", 0, 0, "chassis-id disable <node>." },

    /* quit command */
    { { NULL }, "quit", 0, 0, "Exit the shell" },

    /* End of list */
    { { NULL }, NULL, 0, 0, NULL }
};

/* The enumerated numbers of the command tokens
 * that are defined in the octl_tokens.def file.
 * The enums have the format of "cmd_token" */
typedef enum {
#define TOK(X) cmd_##X,
#define TOK_CONCAT(X, Y) cmd_##X##_##Y,
#include "octl_tokens.def"
    NUM_TOKENS,
    cmd_null
} orcm_octl_cmd_enums;

/* list of unique command strings from the tokens
 * defined in the octl_tokens.def file */
const char* orcm_octl_cmds[] = {
#define TOK(X) #X,
#define TOK_CONCAT(X, Y) #X"-"#Y,
#include "octl_tokens.def"
    "\0"
};

END_C_DECLS

#endif /* ORCM_OCTL_H */
