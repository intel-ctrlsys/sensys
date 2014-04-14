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
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_NOTIFIER_BASE_H
#define MCA_NOTIFIER_BASE_H

/*
 * includes
 */
#include "orcm_config.h"

#include "opal/mca/mca.h"
#include "opal/class/opal_object.h"
#include "opal/class/opal_list.h"

#include "orte/mca/ess/ess.h"
#include "orcm/mca/notifier/notifier.h"

BEGIN_C_DECLS

ORCM_DECLSPEC extern mca_base_framework_t orcm_notifier_base_framework;

typedef struct {
    char *imodules_log;
    char *imodules_help;
    char *imodules_log_peer;
    char *imodules_log_event;
} orcm_notifier_base_t;
ORCM_DECLSPEC extern orcm_notifier_base_t orcm_notifier_base;

/*
 * Type for holding selected module / component pairs
 */
typedef struct {
    opal_list_item_t super;
    /* Component */
    orcm_notifier_base_component_t *onbsp_component;
    /* Module */
    orcm_notifier_base_module_t *onbsp_module;
    /* Priority */
    int onbsp_priority;
} orcm_notifier_base_selected_pair_t;

OBJ_CLASS_DECLARATION(orcm_notifier_base_selected_pair_t);

/*
 * function definitions
 */
ORCM_DECLSPEC int orcm_notifier_base_select(void);

ORCM_DECLSPEC void orcm_notifier_log(orcm_notifier_base_severity_t severity, 
                                     int errcode, 
                                     const char *msg, ...);
ORCM_DECLSPEC void orcm_notifier_show_help(orcm_notifier_base_severity_t severity, 
                                           int errcode, 
                                           const char *file, 
                                           const char *topic, ...);
ORCM_DECLSPEC void orcm_notifier_log_peer(orcm_notifier_base_severity_t severity, 
                                          int errcode, 
                                          orte_process_name_t *peer_proc, 
                                          const char *msg, ...);
ORCM_DECLSPEC const char* orcm_notifier_base_sev2str(orcm_notifier_base_severity_t severity);
ORCM_DECLSPEC char *orcm_notifier_base_peer_log(int errcode, 
                                                orte_process_name_t *peer_proc, 
                                                const char *msg, va_list ap);

#if ORCM_WANT_NOTIFIER_LOG_EVENT

ORCM_DECLSPEC int orcm_notifier_base_events_init(void);
ORCM_DECLSPEC void orcm_notifier_base_events_finalize(void);

#else /* ORCM_WANT_NOTIFIER_LOG_EVENT */

#define orcm_notifier_base_events_init()     do {} while (0)
#define orcm_notifier_base_events_finalize() do {} while (0)

#endif /* ORCM_WANT_NOTIFIER_LOG_EVENT */

/*
 * global variables in the base
 * Needs to be declspec'ed for ompi_info and others 
 */
/*
 * Indication of whether a component was successfully selected or not
 * (1 component per interface)
 */
ORCM_DECLSPEC extern bool orcm_notifier_base_log_selected;
ORCM_DECLSPEC extern bool orcm_notifier_base_help_selected;
ORCM_DECLSPEC extern bool orcm_notifier_base_log_peer_selected;
ORCM_DECLSPEC extern bool orcm_notifier_base_log_event_selected;
/*
 * Lists of selected modules (1 per interface)
 */
ORCM_DECLSPEC extern opal_list_t orcm_notifier_log_selected_modules;
ORCM_DECLSPEC extern opal_list_t orcm_notifier_help_selected_modules;
ORCM_DECLSPEC extern opal_list_t orcm_notifier_log_peer_selected_modules;
ORCM_DECLSPEC extern opal_list_t orcm_notifier_log_event_selected_modules;
/*
 * That one is a merge of the per interface lists
 * It is used during finalize phase to finalize only once each selected module
 */
ORCM_DECLSPEC extern opal_list_t orcm_notifier_base_selected_modules;
ORCM_DECLSPEC extern orcm_notifier_base_severity_t orcm_notifier_threshold_severity;
ORCM_DECLSPEC extern opal_list_t orcm_notifier_base_components_available;

END_C_DECLS
#endif
