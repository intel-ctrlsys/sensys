/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "orcm/mca/pwrmgmt/base/base.h"


/*
 * Global variables
 */
static orcm_pwrmgmt_active_module_t* selected_module = NULL;

int orcm_pwrmgmt_base_init()
{
    orcm_pwrmgmt_active_module_t* active;

    for(int i = 0; i < orcm_pwrmgmt_base.modules.size; i++) {
        if (NULL == (active = (orcm_pwrmgmt_active_module_t*)opal_pointer_array_get_item(&orcm_pwrmgmt_base.modules, i))) {
            continue;
        }
        if (NULL != active->module->init) {
            active->module->init();
        }
    }

    return OPAL_SUCCESS;
}

void orcm_pwrmgmt_base_finalize()
{
    orcm_pwrmgmt_active_module_t* active;

    for(int i = 0; i < orcm_pwrmgmt_base.modules.size; i++) {
        if (NULL == (active = (orcm_pwrmgmt_active_module_t*)opal_pointer_array_get_item(&orcm_pwrmgmt_base.modules, i))) {
            continue;
        }
        if (NULL != active->module->finalize) {
            active->module->finalize();
        }
    }

    return;
}

void orcm_pwrmgmt_base_alloc_notify(orcm_alloc_t* alloc)
{
    bool rc;
    orcm_pwrmgmt_active_module_t* active;
    orcm_pwrmgmt_active_module_t* current;
    int32_t *requested_power_mode;
    int32_t max_priority = 0;

    rc = orte_get_attribute(&alloc->constraints, ORCM_PWRMGMT_POWER_MODE_KEY, (void**)&requested_power_mode, OPAL_INT32); 

    /* Either the ORCM_PWRMGMT_MODE key was not found or the returned
     * pointer was NULL. In either case we keep the orcm_pwrmgmt pointed
     * to the stubs and just return */    
    if(false == rc || NULL == requested_power_mode) {
        return;
    }

    /* No power management was explicitly requested so we just keep pointing
     * to the stub functions */ 
    if(*requested_power_mode == ORCM_PWRMGMT_MODE_NONE) {
        return;
    }

    for(int i = 0; i < orcm_pwrmgmt_base.modules.size; i++) {
        if (NULL == (active = (orcm_pwrmgmt_active_module_t*)opal_pointer_array_get_item(&orcm_pwrmgmt_base.modules, i))) {
            continue;
        }
        if (NULL != active->module->set_attributes) {
            if(OPAL_SUCCESS == active->module->set_attributes(alloc->id, &alloc->constraints)) {
                if (active->priority > max_priority) {
                    max_priority = active->priority;
                    current = active;
                }    
            }
        }
    }
    if(!ORTE_PROC_IS_SCHEDULER) {
        /* If we are changing components, tell the old one to shutdown */
        if(current != selected_module && NULL != selected_module) {
            if(NULL != selected_module->module->dealloc_notify) {
                selected_module->module->dealloc_notify(alloc);
            }
        }
        selected_module = current;
        orcm_pwrmgmt = *current->module;
        /* The dealloc call still needs to be handled bt the base framework */
        orcm_pwrmgmt.dealloc_notify = orcm_pwrmgmt_base_dealloc_notify;
    }
    if (NULL != selected_module->module->alloc_notify) {
        /* give this module a chance to operate on it */
        selected_module->module->alloc_notify(alloc);
    }
}

void orcm_pwrmgmt_base_dealloc_notify(orcm_alloc_t* alloc)
{
    if(NULL != selected_module) {
        selected_module->module->dealloc_notify(alloc);
    }
    /* set all pointers back to us */
    orcm_pwrmgmt = orcm_pwrmgmt_stubs;
    selected_module = NULL;
}

int orcm_pwrmgmt_base_set_attributes(orcm_session_id_t session, opal_list_t* attr)
{
    return ORTE_ERR_NOT_SUPPORTED;
}

int orcm_pwrmgmt_base_reset_attributes(orcm_session_id_t session, opal_list_t* attr)
{
    return ORTE_ERR_NOT_SUPPORTED;
}

int orcm_pwrmgmt_base_get_attributes(orcm_session_id_t session, opal_list_t* attr)
{
    return ORTE_ERR_NOT_SUPPORTED;
}

int orcm_pwrmgmt_base_get_current_power(orcm_session_id_t session, double *power)
{
    *power = -1.0;
    return ORTE_ERR_NOT_SUPPORTED;
}

