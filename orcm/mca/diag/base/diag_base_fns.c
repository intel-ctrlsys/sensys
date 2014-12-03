/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include "opal/mca/mca.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"

#include "orte/types.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/name_fns.h"
#include "orte/util/regex.h"
#include "orte/util/show_help.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/diag/base/base.h"

void orcm_diag_base_calibrate(void)
{
    orcm_diag_active_module_t *mod;
    
    OPAL_LIST_FOREACH(mod, &orcm_diag_base.modules, orcm_diag_active_module_t) {
        if (NULL != mod->module->calibrate) {
            opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                                "%s diag:base: calibrating component %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                mod->component->mca_component_name);
            mod->module->calibrate();
        }
    }
}

void orcm_diag_base_activate(char *dname, bool want_result, orte_process_name_t *sender, opal_buffer_t *buf)
{
    orcm_diag_active_module_t *mod;
    orcm_diag_caddy_t *caddy;
    
    /* if no modules are available, then there is nothing to do */
    if (opal_list_is_empty(&orcm_diag_base.modules)) {
        return;
    }
    
    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                        "%s diag:base: activating diag %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), dname);
    
    /* find the specified module  */
    OPAL_LIST_FOREACH(mod, &orcm_diag_base.modules, orcm_diag_active_module_t) {
        if (0 == strcmp(dname, mod->component->mca_component_name)) {
            if (NULL != mod->module->run) {
                caddy = OBJ_NEW(orcm_diag_caddy_t);
                caddy->want_result = want_result;
                caddy->requester = sender;
                /* unpack options from buffer */
                opal_event_set(orcm_diag_base.ev_base, &caddy->ev, -1,
                               OPAL_EV_WRITE, mod->module->run, caddy);
                opal_event_set_priority(&caddy->ev, mod->pri);
                opal_event_active(&caddy->ev, OPAL_EV_WRITE, 1);
            }
            return;
        }
    }
}

void orcm_diag_base_log(char *dname, opal_buffer_t *buf)
{
    orcm_diag_active_module_t *mod;
    
    /* if no modules are available, then there is nothing to do */
    if (opal_list_is_empty(&orcm_diag_base.modules)) {
        return;
    }
    
    if (NULL == dname || orcm_diag_base.dbhandle < 0) {
        /* nothing we can do */
        return;
    }
    
    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                        "%s diag:base: logging diag %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), dname);
    
    /* find the specified module  */
    OPAL_LIST_FOREACH(mod, &orcm_diag_base.modules, orcm_diag_active_module_t) {
        if (0 == strcmp(dname, mod->component->mca_component_name)) {
            if (NULL != mod->module->log) {
                mod->module->log(buf);
            }
            return;
        }
    }
}
