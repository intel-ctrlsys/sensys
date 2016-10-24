/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2015-2016      Intel Corporation.  All rights reserved.
 *
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_DISPATCH_BASE_H
#define MCA_DISPATCH_BASE_H

/*
 * includes
 */
#include "orcm_config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */

#include "opal/class/opal_list.h"
#include "opal/mca/base/base.h"
#include "opal/mca/event/event.h"

#include "orte/mca/notifier/notifier.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/dispatch/dispatch.h"

BEGIN_C_DECLS

/*
 * MCA Framework
 */

typedef struct {
    opal_list_t actives;
    int sensor_db_commit_rate;
} orcm_dispatch_base_t;

typedef struct {
    opal_list_item_t super;
    int priority;
    mca_base_component_t *component;
    orcm_dispatch_base_module_t *module;
} orcm_dispatch_active_module_t;
OBJ_CLASS_DECLARATION(orcm_dispatch_active_module_t);

ORCM_DECLSPEC int orcm_dispatch_base_select(void);
/*This function is meant to be used by test code only*/
ORCM_DECLSPEC void orcm_dispatch_base_reset_selected(void);

ORCM_DECLSPEC extern mca_base_framework_t orcm_dispatch_base_framework;
ORCM_DECLSPEC extern orcm_dispatch_base_t orcm_dispatch_base;

/* stub functions */
ORCM_DECLSPEC void orcm_dispatch_base_event(int sd, short args, void *cbdata);
ORCM_DECLSPEC const char* orcm_dispatch_base_print_type(int t);
ORCM_DECLSPEC const char* orcm_dispatch_base_print_severity(int s);

ORCM_DECLSPEC orte_notifier_severity_t orcm_dispatch_base_convert_ras_severity_to_orte_notifier(int severity);

END_C_DECLS
#endif
