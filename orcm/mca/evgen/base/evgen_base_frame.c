/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "opal/mca/mca.h"
#include "opal/util/argv.h"
#include "opal/util/fd.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/runtime/opal_progress_threads.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/util/regex.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/evgen/base/base.h"

/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/evgen/base/static-components.h"

/*
 * Global variables
 */
orcm_evgen_base_t orcm_evgen_base;
int orcm_evgen_base_output = -1;
opal_event_base_t *orcm_evgen_evbase = NULL;

static int orcm_evgen_base_close(void)
{
    if (NULL != orcm_evgen_evbase) {
        opal_progress_thread_finalize("evgen");
    }

    OPAL_LIST_DESTRUCT(&orcm_evgen_base.actives);

    /* Close all remaining available components */
    return mca_base_framework_components_close(&orcm_evgen_base_framework, NULL);
}

/**
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_evgen_base_open(mca_base_open_flag_t flags)
{
    int rc;

    /* construct the array of active modules */
    OBJ_CONSTRUCT(&orcm_evgen_base.actives, opal_list_t);

    /* start the progress thread */
      if (NULL == (orcm_evgen_evbase = opal_progress_thread_init("evgen"))) {
        return ORCM_ERROR;
    }

    /* Open up all available components */
    rc = mca_base_framework_components_open(&orcm_evgen_base_framework, flags);
    orcm_evgen_base_output = orcm_evgen_base_framework.framework_output;

    return rc;
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, evgen, "ORCM Event generation", NULL,
                           orcm_evgen_base_open, orcm_evgen_base_close,
                           mca_evgen_base_static_components, 0);


const char* orcm_evgen_base_print_type(int t)
{
    switch(t) {
        case ORCM_RAS_EVENT_SENSOR:
            return "SENSOR";
        case ORCM_RAS_EVENT_EXCEPTION:
            return "EXCEPTION";
        case ORCM_RAS_EVENT_COUNTER:
            return "COUNTER";
        case ORCM_RAS_EVENT_STATE_TRANSITION:
            return "TRANSITION";
        default:
            return "UNKNOWN";
    }
}

const char* orcm_evgen_base_print_severity(int s)
{
    switch(s) {
        case ORCM_RAS_SEVERITY_EMERG:
            return "EMERGENCY";
        case ORCM_RAS_SEVERITY_FATAL:
            return "FATAL";
        case ORCM_RAS_SEVERITY_ALERT:
            return "ALERT";
        case ORCM_RAS_SEVERITY_CRIT:
            return "CRITICAL";
        case ORCM_RAS_SEVERITY_ERROR:
            return "ERROR";
        case ORCM_RAS_SEVERITY_WARNING:
            return "WARNING";
        case ORCM_RAS_SEVERITY_NOTICE:
            return "NOTICE";
        case ORCM_RAS_SEVERITY_INFO:
            return "INFO";
        case ORCM_RAS_SEVERITY_TRACE:
            return "TRACE";
        case ORCM_RAS_SEVERITY_DEBUG:
            return "DEBUG";
        case ORCM_RAS_SEVERITY_UNKNOWN:
            return "UNKNOWN";
        default:
            return "UNKNOWN";
    }
}

/* framework class instanstiations */
OBJ_CLASS_INSTANCE(orcm_evgen_active_module_t,
                   opal_list_item_t,
                   NULL, NULL);


static void evcon(orcm_ras_event_t *p)
{
    OBJ_CONSTRUCT(&p->reporter, opal_list_t);
    p->type = ORCM_RAS_EVENT_UNKNOWN_TYPE;
    p->timestamp = 0;
    p->severity = ORCM_RAS_SEVERITY_UNKNOWN;
    OBJ_CONSTRUCT(&p->description, opal_list_t);
    OBJ_CONSTRUCT(&p->data, opal_list_t);
    p->cbfunc = NULL;
    p->cbdata = NULL;
}
static void evdes(orcm_ras_event_t *p)
{
    OPAL_LIST_DESTRUCT(&p->reporter);
    OPAL_LIST_DESTRUCT(&p->description);
    OPAL_LIST_DESTRUCT(&p->data);
}
OBJ_CLASS_INSTANCE(orcm_ras_event_t,
                   opal_object_t,
                   evcon, evdes);
