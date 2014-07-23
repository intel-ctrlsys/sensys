/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCA_ANALYTICS_TYPES_H
#define MCA_ANALYTICS_TYPES_H

/*
 * includes
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h> /* for time_t */
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h> /* for uid_t, gid_t */
#endif

#include "opal/class/opal_object.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/class/opal_list.h"
#include "opal/mca/event/event.h"

BEGIN_C_DECLS

/* define a workflow "step" object - this object
 * specifies what operation is to be performed
 * on the input */
typedef struct {
    opal_list_item_t super;
    opal_value_array_t attributes;
    char *analytic;
    struct orcm_analytics_base_module_t *mod;
} orcm_workflow_step_t;
OBJ_CLASS_DECLARATION(orcm_workflow_step_t);

/* define a workflow object */
typedef struct {
    opal_object_t super;
    char *name;
    int workflow_id;
    opal_list_t steps;
    opal_event_base_t *ev_base;
} orcm_workflow_t;
OBJ_CLASS_DECLARATION(orcm_workflow_t);

END_C_DECLS

#endif
