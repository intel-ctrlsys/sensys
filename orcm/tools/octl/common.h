/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved
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

BEGIN_C_DECLS

int orcm_octl_resource_status(char **argv);
int orcm_octl_resource_add(char **argv);
int orcm_octl_resource_remove(char **argv);
int orcm_octl_resource_drain(char **argv);
int orcm_octl_queue_status(char **argv);
int orcm_octl_queue_policy(char **argv);
int orcm_octl_queue_define(char **argv);
int orcm_octl_queue_add(char **argv);
int orcm_octl_queue_remove(char **argv);
int orcm_octl_queue_acl(char **argv);
int orcm_octl_queue_priority(char **argv);
int orcm_octl_session_status(char **argv);
int orcm_octl_session_cancel(char **argv);
int orcm_octl_diag_cpu(char **argv);
int orcm_octl_diag_mem(char **argv);

END_C_DECLS

#endif /* ORCM_OCTL_COMMON_H */
