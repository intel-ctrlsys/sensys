/*
 * Copyright (c) 2009-2011 Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2013-2014 Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <signal.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include "opal/util/error.h"
#include "opal/util/output.h"
#include "opal/util/show_help.h"
#include "opal/runtime/opal.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/runtime.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_locks.h"
#include "orte/mca/ess/base/base.h"
#include "orte/util/show_help.h"

#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/sst/base/base.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/runtime/runtime.h"

int orcm_initialized = 0;
bool orcm_finalizing = false;
int orcm_debug_output = -1;
int orcm_debug_verbosity = 0;
bool orcm_sched_kill_dvm = false;
opal_list_t *orcm_clusters = NULL;
opal_list_t *orcm_schedulers = NULL;

#if OPAL_CC_USE_PRAGMA_IDENT
#pragma ident ORCM_IDENT_STRING
#elif OPAL_CC_USE_IDENT
#ident ORCM_IDENT_STRING
#endif
const char orcm_version_string[] = ORCM_IDENT_STRING;

int orcm_init(orcm_proc_type_t flags)
{
    int ret;
    char *error;
    int spin;
    opal_output_stream_t lds;

    if (0 < orcm_initialized) {
        /* track number of times we have been called */
        orcm_initialized++;
        return ORCM_SUCCESS;
    }
    orcm_initialized++;

    if (NULL != getenv("ORCM_MCA_spin")) {
        spin = 1;
        /* spin until a debugger can attach */
        while (0 != spin) {
            ret = 0;
            while (ret < 10000) {
                ret++;
            };
        }
    }
    
    /* initialize the opal layer */
    if (ORTE_SUCCESS != (ret = opal_init(NULL, NULL))) {
        error = "opal_init";
        goto error;
    }
    
    orcm_debug_verbosity = -1;
    (void) mca_base_var_register ("orcm", "orcm", NULL, "debug_verbose",
                                  "Verbosity level for ORCM debug messages (default: 1)",
                                  MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                  OPAL_INFO_LVL_9, MCA_BASE_VAR_SCOPE_ALL,
                                  &orcm_debug_verbosity);
    if (0 <= orcm_debug_verbosity) {
        /* get a debug output channel */
        OBJ_CONSTRUCT(&lds, opal_output_stream_t);
        lds.lds_want_stdout = true;
        orcm_debug_output = opal_output_open(&lds);
        OBJ_DESTRUCT(&lds);
        /* set the verbosity */
        opal_output_set_verbosity(orcm_debug_output, orcm_debug_verbosity);
    }

    /* ensure we know the type of proc for when we finalize */
    orte_process_info.proc_type = flags;

    /* setup the locks */
    if (ORTE_SUCCESS != (ret = orte_locks_init())) {
        error = "orte_locks_init";
        goto error;
    }
    
    /* register handler for errnum -> string conversion */
    opal_error_register("ORTE", ORTE_ERR_BASE, ORTE_ERR_MAX, orte_err2str);

    /* Ensure the rest of the process info structure is initialized */
    if (ORTE_SUCCESS != (ret = orte_proc_info())) {
        error = "orte_proc_info";
        goto error;
    }

    /* register handler for errnum -> string conversion */
    opal_error_register("ORCM", ORCM_ERR_BASE, ORCM_ERR_MAX, orcm_err2str);

    /* we don't need a progress thread as all our tools loop inside themselves,
     * so define orte_event_base to be the base opal_event_base
     */
    orte_event_base = opal_event_base;

    /* setup the globals */
    orcm_clusters = OBJ_NEW(opal_list_t);
    orcm_schedulers = OBJ_NEW(opal_list_t);

    /* everyone must open the cfgi framework */
    if (ORCM_SUCCESS != (ret = mca_base_framework_open(&orcm_cfgi_base_framework, 0))) {
        error = "orcm_cfgi_base_open";
        goto error;
    }
    if (ORCM_SUCCESS != (ret = orcm_cfgi_base_select())) {
        error = "orcm_cfgi_select";
        goto error;
    }

    /* everyone must open the sst framework */
    if (ORCM_SUCCESS != (ret = mca_base_framework_open(&orcm_sst_base_framework, 0))) {
        error = "orcm_sst_base_open";
        goto error;
    }
    if (ORCM_SUCCESS != (ret = orcm_sst_base_select())) {
        error = "orcm_sst_select";
        goto error;
    }

    /* open the ESS and select the correct module for this environment - the
     * orcm module is basically a no-op, but we need the framework defined
     * as other parts of ORTE will want to call it
     */
    if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_ess_base_framework, 0))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_ess_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_ess_base_select())) {
        error = "orte_ess_base_select";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_ess.init())) {
        error = "orte_ess_init";
        goto error;
    }

    /* initialize us - we will register the ORTE-level MCA params in there */
    if (ORTE_SUCCESS != (ret = orcm_sst.init())) {
        error = "orte_init";
        goto error;
    }
    
    /* setup the orte_show_help system - don't do this until the
     * end as otherwise show_help messages won't appear
     */
    if (ORTE_SUCCESS != (ret = orte_show_help_init())) {
        error = "opal_output_init";
        goto error;
    }

    /* initialize orcm datatype support */
    if (ORCM_SUCCESS != (ret = orcm_dt_init())) {
        error = "orcm_dt_init";
        goto error;
    }
    
    /* flag that orte is initialized so things can work */
    orte_initialized = true;

    return ORCM_SUCCESS;

 error:
    if (ORCM_ERR_SILENT != ret) {
        opal_show_help("help-orcm-runtime.txt",
                       "orcm_init:startup:internal-failure",
                       true, error, ORTE_ERROR_NAME(ret), ret);
    }
    
    return ret;
}
