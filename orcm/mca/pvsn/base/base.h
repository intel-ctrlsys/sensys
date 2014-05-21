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

#ifndef MCA_PVSN_BASE_H
#define MCA_PVSN_BASE_H

/*
 * includes
 */
#include "orcm_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/event/event.h"
#include "opal/dss/dss_types.h"
#include "opal/util/output.h"

#include "orcm/mca/pvsn/pvsn.h"
#include "orcm/mca/cfgi/cfgi_types.h"


BEGIN_C_DECLS

/*
 * MCA framework
 */
ORCM_DECLSPEC extern mca_base_framework_t orcm_pvsn_base_framework;
/*
 * Select an available component.
 */
ORCM_DECLSPEC int orcm_pvsn_base_select(void);

typedef struct {
    /* define an event base strictly for provisioning - this
     * allows provisioing to be done without interfering with
     * any other daemon functions
     */
    opal_event_base_t *ev_base;
    bool ev_active;
} orcm_pvsn_base_t;
ORCM_DECLSPEC extern orcm_pvsn_base_t orcm_pvsn_base;

END_C_DECLS
#endif
