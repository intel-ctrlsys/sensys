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

#ifndef MCA_ANALYTICS_BASE_H
#define MCA_ANALYTICS_BASE_H

/*
 * includes
 */
#include "orcm_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/event/event.h"
#include "opal/dss/dss_types.h"
#include "opal/util/output.h"

#include "orcm/mca/analytics/analytics.h"


BEGIN_C_DECLS

/*
 * MCA framework
 */
ORCM_DECLSPEC extern mca_base_framework_t orcm_analytics_base_framework;
/*
 * Select an available component.
 */
ORCM_DECLSPEC int orcm_analytics_base_select(void);

typedef struct {
    /* define an event base strictly for analytics - this
     * allows analytics workflows to be run without interfering with
     * any other daemon functions
     */
    opal_event_base_t *ev_base;
    /* list of active analytics plugins */
    opal_list_t modules;
    /* buffer to hold analytics results */
    opal_buffer_t bucket;
} orcm_analytics_base_t;
ORCM_DECLSPEC extern orcm_analytics_base_t orcm_analytics_base;

/**
 * Select an analytics component / module
 */
typedef struct {
    opal_list_item_t super;
    int pri;
    orcm_analytics_base_module_t *module;
    mca_base_component_t *component;
} orcm_analytics_active_module_t;
OBJ_CLASS_DECLARATION(orcm_analytics_active_module_t);

/* base code stubs */

END_C_DECLS
#endif
