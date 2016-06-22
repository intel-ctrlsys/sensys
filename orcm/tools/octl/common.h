/*
 * Copyright (c) 2014-2016 Intel, Inc. All rights reserved
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_OCTL_COMMON_H
#define ORCM_OCTL_COMMON_H

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdio.h>
#include <ctype.h>

#include "opal/dss/dss.h"
#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/mca/event/event.h"
#include "opal/util/basename.h"
#include "opal/util/cmd_line.h"
#include "opal/util/output.h"
#include "opal/util/opal_environ.h"
#include "opal/util/show_help.h"
#include "opal/mca/base/base.h"
#include "opal/runtime/opal.h"
#if OPAL_ENABLE_FT_CR == 1
#include "opal/runtime/opal_cr.h"
#endif

#include "orte/runtime/orte_wait.h"
#include "orte/util/error_strings.h"
#include "orte/util/regex.h"
#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"

#include "orcm/runtime/runtime.h"
#include "orcm/mca/cfgi/base/base.h"
#include "orcm/version.h"

#define TAG  "octl:command-line:failure"
#define PACKERR  "internal buffer pack error"
#define UNPACKERR "internal buffer unpack error"

#define INVALID_USG 0
#define ILLEGAL_CMD 1

/* A timeout length when blocking
 * until something happens */
#define ORCM_OCTL_WAIT_TIMEOUT 10
#define ORCM_OCTL_WAIT_TIMEOUT_STREAM 600

BEGIN_C_DECLS

typedef enum {
    QUERY_EVENT_DATA,
    QUERY_EVENT_SNSR_DATA,
    QUERY_IDLE,
    QUERY_LOG,
    QUERY_NODE_STATUS,
    QUERY_HISTORY,
    QUERY_SENSOR,
    QUERY_SNSR_GET_INVENTORY,
} query_func_names;

int orcm_octl_resource_status(char **argv);
int orcm_octl_resource_add(char **argv);
int orcm_octl_resource_remove(char **argv);
int orcm_octl_resource_drain(char **argv);
int orcm_octl_resource_resume(char **argv);
int orcm_octl_session_status(char **argv);
int orcm_octl_session_cancel(char **argv);
int orcm_octl_session_set(int cmd, char **argv);
int orcm_octl_session_get(int cmd, char **argv);
int orcm_octl_diag_cpu(char **argv);
int orcm_octl_diag_eth(char **argv);
int orcm_octl_diag_mem(char **argv);
int orcm_octl_power_set(int cmd, char **argv);
int orcm_octl_power_get(int cmd, char **argv);
int orcm_octl_query_func(query_func_names func_name, int argc, char **argv);
int orcm_octl_sensor_sample_rate_set(int cmd, char **argv);
int orcm_octl_sensor_sample_rate_get(int cmd, char **argv);
int orcm_octl_sensor_inventory_get(int command, char** argv);
int orcm_octl_logical_group_add(int argc, char **argv);
int orcm_octl_logical_group_remove(int argc, char **argv);
int orcm_octl_logical_group_list(int argc, char **argv);
int orcm_octl_workflow_add(char **value);
int orcm_octl_workflow_remove(char **value);
int orcm_octl_workflow_list (char **value);
int orcm_octl_sensor_inventory_get(int command, char** argv);
int orcm_octl_sensor_change_sampling(int command, char** cmdlist);
int orcm_octl_get_notifier_policy(int cmd, char **argv);
int orcm_octl_set_notifier_policy(int cmd, char **argv);
int orcm_octl_get_notifier_smtp(int cmd, char **argv);
int orcm_octl_set_notifier_smtp(int cmd, char **argv);
int orcm_octl_chassis_id_off(char **argv);
int orcm_octl_chassis_id_on(char **argv);
int orcm_octl_chassis_id_state(char **argv);
int orcm_octl_sensor_store(int command, char** cmdlist);
void orcm_octl_error(char *label, ...);
void orcm_octl_info(char *label, ...);
void orcm_octl_usage(char *label, int error);
int orcm_octl_work(int argc, char *argv[]);
char **octl_split_argv(char *str, int delimiter);

END_C_DECLS

#endif /* ORCM_OCTL_COMMON_H */
