/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "opal/mca/mca.h"
#include "opal/util/argv.h"
#include "opal/util/fd.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/threads/threads.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/util/regex.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/runtime/orcm_progress.h"
#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/pwrmgmt/base/base.h"
#include "orcm/mca/pwrmgmt/base/pwrmgmt_freq_utils.h"

/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/pwrmgmt/base/static-components.h"

static void orcm_pwrmgmt_base_recv(int status, orte_process_name_t* sender,
                       opal_buffer_t *buffer,
                       orte_rml_tag_t tag, void *cbdata);

/*
 * Global variables
 */
orcm_pwrmgmt_base_API_module_t orcm_pwrmgmt_stubs = {
    orcm_pwrmgmt_base_init,
    orcm_pwrmgmt_base_finalize,
    orcm_pwrmgmt_base_component_select,
    orcm_pwrmgmt_base_alloc_notify,
    orcm_pwrmgmt_base_dealloc_notify,
    orcm_pwrmgmt_base_set_attributes,
    orcm_pwrmgmt_base_reset_attributes,
    orcm_pwrmgmt_base_get_attributes
};

orcm_pwrmgmt_base_API_module_t orcm_pwrmgmt = {0};

orcm_pwrmgmt_base_t orcm_pwrmgmt_base;

static int orcm_pwrmgmt_base_close(void)
{
    orcm_pwrmgmt_active_module_t *i_module;
    int i;

    for (i=0; i < orcm_pwrmgmt_base.modules.size; i++) {
        if (NULL == (i_module = (orcm_pwrmgmt_active_module_t*)opal_pointer_array_get_item(&orcm_pwrmgmt_base.modules, i))) {
            continue;
        }
        if (NULL != i_module->module->finalize) {
            i_module->module->finalize();
        }
    }
    OBJ_DESTRUCT(&orcm_pwrmgmt_base.modules);
    
    /* Close all remaining available components */
    return mca_base_framework_components_close(&orcm_pwrmgmt_base_framework, NULL);
}

/**
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_pwrmgmt_base_open(mca_base_open_flag_t flags)
{
    int rc;

    /* initialize the active module to the stub functions */
    orcm_pwrmgmt = orcm_pwrmgmt_stubs;

    /* construct the array of modules */
    OBJ_CONSTRUCT(&orcm_pwrmgmt_base.modules, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_pwrmgmt_base.modules, 3, INT_MAX, 1);

    /* register the messaging callback for the base framework */
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_PWRMGMT_BASE,
                            ORTE_RML_PERSISTENT,
                            orcm_pwrmgmt_base_recv,
                            NULL);

    /* Open up all available components */
    if (OPAL_SUCCESS != (rc = mca_base_framework_components_open(&orcm_pwrmgmt_base_framework, flags))) {
        return rc;
    }

    return OPAL_SUCCESS;
}

static void cons(orcm_pwrmgmt_active_module_t *t)
{
    t->priority = t->priority;
}
OBJ_CLASS_INSTANCE(orcm_pwrmgmt_active_module_t,
                   opal_object_t,
                   cons, NULL);

MCA_BASE_FRAMEWORK_DECLARE(orcm, pwrmgmt, "ORCM Power Management", NULL,
                           orcm_pwrmgmt_base_open, orcm_pwrmgmt_base_close,
                           mca_pwrmgmt_base_static_components, 0);


static void orcm_pwrmgmt_base_recv(int status, orte_process_name_t* sender,
                                   opal_buffer_t *buffer,
                                   orte_rml_tag_t tag, void *cbdata) {
    int rc, i, cnt;
    int n = 1;
    orcm_rm_cmd_flag_t command;
    orcm_alloc_t* alloc;
    opal_buffer_t *ans;
    char **nodelist = NULL;
    orte_process_name_t tgt;

    opal_output_verbose(3, orcm_pwrmgmt_base_framework.framework_output,
                        "%s orcm_pwrmgmt_base: Received message from %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        ORTE_NAME_PRINT(sender));
    /* if we are aborting or shutting down, ignore this */
    if (orte_abnormal_term_ordered || orte_finalizing || !orte_initialized) {
        return;
    }

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &command, &n, ORCM_RM_CMD_T))) {
        ORTE_ERROR_LOG (rc);
        return;
    }

    switch(command) {
    case ORCM_SET_POWER_COMMAND:
        OPAL_OUTPUT_VERBOSE((5, orcm_pwrmgmt_base_framework.framework_output,
                             "%s pwrmgmt:base:receive got ORCM_SET_POWER_COMMAND",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &alloc,
                                                  &n, ORCM_ALLOC))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
 
        // If I am the head daemon, send the message out to everyone else
        if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL,
                                                            &alloc->hnp,
                                                            ORTE_PROC_MY_NAME)) {
            ans = OBJ_NEW(opal_buffer_t);

            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &command,
                                                    1, ORCM_RM_CMD_T))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(ans);
                return;
            }

            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &alloc,
                                                    n, ORCM_ALLOC))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(ans);
                return;
            }

            orte_regex_extract_node_names(alloc->nodes, &nodelist);
            cnt = opal_argv_count(nodelist);
            if (0 == opal_argv_count(nodelist)) {
                fprintf(stdout, "Error: unable to extract nodelist\n");
                opal_argv_free(nodelist);
                ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                OBJ_RELEASE(ans);
                return;
            }
            for (i = 0; i < cnt; i++) {
                OBJ_RETAIN(ans);

                if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                           &tgt))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_RELEASE(ans);
                    return;
                }

                if(!OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL,
                                                                &tgt,
                                                                ORTE_PROC_MY_NAME)) {
                    if (ORTE_SUCCESS !=
                        (rc = orte_rml.send_buffer_nb(&tgt, ans,
                                                      ORCM_RML_TAG_PWRMGMT_BASE,
                                                      orte_rml_send_callback, NULL))) {
                        ORTE_ERROR_LOG(rc);
                        OBJ_RELEASE(ans);
                        return;
                    }
                }
            }
        }
        
        orcm_pwrmgmt_base_alloc_notify(alloc);
    break;
    default:
        opal_output(0,
                    "%s: orcm_pwrmgmt_base: Received an unknown command",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    break;
    }
}

const char* orcm_pwrmgmt_get_mode_string(int mode)
{
    switch(mode) {
    case ORCM_PWRMGMT_MODE_NONE:
        return "NONE";
    case ORCM_PWRMGMT_MODE_MANUAL_FREQ:
        return "MANUAL_FREQ";
    case ORCM_PWRMGMT_MODE_AUTO_UNIFORM_FREQ:
        return "AUTO_UNIFORM_FREQ";
    case ORCM_PWRMGMT_MODE_AUTO_DUAL_UNIFORM_FREQ:
        return "AUTO_DUAL_UNIFORM_FREQ";
    case ORCM_PWRMGMT_MODE_AUTO_GEO_GOAL_MAX_PERF:
        return "AUTO_GEO_GOAL_MAX_PERF";
    case ORCM_PWRMGMT_MODE_AUTO_GEO_GOAL_MAX_EFFICIENCY:
        return "AUTO_GEO_GOAL_MAX_EFFICIENCY";
    default:
        return "UNKNOWN POWER MODE";
    }
}	

int orcm_pwrmgmt_get_mode_val(const char* mode)
{
    if(!strncmp(mode, "NONE", 4)) {
        return ORCM_PWRMGMT_MODE_NONE;
    }
    else if(!strncmp(mode, "MANUAL_FREQ", 11)) {
        return ORCM_PWRMGMT_MODE_MANUAL_FREQ;
    }
    else if(!strncmp(mode, "AUTO_UNIFORM_FREQ", 17)) {
        return ORCM_PWRMGMT_MODE_AUTO_UNIFORM_FREQ;
    }
    else if(!strncmp(mode, "AUTO_DUAL_UNIFORM_FREQ", 22)) {
        return ORCM_PWRMGMT_MODE_AUTO_DUAL_UNIFORM_FREQ;
    }
    else if(!strncmp(mode, "AUTO_GEO_GOAL_MAX_PERF", 22)) {
        return ORCM_PWRMGMT_MODE_AUTO_GEO_GOAL_MAX_PERF;
    }
    else if(!strncmp(mode, "AUTO_GEO_GOAL_MAX_EFFICIENCY", 28)) {
        return ORCM_PWRMGMT_MODE_AUTO_GEO_GOAL_MAX_EFFICIENCY;
    }
    else {
        return -1;
    }
}	

