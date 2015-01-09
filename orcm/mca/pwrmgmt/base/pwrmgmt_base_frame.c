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

int set_max_freq(opal_pointer_array_t *daemons);
int set_to_idle(opal_pointer_array_t *daemons);
int reset_freq_defaults(opal_pointer_array_t *daemons);
static int send_to_daemons(int command, opal_pointer_array_t *daemons);
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
    orcm_pwrmgmt_base_get_attributes,
    orcm_pwrmgmt_base_get_current_power
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

    /* initialize framework base functions */
    orcm_pwrmgmt_base.set_max_freq = set_max_freq;
    orcm_pwrmgmt_base.set_to_idle = set_to_idle;
    orcm_pwrmgmt_base.reset_freq_defaults = reset_freq_defaults;

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


static int send_to_daemons(int command, opal_pointer_array_t *daemons) {
    int rc;
    orte_process_name_t* proc;
    int sz = opal_pointer_array_get_size(daemons);
    opal_buffer_t* buffer;
    orcm_rm_cmd_flag_t cmd = command;

    buffer = OBJ_NEW(opal_buffer_t);

    if (ORTE_SUCCESS != (rc = opal_dss.pack(buffer, &cmd, 1, ORCM_RM_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    for(int n = 0; n < sz; n++) {
        proc = opal_pointer_array_get_item(daemons, n);
        
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(proc, buffer,
                                      ORCM_RML_TAG_PWRMGMT_BASE,
                                      orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buffer);
           return rc;
        } 
    }

    return OPAL_SUCCESS;
}
 
int set_max_freq(opal_pointer_array_t *daemons) {
    return send_to_daemons(ORCM_PWRMGMT_BASE_MAX_FREQ_CMD, daemons);
} 

int set_to_idle(opal_pointer_array_t *daemons) {
    return send_to_daemons(ORCM_PWRMGMT_BASE_SET_IDLE_CMD, daemons);
} 

int reset_freq_defaults(opal_pointer_array_t *daemons) {
    return send_to_daemons(ORCM_PWRMGMT_BASE_RESET_DEFAULTS_CMD, daemons);
}

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
    opal_list_t* data = NULL;
    opal_value_t *kv;
    bool use_perf_gov = false;
    float freq = 0.0;

    case ORCM_PWRMGMT_BASE_MAX_FREQ_CMD:
        orcm_pwrmgmt_freq_init();
        orcm_pwrmgmt_freq_get_supported_governors(0, data);
        if(NULL != data) {
            OPAL_LIST_FOREACH(kv, data, opal_value_t) {
                if(0 == strcmp(kv->data.string, "performance")) {
                    use_perf_gov = true;
                }
            }
            if(use_perf_gov) {
                orcm_pwrmgmt_freq_set_governor(-1, "performance");
                break;
            }
            OPAL_LIST_FOREACH(kv, data, opal_value_t) {
                if(0 == strcmp(kv->data.string, "userspace")) {
                    orcm_pwrmgmt_freq_set_governor(-1, "userspace");
                    break;
                }
            }
        }
        orcm_pwrmgmt_freq_get_supported_frequencies(0, data);
        if(NULL == data) {
            break;
        }
        if(opal_list_is_empty(data)) {
           break;
        }
        freq = ((opal_value_t*)opal_list_get_first(data))->data.fval;
        OPAL_LIST_FOREACH(kv, data, opal_value_t) {
            if(kv->data.fval > freq) {
                freq = kv->data.fval;
            }
        }
        if(0.0 != freq) {
            orcm_pwrmgmt_freq_set_max_freq(-1, freq);
            orcm_pwrmgmt_freq_set_min_freq(-1, freq);
        }
    break;
    case ORCM_PWRMGMT_BASE_SET_IDLE_CMD:
        orcm_pwrmgmt_freq_init();
        orcm_pwrmgmt_freq_get_supported_governors(0, data);
        if(NULL != data) {
            OPAL_LIST_FOREACH(kv, data, opal_value_t) {
                if(0 == strcmp(kv->data.string, "userspace")) {
                    orcm_pwrmgmt_freq_set_governor(-1, "userspace");
                    break;
                }
            }
            orcm_pwrmgmt_freq_get_supported_frequencies(0, data);
            if(opal_list_is_empty(data)) {
               break;
            }
        }
        freq = ((opal_value_t*)opal_list_get_first(data))->data.fval;
        if(NULL == data) {
            break;
        }
        OPAL_LIST_FOREACH(kv, data, opal_value_t) {
            if(kv->data.fval < freq) {
                freq = kv->data.fval;
            }
        }
        if(0.0 != freq) {
            orcm_pwrmgmt_freq_set_max_freq(-1, freq);
            orcm_pwrmgmt_freq_set_min_freq(-1, freq);
        }
    break;
    case ORCM_PWRMGMT_BASE_RESET_DEFAULTS_CMD:
        orcm_pwrmgmt_freq_reset_system_settings();
    break;
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
