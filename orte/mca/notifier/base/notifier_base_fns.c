/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2008-2015 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2014-2015 Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orte_config.h"
#include "orte/constants.h"

#include "opal/util/argv.h"

#include "orte/util/attr.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"

#include "orte/mca/notifier/base/base.h"

#define NOTIFIER_SAFE_FREE(x) if(NULL != x) free((void*)x); x=NULL

static void orte_notifier_base_identify_modules(char ***modules,
                                                orte_notifier_request_t *req);

int orte_notifier_base_set_config(char *action, opal_value_t *kv)
{
    char **modules = NULL;
    orte_notifier_active_module_t *imod;
    int i = 0;

    /* if no modules are active, then there is nothing to do */
    if (0 == opal_list_get_size(&orte_notifier_base.modules)) {
        return ORTE_ERROR;
    }

    if (NULL == action || NULL == kv) {
        return ORTE_ERROR;
    }

    modules = opal_argv_split(action, ',');

    /* no modules selected then nothing to do */
    if (NULL == modules) {
        return ORTE_ERROR;
    }

    for (i=0; NULL != modules[i]; i++) {
        OPAL_LIST_FOREACH(imod, &orte_notifier_base.modules, orte_notifier_active_module_t) {
            if (NULL != imod->module->log &&
                0 == strcmp(imod->component->base_version.mca_component_name, modules[i])) {
                imod->module->set_config(kv);
            }
        }
    }
    opal_argv_free(modules);

    return ORTE_SUCCESS;
}

int orte_notifier_base_get_config(char *action, opal_list_t **list)
{
    char **modules = NULL;
    orte_notifier_active_module_t *imod;
    int i = 0;

    /* if no modules are active, then there is nothing to do */
    if (0 == opal_list_get_size(&orte_notifier_base.modules)) {
        return ORTE_ERROR;
    }

    if (NULL == action || NULL == list) {
        return ORTE_ERROR;
    }

    modules = opal_argv_split(action, ',');

    /* no modules selected then nothing to do */
    if (NULL == modules) {
        return ORTE_ERROR;
    }

    for (i=0; NULL != modules[i]; i++) {
        OPAL_LIST_FOREACH(imod, &orte_notifier_base.modules, orte_notifier_active_module_t) {
            if (NULL != imod->module->log &&
                0 == strcmp(imod->component->base_version.mca_component_name, modules[i])) {
                imod->module->get_config(list);
            }
        }
    }
    opal_argv_free(modules);

    return ORTE_SUCCESS;
}

void orte_notifier_base_log(int sd, short args, void *cbdata)
{
    orte_notifier_request_t *req = (orte_notifier_request_t*)cbdata;
    char **modules = NULL;
    orte_notifier_active_module_t *imod;
    int i;

    /* if no modules are active, then there is nothing to do */
    if (0 == opal_list_get_size(&orte_notifier_base.modules)) {
        return;
    }

    /* check if the severity is >= severity level set for
     * reporting - note that the severity enum value goes up
     * as severity goes down */
    if (orte_notifier_base.severity_level < req->severity ) {
        return;
    }

    orte_notifier_base_identify_modules(&modules, req);

    /* no modules selected then nothing to do */
    if (NULL == modules) {
        return;
    }

    for (i=0; NULL != modules[i]; i++) {
        OPAL_LIST_FOREACH(imod, &orte_notifier_base.modules, orte_notifier_active_module_t) {
            if (NULL != imod->module->log &&
                0 == strcmp(imod->component->base_version.mca_component_name, modules[i]))
                imod->module->log(req);
        }
    }
    opal_argv_free(modules);
}

void orte_notifier_base_event(int sd, short args, void *cbdata)
{
    orte_notifier_request_t *req = (orte_notifier_request_t*)cbdata;
    char **modules = NULL;
    orte_notifier_active_module_t *imod;
    int i;

    /* if no modules are active, then there is nothing to do */
    if (0 == opal_list_get_size(&orte_notifier_base.modules)) {
        return;
    }

    /* check if the severity is >= severity level set for
     * reporting - note that the severity enum value goes up
     * as severity goes down */
    if (orte_notifier_base.severity_level < req->severity ) {
        return;
    }

    orte_notifier_base_identify_modules(&modules, req);

    /* no modules selected then nothing to do */
    if (NULL == modules) {
        return;
    }

    for (i=0; NULL != modules[i]; i++) {
        OPAL_LIST_FOREACH(imod, &orte_notifier_base.modules, orte_notifier_active_module_t) {
            if (NULL != imod->module->log &&
                0 == strcmp(imod->component->base_version.mca_component_name, modules[i]))
                imod->module->event(req);
        }
    }
    opal_argv_free(modules);
    if (NULL != req) {
        OBJ_RELEASE(req);
    }
}

void orte_notifier_base_report(int sd, short args, void *cbdata)
{
    orte_notifier_request_t *req = (orte_notifier_request_t*)cbdata;
    char **modules = NULL;
    orte_notifier_active_module_t *imod;
    int i;

    /* if no modules are active, then there is nothing to do */
    if (0 == opal_list_get_size(&orte_notifier_base.modules)) {
        return;
    }

    /* see if the job requested any notifications */
    if (!orte_get_attribute(&req->jdata->attributes, ORTE_JOB_NOTIFICATIONS, (void**)modules, OPAL_STRING)) {
        return;
    }

    /* need to process the notification string to get the names of the modules */
    if (NULL == modules) {
        orte_notifier_base_identify_modules(&modules, req);

        /* no modules selected then nothing to do */
        if (NULL == modules) {
            return;
        }
    }

    for (i=0; NULL != modules[i]; i++) {
        OPAL_LIST_FOREACH(imod, &orte_notifier_base.modules, orte_notifier_active_module_t) {
            if (NULL != imod->module->log &&
                0 == strcmp(imod->component->base_version.mca_component_name, modules[i]))
                imod->module->report(req);
        }
    }
    opal_argv_free(modules);
}

const char* orte_notifier_base_sev2str(orte_notifier_severity_t severity)
{
    switch (severity) {
    case ORTE_NOTIFIER_EMERG:  return "EMERGENCY";  break;
    case ORTE_NOTIFIER_ALERT:  return "ALERT";  break;
    case ORTE_NOTIFIER_CRIT:   return "CRITICAL";   break;
    case ORTE_NOTIFIER_ERROR:  return "ERROR";  break;
    case ORTE_NOTIFIER_WARN:   return "WARNING";   break;
    case ORTE_NOTIFIER_NOTICE: return "NOTICE"; break;
    case ORTE_NOTIFIER_INFO:   return "INFO";   break;
    case ORTE_NOTIFIER_DEBUG:  return "DEBUG";  break;
    default: return "UNKNOWN"; break;
    }
}

static void orte_notifier_base_identify_modules(char ***modules,
                                                orte_notifier_request_t *req)
{
    if (NULL != req->action) {
        *modules = opal_argv_split(req->action, ',');
    } else {
        if (ORTE_NOTIFIER_EMERG == req->severity &&
            (NULL != orte_notifier_base.emerg_actions)) {
            *modules = opal_argv_split(orte_notifier_base.emerg_actions, ',');
        } else if (ORTE_NOTIFIER_ALERT == req->severity &&
                   (NULL != orte_notifier_base.alert_actions)) {
            *modules = opal_argv_split(orte_notifier_base.alert_actions, ',');
        } else if (ORTE_NOTIFIER_CRIT == req->severity &&
                   (NULL != orte_notifier_base.crit_actions)) {
            *modules = opal_argv_split(orte_notifier_base.crit_actions, ',');
        } else if (ORTE_NOTIFIER_WARN == req->severity &&
                   (NULL != orte_notifier_base.warn_actions)) {
            *modules = opal_argv_split(orte_notifier_base.warn_actions, ',');
        } else if (ORTE_NOTIFIER_NOTICE == req->severity &&
                   (NULL != orte_notifier_base.notice_actions)) {
            *modules = opal_argv_split(orte_notifier_base.notice_actions, ',');
        } else if (ORTE_NOTIFIER_INFO == req->severity &&
                   (NULL != orte_notifier_base.info_actions)) {
            *modules = opal_argv_split(orte_notifier_base.info_actions, ',');
        } else if (ORTE_NOTIFIER_DEBUG == req->severity &&
                   (NULL != orte_notifier_base.debug_actions)) {
            *modules = opal_argv_split(orte_notifier_base.debug_actions, ',');
        } else if (ORTE_NOTIFIER_ERROR == req->severity &&
                   (NULL != orte_notifier_base.error_actions)) {
            *modules = opal_argv_split(orte_notifier_base.error_actions, ',');
        } else if (NULL != orte_notifier_base.default_actions) {
            *modules = opal_argv_split(orte_notifier_base.default_actions, ',');
        }
    }
    return;
}

int set_notifier_policy(orte_notifier_severity_t sev, char *action)
{
    switch (sev) {
    case ORTE_NOTIFIER_EMERG:
            NOTIFIER_SAFE_FREE(orte_notifier_base.emerg_actions);
            orte_notifier_base.emerg_actions = action;
            break;
    case ORTE_NOTIFIER_ALERT:
            NOTIFIER_SAFE_FREE(orte_notifier_base.alert_actions);
            orte_notifier_base.alert_actions = action;
            break;
    case ORTE_NOTIFIER_CRIT:
            NOTIFIER_SAFE_FREE(orte_notifier_base.crit_actions);
            orte_notifier_base.crit_actions = action;
            break;
    case ORTE_NOTIFIER_WARN:
            NOTIFIER_SAFE_FREE(orte_notifier_base.warn_actions);
            orte_notifier_base.warn_actions = action;
            break;
    case ORTE_NOTIFIER_NOTICE:
            NOTIFIER_SAFE_FREE(orte_notifier_base.notice_actions);
            orte_notifier_base.notice_actions = action;
            break;
    case ORTE_NOTIFIER_INFO:
            NOTIFIER_SAFE_FREE(orte_notifier_base.info_actions);
            orte_notifier_base.info_actions = action;
            break;
    case ORTE_NOTIFIER_DEBUG:
            NOTIFIER_SAFE_FREE(orte_notifier_base.debug_actions);
            orte_notifier_base.debug_actions = action;
            break;
    case ORTE_NOTIFIER_ERROR:
            NOTIFIER_SAFE_FREE(orte_notifier_base.error_actions);
            orte_notifier_base.error_actions = action;
            break;
   default: 
            NOTIFIER_SAFE_FREE(orte_notifier_base.default_actions);
            orte_notifier_base.default_actions = action;
            break;
    }
    return ORTE_SUCCESS;
}

const char *get_notifier_policy(orte_notifier_severity_t sev)
{
    const char *action = NULL;
    if (ORTE_NOTIFIER_EMERG ==  sev) {
        action = orte_notifier_base.emerg_actions; 
    } else if (ORTE_NOTIFIER_ALERT ==  sev) {
        action = orte_notifier_base.alert_actions; 
    } else  if (ORTE_NOTIFIER_CRIT ==  sev) {
        action = orte_notifier_base.crit_actions; 
    } else if (ORTE_NOTIFIER_WARN ==  sev) {
        action = orte_notifier_base.warn_actions; 
    } else if (ORTE_NOTIFIER_NOTICE ==  sev) {
        action = orte_notifier_base.notice_actions; 
    } else if (ORTE_NOTIFIER_INFO ==  sev) {
        action = orte_notifier_base.info_actions; 
    } else if (ORTE_NOTIFIER_DEBUG ==  sev) {
        action = orte_notifier_base.debug_actions; 
    } else if (ORTE_NOTIFIER_ERROR ==  sev) {
        action = orte_notifier_base.error_actions; 
    } else {
        action = orte_notifier_base.default_actions; 
    } 
    if (NULL == action) {
        action = orte_notifier_base.default_actions; 
    } 

    return action;
}
