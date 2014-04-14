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
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/util/output.h"
#include "opal/mca/base/base.h"
#include "opal/class/opal_list.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "orcm/mca/notifier/base/base.h"

/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/notifier/base/static-components.h"

/*
 * Global variables
 */
orcm_notifier_base_severity_t orcm_notifier_threshold_severity = 
    ORCM_NOTIFIER_ERROR;
opal_list_t orcm_notifier_base_components_available;
opal_list_t orcm_notifier_base_selected_modules;
opal_list_t orcm_notifier_log_selected_modules;
opal_list_t orcm_notifier_help_selected_modules;
opal_list_t orcm_notifier_log_peer_selected_modules;
opal_list_t orcm_notifier_log_event_selected_modules;

orcm_notifier_API_module_t orcm_notifier = {
    orcm_notifier_log,
    orcm_notifier_show_help,
    orcm_notifier_log_peer,
};
orcm_notifier_base_t orcm_notifier_base;

/**
 * Function for selecting a set of components from all those that are
 * available.
 *
 * It is possible to select a subset of these components for any interface.
 * The syntax is the following:
 * [ -mca notifier <list0> ] [ -mca notifier_log <list1> ]
 *                           [ -mca notifier_help <list2> ]
 *                           [ -mca notifier_log_peer <list3> ]
 *                           [ -mca notifier_log_event <list4> ]
 * Rules:
 * . <list0> empty means nothing selected
 * . <list0> to <list4> = comma separated lists of component names
 * . <list1> to <list4> may be one of:
 *     . subsets of <list0>
 *     . "none" keyword (means empty)
 * . 1 of <list1> to <list4> empty means = <list0>
 * Last point makes it possible to preserve the way it works today
 *
 * Examples:
 * 1)
 * -mca notifier syslog,smtp
 *      --> syslog and smtp are selected for the log, show_help, log_peer and
 *          log_event interfaces.
 * 2)
 * -mca notifier_log syslog
 *      --> no interface is activated, no component is selected
 * 3)
 * -mca notifier syslog -mca notifier_help none
 *                      -mca notifier_log_peer none
 *                      -mca notifier_log_event none
 *      --> only the log interface is activated, with the syslog component
 * 4)
 * -mca notifier syslog,smtp,hnp -mca notifier_help syslog
 *                               -mca notifier_log_peer smtp
 *                               -mca notifier_log_event none
 *      --> the log interface is activated, with the syslog, smtp and hnp
 *                                               components
 *          the log_help interface is activated, with the syslog component
 *          the log_peer interface is activated, with the smtp component
 *          the log_event interface is not activated
 */
static char *level;
static int orcm_notifier_base_register(mca_base_register_flag_t flags)
{
    /* let the user define a base level of severity to report */
    level = NULL;
    (void) mca_base_var_register("orcm", "notifier", "base", "threshold_severity",
                                 "Report all events at or above this severity [default: error]",
                                 MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                 OPAL_INFO_LVL_9,
                                 MCA_BASE_VAR_SCOPE_READONLY,
                                 &level);
    if (NULL == level) {
        orcm_notifier_threshold_severity = ORCM_NOTIFIER_ERROR;
    } else if (0 == strncasecmp(level, "emerg", strlen("emerg"))) {
        orcm_notifier_threshold_severity = ORCM_NOTIFIER_EMERG;
    } else if (0 == strncasecmp(level, "alert", strlen("alert"))) {
        orcm_notifier_threshold_severity = ORCM_NOTIFIER_ALERT;
    } else if (0 == strncasecmp(level, "crit", strlen("crit"))) {
        orcm_notifier_threshold_severity = ORCM_NOTIFIER_CRIT;
    } else if (0 == strncasecmp(level, "warn", strlen("warn"))) {
        orcm_notifier_threshold_severity = ORCM_NOTIFIER_WARN;
    } else if (0 == strncasecmp(level, "notice", strlen("notice"))) {
        orcm_notifier_threshold_severity = ORCM_NOTIFIER_NOTICE;
    } else if (0 == strncasecmp(level, "info", strlen("info"))) {
        orcm_notifier_threshold_severity = ORCM_NOTIFIER_INFO;
    } else if (0 == strncasecmp(level, "debug", strlen("debug"))) {
        orcm_notifier_threshold_severity = ORCM_NOTIFIER_DEBUG;
    } else if (0 != strncasecmp(level, "error", strlen("error"))) {
        opal_output(0, "Unknown notifier level");
        return ORCM_ERROR;
    }

    /*
     * Get the include lists for each interface that is available
     */
    orcm_notifier_base.imodules_log = NULL;
    (void) mca_base_var_register("orcm", "notifier", "base", "log",
                                 "Comma-delimisted list of notifier components to use "
                                 "for orcm_notifier_log (empty = all selected)",
                                 MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                 OPAL_INFO_LVL_9,
                                 MCA_BASE_VAR_SCOPE_READONLY,
                                 &orcm_notifier_base.imodules_log);

    orcm_notifier_base.imodules_help = NULL;
    (void) mca_base_var_register("orcm", "notifier", "base", "log",
                                 "Comma-delimisted list of notifier components to use "
                                 "for orcm_notifier_log (empty = all selected)",
                                 MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                 OPAL_INFO_LVL_9,
                                 MCA_BASE_VAR_SCOPE_READONLY,
                                 &orcm_notifier_base.imodules_help);

    orcm_notifier_base.imodules_log_peer = NULL;
    (void) mca_base_var_register("orcm", "notifier", "base", "log_peer",
                                 "Comma-delimisted list of notifier components to "
                                 "use for orcm_notifier_log_peer (empty = all selected)",
                                 MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                 OPAL_INFO_LVL_9,
                                 MCA_BASE_VAR_SCOPE_READONLY,
                                 &orcm_notifier_base.imodules_log_peer);

    orcm_notifier_base.imodules_log_event = NULL;
#if ORCM_WANT_NOTIFIER_LOG_EVENT
    (void) mca_base_var_register("orcm", "notifier", "base", "log_event",
                                 "Comma-delimisted list of notifier components to "
                                 "use for ORCM_NOTIFIER_LOG_EVENT (empty = all selected)",
                                 MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                 OPAL_INFO_LVL_9,
                                 MCA_BASE_VAR_SCOPE_READONLY,
                                 &orcm_notifier_base.imodules_log_event);
#endif /* ORCM_WANT_NOTIFIER_LOG_EVENT */

    return ORCM_SUCCESS;
}    

static int orcm_notifier_base_close(void)
{
    /* cleanup the globals */
    OPAL_LIST_DESTRUCT(&orcm_notifier_base_selected_modules);
    OPAL_LIST_DESTRUCT(&orcm_notifier_log_selected_modules);
    OPAL_LIST_DESTRUCT(&orcm_notifier_help_selected_modules);
    OPAL_LIST_DESTRUCT(&orcm_notifier_log_peer_selected_modules);
    OPAL_LIST_DESTRUCT(&orcm_notifier_log_event_selected_modules);

    return mca_base_framework_components_close(&orcm_notifier_base_framework, NULL);
}

/**
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_notifier_base_open(mca_base_open_flag_t flags)
{
    /* initialize the globals */
    OBJ_CONSTRUCT(&orcm_notifier_base_selected_modules, opal_list_t);
    OBJ_CONSTRUCT(&orcm_notifier_log_selected_modules, opal_list_t);
    OBJ_CONSTRUCT(&orcm_notifier_help_selected_modules, opal_list_t);
    OBJ_CONSTRUCT(&orcm_notifier_log_peer_selected_modules, opal_list_t);
    OBJ_CONSTRUCT(&orcm_notifier_log_event_selected_modules, opal_list_t);

    /* Open up all available components */

    return mca_base_framework_components_open(&orcm_notifier_base_framework, flags);
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, notifier, "ORCM Notifier Framework",
                           orcm_notifier_base_register,
                           orcm_notifier_base_open, orcm_notifier_base_close,
                           mca_notifier_base_static_components, 0);
