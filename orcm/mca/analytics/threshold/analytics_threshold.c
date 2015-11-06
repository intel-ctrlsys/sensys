/*
* Copyright (c) 2014      Intel, Inc. All rights reserved.
* $COPYRIGHT$
*
* Additional copyrights may follow
*
* $HEADER$
*/

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/analytics/base/base.h"

#include "orcm/mca/analytics/analytics_types.h"
#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/mca/analytics/threshold/analytics_threshold.h"

#include "orcm/runtime/orcm_globals.h"
#include "orte/mca/notifier/notifier.h"
#include "opal/class/opal_list.h"

#include <stdarg.h>
#include <limits.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include "orcm/util/utils.h"


static int init(orcm_analytics_base_module_t *imod);
static void finalize(orcm_analytics_base_module_t *imod);
static int analyze(int sd, short args, void *cbdata);


static void threshold_policy_t_con(orcm_mca_analytics_threshold_policy_t *policy)
{
    policy->hi = 0.0;
    policy->hi_action = NULL;
    policy->hi_sev = ORTE_NOTIFIER_INFO;
    policy->low = 0.0;
    policy->low_action = NULL;
    policy->low_sev = ORTE_NOTIFIER_INFO;
}

static void threshold_policy_t_des(orcm_mca_analytics_threshold_policy_t *policy)
{
    SAFEFREE(policy->hi_action);
    SAFEFREE(policy->low_action);
}

OBJ_CLASS_INSTANCE(orcm_mca_analytics_threshold_policy_t,
                   opal_object_t,
                   threshold_policy_t_con, threshold_policy_t_des);

mca_analytics_threshold_module_t orcm_analytics_threshold_module = {
    {
        init,
        finalize,
        analyze,
        NULL,
    }
};

static int init(orcm_analytics_base_module_t *imod)
{
    if(NULL == imod) {
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

static void finalize(orcm_analytics_base_module_t *imod)
{
OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                     "%s analytics:threshold:finalize",
                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
}

static orte_notifier_severity_t get_severity(char* severity)
{
    orte_notifier_severity_t sev;
    if ( 0 == strcmp(severity, "emerg") ) {
        sev = ORTE_NOTIFIER_EMERG;
    } else if ( 0 == strcmp(severity, "alert") ) {
        sev = ORTE_NOTIFIER_ALERT;
    } else if ( 0 == strcmp(severity, "crit") ) {
        sev = ORTE_NOTIFIER_CRIT;
    } else if ( 0 == strcmp(severity, "error") ) {
        sev = ORTE_NOTIFIER_ERROR;
    } else if ( 0 == strcmp(severity, "warn") ) {
        sev = ORTE_NOTIFIER_WARN;
    } else if ( 0 == strcmp(severity, "notice") ) {
        sev = ORTE_NOTIFIER_NOTICE;
    } else if ( 0 == strcmp(severity, "info") ) {
        sev = ORTE_NOTIFIER_INFO;
    } else if ( 0 == strcmp(severity, "debug") ) {
        sev = ORTE_NOTIFIER_DEBUG;
    } else {
        sev = ORTE_NOTIFIER_INFO;
    }
    return sev;
}

static int monitor_threshold(orcm_workflow_caddy_t *current_caddy,
                             orcm_mca_analytics_threshold_policy_t *threshold_policy,
                             opal_list_t* threshold_list)
{
    char* msg1 = NULL;
    char* msg2 = NULL;
    char* action_hi = NULL;
    char* action_low = NULL;
    orcm_value_t *current_value = NULL;
    orcm_value_t *analytics_orcm_value = NULL;
    opal_list_t *sample_data_list = NULL;
    int rc = ORCM_SUCCESS;
    double val = 0.0;

    if(NULL == current_caddy || NULL == threshold_policy || NULL == threshold_list) {
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }
    sample_data_list = current_caddy->analytics_value->compute_data;
    if (NULL == sample_data_list) {
        rc = ORCM_ERROR;
        goto done;
    }
    if(NULL != threshold_policy->hi_action) {
        action_hi = strdup(threshold_policy->hi_action);
    }
    if(NULL != threshold_policy->low_action) {
        action_low = strdup(threshold_policy->low_action);
    }
    OPAL_LIST_FOREACH(current_value, sample_data_list, orcm_value_t) {
        if(NULL == current_value){
            rc = ORCM_ERROR;
            goto done;
        }
        val = orcm_util_get_number_orcm_value(current_value);

        if(val >= threshold_policy->hi && NULL != action_hi){
            if(0 < asprintf(&msg1, "%s value %.02f %s,greater than threshold %.02f %s",
                            current_value->value.key,val,current_value->units,threshold_policy->hi,current_value->units)){
                OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                                     "%s analytics:threshold:%s",ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), msg1));
                if(0 != strcmp(action_hi,"none")) {
                    ORTE_NOTIFIER_SYSTEM_EVENT(threshold_policy->hi_sev,msg1,action_hi);
                }
            }
            analytics_orcm_value = orcm_util_copy_orcm_value(current_value);
            if(NULL != analytics_orcm_value) {
                opal_list_append(threshold_list, (opal_list_item_t *)analytics_orcm_value);
            }
        }
        else if(val <= threshold_policy->low && NULL != action_low && threshold_policy->low != threshold_policy->hi) {
            if(0 < asprintf(&msg2, "%s value %.02f %s, lower than threshold %.02f %s",
                            current_value->value.key,val,current_value->units,threshold_policy->low,current_value->units)) {
                OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                                     "%s analytics:threshold:%s",ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), msg2));
                if(0 != strcmp(action_low, "none")) {
                    ORTE_NOTIFIER_SYSTEM_EVENT(threshold_policy->low_sev,msg2,action_low);
                }
            }
            analytics_orcm_value = orcm_util_copy_orcm_value(current_value);
            if(NULL != analytics_orcm_value) {
                opal_list_append(threshold_list, (opal_list_item_t *)analytics_orcm_value);
            }
        }
    }
done:
    SAFEFREE(action_hi);
    SAFEFREE(action_low);
    return rc;
}

static int get_threshold_policy(void *cbdata,orcm_mca_analytics_threshold_policy_t* threshold_policy )
{
    opal_value_t *temp = NULL;
    char **policy = NULL;
    char **token = NULL;
    char* action_hi = NULL;
    char* severity = NULL;
    char* action_low = NULL;
    orte_notifier_severity_t sev = ORTE_NOTIFIER_ERROR;
    orcm_workflow_caddy_t *current_caddy = NULL;
    char* label = strdup("policy");
    int rc = ORCM_SUCCESS;
    double val;
    int count=0, i=0, j=0;
    char* tval = NULL;

    if(NULL == cbdata || NULL == threshold_policy) {
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }
    current_caddy = (orcm_workflow_caddy_t *)cbdata;
    temp = (opal_value_t*)opal_list_get_first(&current_caddy->wf_step->attributes);
    if(NULL == temp) {
        rc = ORCM_ERR_NOT_FOUND;
        goto done;
    }

    if (0 == strcmp(temp->key,label)) {
        policy = opal_argv_split(temp->data.string,',');
        count = opal_argv_count(policy);
        if(0 == count || 2 < count) {
            rc = ORCM_ERR_BAD_PARAM;
            goto done;
        }
        for(i=0; i<count; i++) {
            token = opal_argv_split(policy[i], '|');
            if(NULL == token || opal_argv_count(token) != 4) {
                rc = ORCM_ERR_BAD_PARAM;
                goto done;
            }

            tval = strdup(token[1]);
            if(NULL == tval) {
                rc = ORCM_ERR_BAD_PARAM;
                goto done;
            }
            for (j=0; j < (int)strlen(tval); j++) {
                if (!isdigit(tval[i]) && '-' != tval[i] && '+' != tval[i] && '.' != tval[i]) {
                    rc = ORCM_ERR_BAD_PARAM;
                    goto done;
                }
            }
            errno = 0;
            val = strtod(token[1], NULL);
            if(0 == val && 0 != errno) {
                rc = ORCM_ERR_BAD_PARAM;
                goto done;
            }

            severity = strdup(token[2]);
            if(NULL != severity) {
            sev = get_severity(severity);
            }

            if(0 == strcmp(token[0],"hi")) {
                threshold_policy->hi = val;
                threshold_policy->hi_sev = sev;
                action_hi = strdup(token[3]);
                threshold_policy->hi_action = action_hi;
            }
            else if(0 == strcmp(token[0],"low")) {
                threshold_policy->low = val;
                threshold_policy->low_sev = sev;
                action_low = strdup(token[3]);
                threshold_policy->low_action = action_low;
            }
            else {
                rc = ORCM_ERR_BAD_PARAM;
                goto done;
            }
            SAFEFREE(tval);
            SAFEFREE(severity);
            opal_argv_free(token);
            token = NULL;
        }
    }
    else {
        rc = ORCM_ERR_BAD_PARAM;
    }
done:
    SAFEFREE(label);
    SAFEFREE(tval);
    SAFEFREE(severity);
    opal_argv_free(policy);
    policy = NULL;
    opal_argv_free(token);
    token = NULL;
    return rc;
}

static int analyze(int sd, short args, void *cbdata)
{
    int rc = ORCM_SUCCESS;
    opal_list_t* threshold_list = NULL;
    orcm_mca_analytics_threshold_policy_t *threshold_policy = NULL;
    orcm_analytics_value_t *analytics_value_to_next = NULL;
    orcm_workflow_caddy_t *current_caddy = NULL;

    if (ORCM_SUCCESS != orcm_analytics_base_assert_caddy_data(cbdata)) {
        return ORCM_ERROR;
    }

    current_caddy = (orcm_workflow_caddy_t *)cbdata;

    threshold_policy = OBJ_NEW(orcm_mca_analytics_threshold_policy_t);
    if(NULL == threshold_policy){
        rc = ORCM_ERR_OUT_OF_RESOURCE;
        goto done;
    }
    rc = get_threshold_policy(cbdata,threshold_policy);
    if(ORCM_SUCCESS != rc) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                        "%s analytics:threshold:Invalid argument/s to workflow step",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        goto done;
    }

    threshold_list = OBJ_NEW(opal_list_t);
    if(NULL == threshold_list || NULL == threshold_policy){
        rc = ORCM_ERR_OUT_OF_RESOURCE;
        goto done;
    }
    rc = monitor_threshold(current_caddy, threshold_policy, threshold_list);
    if(ORCM_SUCCESS != rc){
        goto done;
    }
    analytics_value_to_next = OBJ_NEW(orcm_analytics_value_t);
    if(NULL != threshold_list)
    {
        analytics_value_to_next = orcm_util_load_orcm_analytics_value_compute(current_caddy->analytics_value->key,
                                          current_caddy->analytics_value->non_compute_data, threshold_list);
        if(NULL != analytics_value_to_next) {
            ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(current_caddy->wf,current_caddy->wf_step,
                                             current_caddy->hash_key, analytics_value_to_next);
        }
    }
done:
    if(NULL != current_caddy) {
        OBJ_RELEASE(current_caddy);
    }
    if(NULL != threshold_policy){
        OBJ_RELEASE(threshold_policy);
    }
    return rc;
}
