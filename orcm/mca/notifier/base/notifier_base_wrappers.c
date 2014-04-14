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
 * Copyright (c) 2008-2009 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"

#include "orcm/constants.h"
#include "orte/mca/ess/ess.h"
#include "orte/util/error_strings.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/notifier/base/base.h"

void orcm_notifier_log(orcm_notifier_base_severity_t severity, 
                       int errcode, const char *msg, ...)
{
    va_list ap;
    opal_list_item_t *item;
    orcm_notifier_base_selected_pair_t *pair;

    if (!orcm_notifier_base_log_selected) {
        return;
    }

    /* is the severity value above the threshold - I know
     * this seems backward, but lower severity values are
     * considered "more severe"
     */
    if (severity > orcm_notifier_threshold_severity) {
        return;
    }

    for (item = opal_list_get_first(&orcm_notifier_log_selected_modules);
         opal_list_get_end(&orcm_notifier_log_selected_modules) != item;
         item = opal_list_get_next(item)) {
        pair = (orcm_notifier_base_selected_pair_t*) item;
        if (NULL != pair->onbsp_module->log) {
            va_start(ap, msg);
            pair->onbsp_module->log(severity, errcode, msg, ap);
            va_end(ap);
        }
    }
}

void orcm_notifier_show_help(orcm_notifier_base_severity_t severity, 
                             int errcode, const char *file, 
                             const char *topic, ...)
{
    va_list ap;
    opal_list_item_t *item;
    orcm_notifier_base_selected_pair_t *pair;

    if (!orcm_notifier_base_help_selected) {
        return;
    }

    /* is the severity value above the threshold - I know
     * this seems backward, but lower severity values are
     * considered "more severe"
     */
    if (severity > orcm_notifier_threshold_severity) {
        return;
    }

    for (item = opal_list_get_first(&orcm_notifier_help_selected_modules);
         opal_list_get_end(&orcm_notifier_help_selected_modules) != item;
         item = opal_list_get_next(item)) {
        pair = (orcm_notifier_base_selected_pair_t*) item;
        if (NULL != pair->onbsp_module->help) {
            va_start(ap, topic);
            pair->onbsp_module->help(severity, errcode, file, topic, ap);
            va_end(ap);
        }
    }
}

void orcm_notifier_log_peer(orcm_notifier_base_severity_t severity, 
                            int errcode, 
                            orte_process_name_t *peer_proc, 
                            const char *msg, ...)
{
    va_list ap;
    opal_list_item_t *item;
    orcm_notifier_base_selected_pair_t *pair;

    if (!orcm_notifier_base_log_peer_selected) {
        return;
    }

    /* is the severity value above the threshold - I know
     * this seems backward, but lower severity values are
     * considered "more severe"
     */
    if (severity > orcm_notifier_threshold_severity) {
        return;
    }

    for (item = opal_list_get_first(&orcm_notifier_log_peer_selected_modules);
         opal_list_get_end(&orcm_notifier_log_peer_selected_modules) != item;
         item = opal_list_get_next(item)) {
        pair = (orcm_notifier_base_selected_pair_t*) item;
        if (NULL != pair->onbsp_module->peer) {
            va_start(ap, msg);
            pair->onbsp_module->peer(severity, errcode, peer_proc, msg, ap);
            va_end(ap);
        }
    }
}


const char* orcm_notifier_base_sev2str(orcm_notifier_base_severity_t severity)
{
    switch (severity) {
    case ORCM_NOTIFIER_EMERG:  return "EMERG";  break;
    case ORCM_NOTIFIER_ALERT:  return "ALERT";  break;
    case ORCM_NOTIFIER_CRIT:   return "CRIT";   break;
    case ORCM_NOTIFIER_ERROR:  return "ERROR";  break;
    case ORCM_NOTIFIER_WARN:   return "WARN";   break;
    case ORCM_NOTIFIER_NOTICE: return "NOTICE"; break;
    case ORCM_NOTIFIER_INFO:   return "INFO";   break;
    case ORCM_NOTIFIER_DEBUG:  return "DEBUG";  break;
    default: return "UNKNOWN"; break;
    }
}


char *orcm_notifier_base_peer_log(int errcode, orte_process_name_t *peer_proc, 
                                  const char *msg, va_list ap)
{
    char *buf = (char *) malloc(ORCM_NOTIFIER_MAX_BUF + 1);
    char *peer_host = NULL, *peer_name = NULL;
    char *pos = buf;
    char *errstr;
    int ret, len, space = ORCM_NOTIFIER_MAX_BUF;

    if (NULL == buf) {
        return NULL;
    }

    if (peer_proc) {
        peer_host = orte_get_proc_hostname(peer_proc);
        peer_name = strdup(ORTE_NAME_PRINT(peer_proc));
    }

    len = snprintf(pos, space,
                   "While communicating to proc %s on node %s,"
                   " proc %s on node %s encountered an error ",
                   peer_name ? peer_name : "UNKNOWN",
                   peer_host ? peer_host : "UNKNOWN",
                   ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                   orte_process_info.nodename);
    space -= len;
    pos += len;
    
    if (0 < space) {
        ret = orte_err2str(errcode, (const char **)&errstr);
        if (ORCM_SUCCESS == ret) {
            len = snprintf(pos, space, "'%s':", errstr);
            free(errstr);
        } else {
            len = snprintf(pos, space, "(%d):", errcode);
        }
        space -= len;
        pos += len;
    }

    if (0 < space) {
        vsnprintf(pos, space, msg, ap);
    }

    buf[ORCM_NOTIFIER_MAX_BUF] = '\0';
    if (NULL != peer_name) {
        free(peer_name);
    }
    return buf;
}

