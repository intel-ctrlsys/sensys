/*
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2013      Los Alamos National Security, LLC.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_PWRMGMT_BASE_H
#define MCA_PWRMGMT_BASE_H

/*
 * includes
 */
#include "orcm_config.h"

#include "opal/class/opal_list.h"
#include "opal/mca/base/base.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */

#include "opal/class/opal_pointer_array.h"
#include "opal/mca/event/event.h"
#include "opal/threads/threads.h"

#include "orte/runtime/orte_globals.h"

#include "orcm/mca/pwrmgmt/pwrmgmt.h"

BEGIN_C_DECLS

/*
 * MCA Framework
 */

typedef struct {
    opal_pointer_array_t modules;
} orcm_pwrmgmt_base_t;

typedef struct {
    opal_object_t super;
    int priority;
    orcm_pwrmgmt_base_component_t *component;
    orcm_pwrmgmt_base_API_module_t *module;
} orcm_pwrmgmt_active_module_t;
OBJ_CLASS_DECLARATION(orcm_pwrmgmt_active_module_t);

ORCM_DECLSPEC extern mca_base_framework_t orcm_pwrmgmt_base_framework;
ORCM_DECLSPEC extern orcm_pwrmgmt_base_t orcm_pwrmgmt_base;
ORCM_DECLSPEC extern orcm_pwrmgmt_base_API_module_t orcm_pwrmgmt_stubs;
ORCM_DECLSPEC int orcm_pwrmgmt_base_init(void);
ORCM_DECLSPEC void orcm_pwrmgmt_base_finalize(void);
ORCM_DECLSPEC int orcm_pwrmgmt_base_component_select(orcm_session_id_t session, opal_list_t* attr);
ORCM_DECLSPEC int orcm_pwrmgmt_base_alloc_notify(orcm_alloc_t* alloc);
ORCM_DECLSPEC void orcm_pwrmgmt_base_dealloc_notify(orcm_alloc_t* alloc);
ORCM_DECLSPEC int orcm_pwrmgmt_base_set_attributes(orcm_session_id_t session, opal_list_t* attr);
ORCM_DECLSPEC int orcm_pwrmgmt_base_reset_attributes(orcm_session_id_t session, opal_list_t* attr);
ORCM_DECLSPEC int orcm_pwrmgmt_base_get_attributes(orcm_session_id_t session, opal_list_t* attr);
ORCM_DECLSPEC int orcm_pwrmgmt_base_select(void);

END_C_DECLS
#endif
