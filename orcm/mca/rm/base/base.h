/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_RM_BASE_H
#define MCA_RM_BASE_H

/*
 * includes
 */
#include "orcm_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/event/event.h"
#include "opal/dss/dss_types.h"
#include "opal/util/output.h"

#include "orcm/mca/rm/rm.h"


BEGIN_C_DECLS

/*
 * MCA framework
 */
ORCM_DECLSPEC extern mca_base_framework_t orcm_rm_base_framework;
/*
 * Select an available component.
 */
ORCM_DECLSPEC int orcm_rm_base_select(void);

typedef struct {
    /* flag that we just want to test */
    bool test_mode;
    /* define an event base strictly for resource manager - this
     * allows the resource manager to respond to requests for
     * information without interference with the
     * actual computation
     */
    opal_event_base_t *ev_base;
    bool ev_active;
    /* state machine */
    opal_list_t states;
    /* list of active resource manager plugins */
    opal_list_t active_modules;
    /* available nodes */
    opal_pointer_array_t nodes;
} orcm_rm_base_t;
ORCM_DECLSPEC extern orcm_rm_base_t orcm_rm_base;

/**
 * Select an rm component / module
 */
typedef struct {
    opal_list_item_t super;
    int pri;
    orcm_rm_base_module_t *module;
    mca_base_component_t *component;
} orcm_rm_base_active_module_t;
OBJ_CLASS_DECLARATION(orcm_rm_base_active_module_t);

/* start/stop base receive */
ORCM_DECLSPEC int orcm_rm_base_comm_start(void);
ORCM_DECLSPEC int orcm_rm_base_comm_stop(void);

END_C_DECLS
#endif
