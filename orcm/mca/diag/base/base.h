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

#ifndef MCA_DIAG_BASE_H
#define MCA_DIAG_BASE_H

/*
 * includes
 */
#include "orcm_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/event/event.h"
#include "opal/dss/dss_types.h"
#include "opal/util/output.h"

#include "orcm/mca/diag/diag.h"
#include "orcm/mca/cfgi/cfgi_types.h"


BEGIN_C_DECLS

/*
 * MCA framework
 */
ORCM_DECLSPEC extern mca_base_framework_t orcm_diag_base_framework;
/*
 * Select an available component.
 */
ORCM_DECLSPEC int orcm_diag_base_select(void);

typedef struct {
    opal_object_t super;
    opal_event_t ev;
    orcm_scheduler_t *scheduler;
} orcm_scheduler_caddy_t;
OBJ_CLASS_DECLARATION(orcm_scheduler_caddy_t);

typedef struct {
    /* define an event base strictly for diagnostics - this
     * allows diagnostics to be run without interfering with
     * any other daemon functions
     */
    opal_event_base_t *ev_base;
    bool ev_active;
    /* list of active diagnostic plugins */
    opal_list_t modules;
    /* buffer to hold diagnostic results */
    opal_buffer_t bucket;
} orcm_diag_base_t;
ORCM_DECLSPEC extern orcm_diag_base_t orcm_diag_base;

/**
 * Select an diag component / module
 */
typedef struct {
    opal_list_item_t super;
    int pri;
    orcm_diag_base_module_t *module;
    mca_base_component_t *component;
} orcm_diag_active_module_t;
OBJ_CLASS_DECLARATION(orcm_diag_active_module_t);

/* base code stubs */
ORCM_DECLSPEC void orcm_diag_base_calibrate(void);

END_C_DECLS
#endif
