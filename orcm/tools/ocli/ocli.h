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

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdio.h>

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
#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"

#include "orcm/runtime/runtime.h"
#include "orcm/util/cli.h"

BEGIN_C_DECLS

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
    { { "session", NULL }, "status", 0, 1, "Session Status" },

    /* End of list */
    { { NULL }, NULL, 0, 0, NULL }
};

int orcm_ocli_resource_status(char **argv);
int orcm_ocli_resource_availability(char **argv);
int orcm_ocli_queue_status(char **argv);
int orcm_ocli_queue_policy(char **argv);
int orcm_ocli_session_status(char **argv);

END_C_DECLS

#endif /* ORCM_OCLI_H */
