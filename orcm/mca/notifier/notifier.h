/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2009      Cisco Systems, Inc.  All Rights Reserved.
 * Copyright (c) 2012      Los Alamos National Security, LLC.
 *                         All rights reserved.
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 *
 * The OpenRTE Notifier Framework
 *
 * The OpenRTE Notifier framework provides a mechanism for notifying
 * system administrators or other fault monitoring systems that a
 * problem with the underlying cluster has been detected - e.g., a
 * failed connection in a network fabric
 */

#ifndef MCA_NOTIFIER_H
#define MCA_NOTIFIER_H

/*
 * includes
 */

#include "orcm_config.h"

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include "opal/mca/mca.h"

#include "orcm/constants.h"
#include "orcm/types.h"

#include "notifier_event_types.h"

BEGIN_C_DECLS

/* The maximum size of any on-stack buffers used in the notifier
 * so we can try to avoid calling malloc in OUT_OF_RESOURCES conditions.
 * The code has NOT been auditied for use of malloc, so this still
 * may fail to get the "OUT_OF_RESOURCE" message out.  Oh Well.
 */
#define ORCM_NOTIFIER_MAX_BUF	512

/* Severities */
typedef enum {
    ORCM_NOTIFIER_EMERG = LOG_EMERG,
    ORCM_NOTIFIER_ALERT = LOG_ALERT,
    ORCM_NOTIFIER_CRIT = LOG_CRIT,
    ORCM_NOTIFIER_ERROR = LOG_ERR,
    ORCM_NOTIFIER_WARN = LOG_WARNING,
    ORCM_NOTIFIER_NOTICE = LOG_NOTICE,
    ORCM_NOTIFIER_INFO = LOG_INFO,
    ORCM_NOTIFIER_DEBUG = LOG_DEBUG
} orcm_notifier_base_severity_t;

/*
 * Component functions - all MUST be provided!
 */

/* initialize the selected module */
typedef int (*orcm_notifier_base_module_init_fn_t)(void);
    
/* finalize the selected module */
typedef void (*orcm_notifier_base_module_finalize_fn_t)(void);

/* Log a failure message */
typedef void (*orcm_notifier_base_module_log_fn_t)(orcm_notifier_base_severity_t severity, int errcode, const char *msg, va_list ap)
    __opal_attribute_format_funcptr__(__printf__, 3, 0);

/* Log a failure that is based upon a show_help message */
typedef void (*orcm_notifier_base_module_log_show_help_fn_t)(orcm_notifier_base_severity_t severity, int errcode, const char *file, const char *topic, va_list ap);

/* Log a failure related to a peer */
typedef void (*orcm_notifier_base_module_log_peer_fn_t)(orcm_notifier_base_severity_t severity, int errcode, orte_process_name_t *peer_proc, const char *msg, va_list ap)
    __opal_attribute_format_funcptr__(__printf__, 4, 0);

/* Log an unusual event message */
typedef void (*orcm_notifier_base_module_log_event_fn_t)(const char *msg);

/*
 * Ver 1.0
 */
struct orcm_notifier_base_module_1_0_0_t {
    orcm_notifier_base_module_init_fn_t             init;
    orcm_notifier_base_module_finalize_fn_t         finalize;
    orcm_notifier_base_module_log_fn_t              log;
    orcm_notifier_base_module_log_show_help_fn_t    help;
    orcm_notifier_base_module_log_peer_fn_t         peer;
    orcm_notifier_base_module_log_event_fn_t        log_event;
};

typedef struct orcm_notifier_base_module_1_0_0_t orcm_notifier_base_module_1_0_0_t;
typedef orcm_notifier_base_module_1_0_0_t orcm_notifier_base_module_t;

/*
 * API functions
 */
/* Log a failure message */
typedef void (*orcm_notifier_base_API_log_fn_t)(orcm_notifier_base_severity_t severity, int errcode, const char *msg, ...);

/* Log a failure that is based upon a show_help message */
typedef void (*orcm_notifier_base_API_log_show_help_fn_t)(orcm_notifier_base_severity_t severity, int errcode, const char *file, const char *topic, ...);

/* Log a failure related to a peer */
typedef void (*orcm_notifier_base_API_log_peer_fn_t)(orcm_notifier_base_severity_t severity, int errcode, orte_process_name_t *peer_proc, const char *msg, ...);
    
/*
 * Define a struct to hold the API functions that users will call
 */
struct orcm_notifier_API_module_1_0_0_t {
    orcm_notifier_base_API_log_fn_t              log;
    orcm_notifier_base_API_log_show_help_fn_t    show_help;
    orcm_notifier_base_API_log_peer_fn_t         log_peer;
};
typedef struct orcm_notifier_API_module_1_0_0_t orcm_notifier_API_module_1_0_0_t;
typedef orcm_notifier_API_module_1_0_0_t orcm_notifier_API_module_t;

ORCM_DECLSPEC extern orcm_notifier_API_module_t orcm_notifier;

/*
 * the standard component data structure
 */
struct orcm_notifier_base_component_1_0_0_t {
    mca_base_component_t base_version;
    mca_base_component_data_t base_data;
};
typedef struct orcm_notifier_base_component_1_0_0_t orcm_notifier_base_component_1_0_0_t;
typedef orcm_notifier_base_component_1_0_0_t orcm_notifier_base_component_t;


/*
 * Macro for use in components that are of type notifier v1.0.0
 */
#define ORCM_NOTIFIER_BASE_VERSION_1_0_0 \
  /* notifier v1.0 is chained to MCA v2.0 */ \
  MCA_BASE_VERSION_2_0_0, \
  /* notifier v1.0 */ \
  "notifier", 1, 0, 0

/*
 * To manage unusual events notifications
 * Set to noop if not wanted
 */

#if ORCM_WANT_NOTIFIER_LOG_EVENT

#include "notifier_event_calls.h"

#else /* ORCM_WANT_NOTIFIER_LOG_EVENT */

#define ORCM_NOTIFIER_DEFINE_EVENT(i, m)
#define ORCM_NOTIFIER_LOG_EVENT(i, c, t) do {} while (0)

#endif /* ORCM_WANT_NOTIFIER_LOG_EVENT */

END_C_DECLS

#endif /* MCA_NOTIFIER_H */
