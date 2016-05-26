/*
* Copyright (c) 2016      Intel, Inc. All rights reserved.
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
#include <time.h>
#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/analytics/base/base.h"

#include "orcm/mca/analytics/analytics_types.h"
#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/mca/analytics/threshold/analytics_threshold.h"

#include "orcm/runtime/orcm_globals.h"
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
bool check_threshold(orcm_value_t* current_value,
                     orcm_mca_analytics_threshold_policy_t* threshold_policy,
                     char** msg, char* hostname, int* sev, char** action);

#define ON_NULL_RETURN(x, e)  if(NULL==x) { return e; }
#define ON_ERROR_RETURN(x, rc, label)  if(ORCM_SUCCESS != (rc = x)) { goto label; }

static void threshold_policy_t_con(orcm_mca_analytics_threshold_policy_t *policy)
{
    policy->hi = 0.0;
    policy->hi_action = NULL;
    policy->hi_sev = ORCM_RAS_SEVERITY_INFO;
    policy->low = 0.0;
    policy->low_action = NULL;
    policy->low_sev = ORCM_RAS_SEVERITY_INFO;
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
        NULL
    }
};

static int init(orcm_analytics_base_module_t *imod)
{
    ON_NULL_RETURN(imod, ORCM_ERROR);
    mca_analytics_threshold_module_t* mod = (mca_analytics_threshold_module_t *)imod;
    mod->api.orcm_mca_analytics_event_store = OBJ_NEW(opal_hash_table_t);
    ON_NULL_RETURN(mod->api.orcm_mca_analytics_event_store, ORCM_ERROR);
    opal_hash_table_t* table = (opal_hash_table_t*)mod->api.orcm_mca_analytics_event_store;
    if(ORCM_SUCCESS != opal_hash_table_init(table, HASH_TABLE_SIZE)) {
        return ORCM_ERROR;
    }
    mod->api.orcm_mca_analytics_data_store = OBJ_NEW(orcm_mca_analytics_threshold_policy_t);
    ON_NULL_RETURN(mod->api.orcm_mca_analytics_data_store, ORCM_ERR_OUT_OF_RESOURCE);
    return ORCM_SUCCESS;
}

static void finalize(orcm_analytics_base_module_t *imod)
{
    orcm_mca_analytics_threshold_policy_t* threshold_policy = NULL;
    if (NULL != imod) {
        mca_analytics_threshold_module_t* mod = (mca_analytics_threshold_module_t *)imod;
        opal_hash_table_t* table = (opal_hash_table_t*)mod->api.orcm_mca_analytics_event_store;
        if (NULL != table) {
            opal_hash_table_remove_all(table);
            SAFE_RELEASE(table);
        }
        threshold_policy = (orcm_mca_analytics_threshold_policy_t*)mod->api.orcm_mca_analytics_data_store;
        SAFE_RELEASE(threshold_policy);
        SAFEFREE(mod);
    }
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output, "%s analytics:threshold:finalize",
                       ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
}

bool check_threshold(orcm_value_t* current_value, orcm_mca_analytics_threshold_policy_t* threshold_policy,
                     char** msg, char* hostname, int* sev, char** action)
{
    bool copy = false;
    double val = orcm_util_get_number_orcm_value(current_value);
    if (val >= threshold_policy->hi && NULL != threshold_policy->hi_action) {
        copy = true;
        if (0 < asprintf(&*msg, "%s %s value %.02f %s,greater than threshold %.02f %s", hostname,
                        current_value->value.key, val, current_value->units, threshold_policy->hi, current_value->units)) {
            *sev = threshold_policy->hi_sev;
            *action = threshold_policy->hi_action;
        }
    } else if (val <= threshold_policy->low
            && NULL != threshold_policy->low_action
            && threshold_policy->low != threshold_policy->hi) {
        copy = true;
        if (0 < asprintf(&*msg, "%s %s value %.02f %s, lower than threshold %.02f %s", hostname,
                         current_value->value.key, val, current_value->units, threshold_policy->low, current_value->units)) {
            *sev = threshold_policy->low_sev;
            *action = threshold_policy->low_action;
        }
    }
    return copy;
}

static int monitor_threshold(orcm_workflow_caddy_t *current_caddy,
                             orcm_mca_analytics_threshold_policy_t *threshold_policy,
                             opal_list_t* threshold_list, opal_list_t* event_list)
{
    char* msg = NULL;
    orcm_value_t *current_value = NULL;
    orcm_value_t *analytics_orcm_value = NULL;
    int rc = ORCM_SUCCESS;
    int sev = ORCM_RAS_SEVERITY_INFO;
    char* action = NULL;
    char* hostname = NULL;

    if(NULL == current_caddy || NULL == current_caddy->analytics_value ||
       NULL == current_caddy->analytics_value->compute_data || NULL == threshold_policy || NULL == threshold_list ) {
        return ORCM_ERR_BAD_PARAM;
    }
    hostname = orcm_analytics_get_hostname_from_attributes(current_caddy->analytics_value->key);
    OPAL_LIST_FOREACH(current_value, current_caddy->analytics_value->compute_data, orcm_value_t) {
        if(check_threshold(current_value, threshold_policy, &msg, hostname, &sev, &action)) {
            rc = orcm_analytics_base_gen_notifier_event(current_value, current_caddy, sev, msg,
                                                        action, event_list);
            if (NULL != (analytics_orcm_value = orcm_util_copy_orcm_value(current_value))) {
                opal_list_append(threshold_list, (opal_list_item_t *)analytics_orcm_value);
            }
        }
        SAFEFREE(msg);
    }
    SAFEFREE(msg);
    SAFEFREE(hostname);
    return rc;
}

static int get_threshold_value(char *tval, double* val)
{
    int j = 0;
    ON_NULL_RETURN(tval, ORCM_ERR_BAD_PARAM);
    ON_NULL_RETURN(val, ORCM_ERR_BAD_PARAM);
    for (j = 0; j < (int)strlen(tval); j++) {
        if (!isdigit(tval[j]) && '-' != tval[j] && '+' != tval[j] && '.' != tval[j]) {
            return ORCM_ERR_BAD_PARAM;
        }
    }
    errno = 0;
    *val = strtod(tval, NULL);
    if(0 == *val && 0 != errno) {
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;
}

static int parse_token(char** token, orcm_mca_analytics_threshold_policy_t* threshold_policy)
{
    double val = 0.0;
    int sev = ORCM_RAS_SEVERITY_ERROR;
    int rc = ORCM_SUCCESS;
    if (NULL == token || opal_argv_count(token) != 4) {
        return ORCM_ERR_BAD_PARAM;
    }
    if(ORCM_SUCCESS != (rc = get_threshold_value(token[1], &val))){
        return rc;
    }
    if (NULL != token[2]) {
        sev = orcm_analytics_event_get_severity(token[2]);
    }
    if (0 == strcmp(token[0], "hi")) {
        threshold_policy->hi = val;
        threshold_policy->hi_sev = sev;
        threshold_policy->hi_action = strdup(token[3]);
    } else if (0 == strcmp(token[0], "low")) {
        threshold_policy->low = val;
        threshold_policy->low_sev = sev;
        threshold_policy->low_action = strdup(token[3]);
    } else {
        rc = ORCM_ERR_BAD_PARAM;
    }
    return rc;
}

static int get_threshold_policy(opal_list_t* attributes, orcm_mca_analytics_threshold_policy_t* threshold_policy )
{
    opal_value_t *temp = NULL;
    char **policy = NULL;
    char **token = NULL;
    int rc = ORCM_SUCCESS;
    int count = 0;
    int i = 0;

    ON_NULL_RETURN(attributes, ORCM_ERR_BAD_PARAM);
    ON_NULL_RETURN(threshold_policy, ORCM_ERR_BAD_PARAM);
    temp = (opal_value_t*)opal_list_get_first(attributes);
    ON_NULL_RETURN(temp, ORCM_ERR_BAD_PARAM);
    if (0 != strcmp(temp->key,"policy")) {
        return ORCM_ERR_BAD_PARAM;
    }
    policy = opal_argv_split(temp->data.string,',');
    count = opal_argv_count(policy);
    if(0 == count || 2 < count) {
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }
    for(i = 0; i < count; i++) {
        token = opal_argv_split(policy[i], '|');
        ON_ERROR_RETURN(parse_token(token, threshold_policy), rc, done);
        opal_argv_free(token);
        token = NULL;
    }
done:
    opal_argv_free(policy);
    opal_argv_free(token);
    return rc;
}

static int analyze(int sd, short args, void *cbdata)
{
    int rc = ORCM_SUCCESS;
    opal_list_t* threshold_list = NULL;
    orcm_mca_analytics_threshold_policy_t *threshold_policy = NULL;
    orcm_analytics_value_t *analytics_value_to_next = NULL;
    orcm_workflow_caddy_t *current_caddy = (orcm_workflow_caddy_t*)cbdata;
    opal_list_t* event_list = NULL;

    ON_ERROR_RETURN(orcm_analytics_base_assert_caddy_data(current_caddy), rc, done);
    threshold_policy = (orcm_mca_analytics_threshold_policy_t*) current_caddy->imod->orcm_mca_analytics_data_store;
    if(NULL == threshold_policy->hi_action && NULL == threshold_policy->low_action){
        if(ORCM_SUCCESS != (rc = get_threshold_policy(&current_caddy->wf_step->attributes,threshold_policy))){
            opal_output_verbose(5, orcm_analytics_base_framework.framework_output,
                                 "%s analytics:threshold:Invalid argument/s to workflow step",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            goto done;
        }
    }
    threshold_list = OBJ_NEW(opal_list_t);
    CHECK_NULL_ALLOC(threshold_list, rc, done);
    event_list = OBJ_NEW(opal_list_t);
    CHECK_NULL_ALLOC(event_list, rc, done);
    ON_ERROR_RETURN(monitor_threshold(current_caddy, threshold_policy, threshold_list, event_list), rc, done);
    analytics_value_to_next = orcm_util_load_orcm_analytics_value_compute(current_caddy->analytics_value->key,
                                      current_caddy->analytics_value->non_compute_data, threshold_list);
    CHECK_NULL_ALLOC(analytics_value_to_next, rc, done);
    if(true == orcm_analytics_base_db_check(current_caddy->wf_step)){
        ON_ERROR_RETURN(orcm_analytics_base_log_to_database_event(analytics_value_to_next), rc, done);
    }
    if(0 == event_list->opal_list_length){
        SAFE_RELEASE(event_list);
    }
    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(current_caddy->wf,current_caddy->wf_step,
                                     current_caddy->hash_key, analytics_value_to_next, event_list);
done:
    SAFE_RELEASE(current_caddy);
    if(ORCM_SUCCESS != rc){
        SAFE_RELEASE(event_list);
        SAFE_RELEASE(threshold_list);
    }
    return rc;
}
