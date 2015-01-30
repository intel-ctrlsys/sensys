/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#include <stdio.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */
#include <ctype.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

#include "opal_stdint.h"
#include "opal/class/opal_list.h"
#include "opal/dss/dss.h"
#include "opal/util/os_path.h"
#include "opal/util/output.h"
#include "opal/util/os_dirpath.h"

#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/routed/routed.h"
#include "orte/mca/grpcomm/grpcomm.h"
#include "orte/mca/grpcomm/base/base.h"
#include "orte/runtime/orte_wait.h"
#include "orte/util/regex.h"

#include "orcm/mca/db/db.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/cfgi/cfgi_types.h"
#include "orcm/mca/scd/base/base.h"
#include "orcm/mca/pwrmgmt/base/base.h"
#include "orcm/mca/pwrmgmt/base/pwrmgmt_freq_utils.h"
#include "pwrmgmt_manualfreq.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
static int  alloc_notify(orcm_alloc_t* alloc);
static void dealloc_notify(orcm_alloc_t* alloc);
static int component_select(orcm_session_id_t session, opal_list_t* attr);
static int set_attributes(orcm_session_id_t session, opal_list_t* attr);
static int reset_attributes(orcm_session_id_t session, opal_list_t* attr);
static int get_attributes(orcm_session_id_t session, opal_list_t* attr);

/* instantiate the module */
orcm_pwrmgmt_base_API_module_t orcm_pwrmgmt_manualfreq_module = {
    init,
    finalize,
    component_select,
    alloc_notify,
    dealloc_notify,
    set_attributes,
    reset_attributes,
    get_attributes
};

const char* component_name = "manual_frequency";
static double frequency = -1.0;

static int init(void)
{
    return ORCM_SUCCESS;
}

static void finalize(void)
{
}

static int component_select(orcm_session_id_t session, opal_list_t* attr) 
{
    opal_output_verbose(5, orcm_pwrmgmt_base_framework.framework_output,
                        "%s pwrmgmt:manualfreq: component select called",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    int32_t mode, *mode_ptr;
    char* name;
    opal_list_t* data = NULL;
    opal_value_t *kv;
    bool governor_supported = false;

    mode_ptr = &mode;
    if (true != orte_get_attribute(attr, ORCM_PWRMGMT_POWER_MODE_KEY, (void**)&mode_ptr, OPAL_INT32)) {
        opal_output(0, "pwrmgmt:manualfreq: no mode was specified in constraints");
        return ORCM_ERROR;
    }
    if(ORCM_PWRMGMT_MODE_MANUAL_FREQ != mode) {
        //we cannot handle this request
        return ORCM_ERROR;
    }

    if(!ORTE_PROC_IS_SCHEDULER) {
        if (true != orte_get_attribute(attr, ORCM_PWRMGMT_SELECTED_COMPONENT_KEY, (void**)&name, OPAL_STRING)) {
            opal_output(0, "pwrmgmt:manualfreq: No global component has been chosen");
            //The scheduler should have selected a component
            return ORCM_ERROR;
        }
        if(strncmp(name, component_name, strlen(component_name)) ) { 
            //We can handle this mode, but we are not the selected component
            return ORCM_ERROR;
        }
    }

    if(ORTE_PROC_IS_DAEMON) {
        //We require the userspace governor in order to operate
        orcm_pwrmgmt_freq_init();
        orcm_pwrmgmt_freq_get_supported_governors(0, &data);
        if(NULL != data) {
            OPAL_LIST_FOREACH(kv, data, opal_value_t) {
                if(0 == strcmp(kv->data.string, "userspace")) {
                    governor_supported = true;
                    break;
                }
            }
        }
        
        if (false == governor_supported) {
            opal_output(0, "pwrmgmt:manualfreq: userspace governor is not supported");
            return ORCM_ERROR;
        }
    }
    return ORCM_SUCCESS;
}

static int set_attributes(orcm_session_id_t session, opal_list_t* attr) 
{
    opal_output_verbose(5, orcm_pwrmgmt_base_framework.framework_output,
                        "%s pwrmgmt:manualfreq: set attributes called",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    int32_t mode, *mode_ptr;
    float freq, *freq_ptr;
    bool strict = true;
    bool *strict_ptr;
    float epsilon;
    float minval;
    int rc;
    opal_list_t* data = NULL;
    opal_value_t *kv;
    mode_ptr = &mode;
    freq_ptr = &freq;
    strict_ptr = &strict;

    if (true != orte_get_attribute(attr, ORCM_PWRMGMT_POWER_MODE_KEY, (void**)&mode_ptr, OPAL_INT32)) {
        opal_output(0, "pwrmgmt:manualfreq: no mode was specified in constraints");
        return ORCM_ERROR;
    }
    if (ORCM_PWRMGMT_MODE_MANUAL_FREQ != mode) {
        opal_output(0, "pwrmgmt:manualfreq: Got an incorrect mode for this component");
        return ORCM_ERROR;
    }
    if (true != orte_get_attribute(attr, ORCM_PWRMGMT_MANUAL_FREQUENCY_KEY, (void**)&freq_ptr, OPAL_FLOAT)) {
        //Nothing to do
        return ORCM_SUCCESS;
    }
    if(fabsf(freq - ORCM_PWRMGMT_MAX_FREQ) < 0.0001) {
        orcm_pwrmgmt_freq_get_supported_frequencies(0, &data);
        frequency = ((opal_value_t*)opal_list_get_first(data))->data.fval;
        if (ORCM_SUCCESS != (rc = orcm_pwrmgmt_freq_set_max_freq(-1, frequency))) {
            return rc;
        }
        if (ORCM_SUCCESS != (rc = orcm_pwrmgmt_freq_set_min_freq(-1, frequency))) {
            return rc;
        }
        return ORCM_SUCCESS;
    }
    if(fabsf(freq - ORCM_PWRMGMT_MIN_FREQ) < 0.0001) {
        orcm_pwrmgmt_freq_get_supported_frequencies(0, &data);
        frequency = ((opal_value_t*)opal_list_get_last(data))->data.fval;
        if (ORCM_SUCCESS != (rc = orcm_pwrmgmt_freq_set_max_freq(-1, frequency))) {
            return rc;
        }
        if (ORCM_SUCCESS != (rc = orcm_pwrmgmt_freq_set_min_freq(-1, frequency))) {
            return rc;
        }
        return ORCM_SUCCESS;
    }

    orte_get_attribute(attr, ORCM_PWRMGMT_FREQ_STRICT_KEY, (void**)&strict_ptr, OPAL_BOOL);

    if (ORTE_PROC_IS_DAEMON) {
        if (0.0 < freq && frequency != freq) { 
            orcm_pwrmgmt_freq_get_supported_frequencies(0, &data);
            if (opal_list_is_empty(data)) {
               return ORCM_ERROR;
            }
            //Find the closest supported frequency to the requested frequency
            frequency = ((opal_value_t*)opal_list_get_first(data))->data.fval;
            minval = fabs(freq - frequency);
            OPAL_LIST_FOREACH(kv, data, opal_value_t) {
                epsilon = fabs(freq - kv->data.fval);
                if (epsilon < minval) {
                    minval = epsilon;
                    frequency = kv->data.fval;
                }
            }
            if(strict && minval > 0.0001) {
                return ORCM_ERR_BAD_PARAM;
            }
            if (0.0 < frequency) {
                opal_output_verbose(1, orcm_pwrmgmt_base_framework.framework_output,
                                    "%s pwrmgmt:manualfreq: setting frequency to %lf GHz",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                    frequency);

                if (ORCM_SUCCESS != (rc = orcm_pwrmgmt_freq_set_max_freq(-1, frequency))) {
                    return rc;
                }
                if (ORCM_SUCCESS != (rc = orcm_pwrmgmt_freq_set_min_freq(-1, frequency))) {
                    return rc;
                }
            }
        }
    }

    return ORCM_SUCCESS;
}

static int reset_attributes(orcm_session_id_t session, opal_list_t* attr) 
{
    opal_output_verbose(5, orcm_pwrmgmt_base_framework.framework_output,
                        "%s pwrmgmt:manualfreq: reset attributes called",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    int32_t mode, *mode_ptr;
    char* name;

    mode_ptr = &mode;

    if (true != orte_get_attribute(attr, ORCM_PWRMGMT_POWER_MODE_KEY, (void**)&mode_ptr, OPAL_INT32)) {
        opal_output(0, "pwrmgmt:manualfreq: no mode was specified in constraints");
        return ORCM_ERROR;
    }

    if(ORCM_PWRMGMT_MODE_MANUAL_FREQ != mode) {
        opal_output(0, "pwrmgmt:manualfreq: Got an incorrect mode for this component");
        return ORCM_ERROR;
    }

    if (true != orte_get_attribute(attr, ORCM_PWRMGMT_SELECTED_COMPONENT_KEY, (void**)&name, OPAL_STRING)) {
        //Nothing to do
        return ORCM_SUCCESS;
    }
    if(strncmp(name, component_name, strlen(component_name)) ) {
        //we are not the selected component
        return ORCM_ERROR;
    }

    orte_remove_attribute(attr, ORCM_PWRMGMT_SELECTED_COMPONENT_KEY);

    return ORCM_SUCCESS;
}

static int get_attributes(orcm_session_id_t session, opal_list_t* attr) 
{
    opal_output_verbose(5, orcm_pwrmgmt_base_framework.framework_output,
                        "%s pwrmgmt:manualfreq: get attributes called",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    int32_t mode = ORCM_PWRMGMT_MODE_MANUAL_FREQ;
    int rc;
 
    if (ORCM_SUCCESS != (rc = orte_set_attribute(attr, ORCM_PWRMGMT_POWER_MODE_KEY, ORTE_ATTR_GLOBAL, &mode, OPAL_INT32))) {
        return ORCM_ERROR;
    }

    if (ORCM_SUCCESS != (rc = orte_set_attribute(attr, ORCM_PWRMGMT_SELECTED_COMPONENT_KEY, ORTE_ATTR_GLOBAL, &component_name, OPAL_STRING))) {
        return ORCM_ERROR;
    }

    if (ORCM_SUCCESS != (rc = orte_set_attribute(attr, ORCM_PWRMGMT_MANUAL_FREQUENCY_KEY, ORTE_ATTR_GLOBAL, &frequency, OPAL_FLOAT))) {
        return ORCM_ERROR;
    }

    return ORCM_SUCCESS;
}

int alloc_notify(orcm_alloc_t* alloc)
{
    opal_output_verbose(5, orcm_pwrmgmt_base_framework.framework_output,
                        "%s pwrmgmt:manualfreq: alloc_notify called",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    int32_t mode, *mode_ptr;
    int rc;

    mode_ptr = &mode;
    if (true != orte_get_attribute(&alloc->constraints, ORCM_PWRMGMT_POWER_MODE_KEY, (void**)&mode_ptr, OPAL_INT32)) {
        opal_output(0, "pwrmgmt:manualfreq: no mode was specified in constraints"); 
        return ORCM_ERR_BAD_PARAM;
    }

    if (ORCM_PWRMGMT_MODE_MANUAL_FREQ != mode) {
        opal_output(0, "pwrmgmt:manualfreq: Got an incorrect mode for this component");
        return ORCM_ERR_BAD_PARAM;
    }

    if (ORTE_PROC_IS_SCHEDULER) {
        //We have been selected. Let's add our component string to the attributes.
        if (ORCM_SUCCESS != (rc = orte_set_attribute(&alloc->constraints, ORCM_PWRMGMT_SELECTED_COMPONENT_KEY, ORTE_ATTR_GLOBAL, (void*)component_name, OPAL_STRING))) {
            return rc;
        }
    }

    if (ORTE_PROC_IS_DAEMON) {
        opal_output_verbose(1, orcm_pwrmgmt_base_framework.framework_output,
                            "%s pwrmgmt:manualfreq: setting governor to userspace",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

        orcm_pwrmgmt_freq_set_governor(-1, "userspace");
        set_attributes(alloc->id, &alloc->constraints);
    }
    return ORCM_SUCCESS;
}

void dealloc_notify(orcm_alloc_t* alloc)
{
    opal_output_verbose(5, orcm_pwrmgmt_base_framework.framework_output,
                        "%s pwrmgmt:manualfreq: dealloc_notify called",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    opal_output_verbose(1, orcm_pwrmgmt_base_framework.framework_output,
                        "%s pwrmgmt:manualfreq: resetting governor and frequency to defaults",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    orcm_pwrmgmt_freq_reset_system_settings();
}

