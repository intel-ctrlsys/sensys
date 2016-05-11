/*
 * Copyright (c) 2015-2016 Intel, Inc. All rights reserved.
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
#include <regex.h>
#include <fnmatch.h>

#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/mca/analytics/genex/analytics_genex.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/notifier/base/base.h"

#define IF_NULL(x) if(NULL==x) { ORTE_ERROR_LOG(ORCM_ERROR); return ORCM_ERROR; }
#define IF_NULL_ALLOC(x) if(NULL==x) { ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE); return ORCM_ERR_OUT_OF_RESOURCE; }

static int init(orcm_analytics_base_module_t *imod);
static void finalize(orcm_analytics_base_module_t *imod);
static int analyze(int sd, short args, void *cbdata);
int monitor_genex(genex_workflow_value_t *workflow_value, void* cbdata, opal_list_t* event_list);
int get_genex_policy(void *cbdata, genex_workflow_value_t *workflow_value);
void dest_genex_workflow_value(genex_workflow_value_t *workflow_value, orcm_workflow_caddy_t *genex_analyze_caddy);

mca_analytics_genex_module_t orcm_analytics_genex_module = {{
    init,
    finalize,
    analyze,
    NULL,
    NULL
}};

void dest_genex_workflow_value(genex_workflow_value_t *workflow_value,
                               orcm_workflow_caddy_t *genex_analyze_caddy)
{
    SAFEFREE(workflow_value->msg_regex_label);
    SAFEFREE(workflow_value->severity_label);
    SAFEFREE(workflow_value->notifier_label);
    SAFEFREE(workflow_value->db_label);
    SAFEFREE(workflow_value->db);
    SAFEFREE(workflow_value->notifier);
    SAFEFREE(workflow_value->msg_regex);
    SAFEFREE(workflow_value->severity);
    SAFEFREE(workflow_value);
    SAFE_RELEASE(genex_analyze_caddy);
}

static int init(orcm_analytics_base_module_t *imod)
{
    int rc = 0;

    IF_NULL(imod);
    mca_analytics_genex_module_t* mod = (mca_analytics_genex_module_t *)imod;

    mod->api.orcm_mca_analytics_event_store = OBJ_NEW(opal_hash_table_t);
    IF_NULL(mod->api.orcm_mca_analytics_event_store);

    opal_hash_table_t* table = (opal_hash_table_t*)mod->api.orcm_mca_analytics_event_store;
    rc = opal_hash_table_init(table, HASH_TABLE_SIZE);
    ORCM_ON_NULL_RETURN_ERROR(rc, ORCM_ERROR);

    return ORCM_SUCCESS;
}

static void finalize(orcm_analytics_base_module_t *imod)
{
    if (NULL != imod) {
        mca_analytics_genex_module_t* mod = (mca_analytics_genex_module_t *)imod;
        opal_hash_table_t* table = (opal_hash_table_t*)mod->api.orcm_mca_analytics_event_store;
        if (NULL != table) {
            opal_hash_table_remove_all(table);
            OBJ_RELEASE(table);
        }
        SAFEFREE(mod);
    }
}

/**
 * @brief Function that parses the information (arguments) on the
 *        workflow file and loads it into the genex-workflow structure.
 *        Usually this arguments are user defined.
 *
 * @param cbdata Pointer to a structure that contains the information
 *        of the arguments to be loaded to the genex-workflow structure.
 *
 * @param workflow_value Pointer to the genex-workflow structure in which
 *        the arguments will be loaded
 */
int get_genex_policy(void *cbdata, genex_workflow_value_t *workflow_value)
{
    orcm_workflow_caddy_t *parse_workflow_caddy = NULL;
    opal_value_t *temp = NULL;

    IF_NULL(workflow_value);

    workflow_value->msg_regex_label = strdup("msg_regex");
    workflow_value->severity_label = strdup("severity");
    workflow_value->notifier_label = strdup("notifier");
    workflow_value->db_label = strdup("db");
    workflow_value->msg_regex = NULL;
    workflow_value->severity = NULL;
    workflow_value->notifier = NULL;
    workflow_value->db = NULL;

    IF_NULL(cbdata);

    parse_workflow_caddy = (orcm_workflow_caddy_t *) cbdata;
    if (ORCM_SUCCESS != orcm_analytics_base_assert_caddy_data(cbdata)) {
        return ORCM_ERROR;
    }

    OPAL_LIST_FOREACH(temp, &parse_workflow_caddy->wf_step->attributes, opal_value_t) {
        IF_NULL(temp);
        IF_NULL(temp->key);
        IF_NULL(temp->data.string);

        if (0 == strncmp(temp->key, workflow_value->msg_regex_label,strlen(temp->key))) {
            if (NULL == workflow_value->msg_regex) {
                workflow_value->msg_regex = strdup(temp->data.string);
            }
        } else if (0 == strncmp(temp->key, workflow_value->severity_label,strlen(temp->key))) {
            if (NULL == workflow_value->severity) {
                workflow_value->severity = strdup(temp->data.string);
            }
        } else if (0 == strncmp(temp->key, workflow_value->notifier_label,strlen(temp->key))) {
            if (NULL == workflow_value->notifier) {
                workflow_value->notifier = strdup(temp->data.string);
            }
        } else if (0 == strncmp(temp->key, workflow_value->db_label,strlen(temp->key))) {
            if (NULL == workflow_value->db) {
                workflow_value->db = strdup(temp->data.string);
            }
        }
    }

    return ORCM_SUCCESS;
}

int monitor_genex(genex_workflow_value_t *workflow_value, void* cbdata, opal_list_t* event_list)
{
    int rc = ORCM_SUCCESS;
    orcm_workflow_caddy_t *caddy = NULL;
    orcm_value_t *analytics_value = NULL;
    opal_list_t *sample_data_list = NULL;
    genex_value_t genex_value;
    regex_t regex_comp_wflow;
    char *error_buffer;
    int regex_res;

    IF_NULL(workflow_value);
    IF_NULL(workflow_value->severity);
    IF_NULL(workflow_value->msg_regex);

    genex_value.message = NULL;
    genex_value.severity = NULL;

    regex_res = regcomp(&regex_comp_wflow, workflow_value->msg_regex, REG_EXTENDED);
    if (regex_res) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                             "%s analytics:genex:Invalid regular expression at wflow",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ORCM_ERROR;
    }

    IF_NULL(cbdata);

    caddy = (orcm_workflow_caddy_t *) cbdata;
    sample_data_list = caddy->analytics_value->compute_data;
    IF_NULL(sample_data_list);

    OPAL_LIST_FOREACH(analytics_value, sample_data_list, orcm_value_t) {
        IF_NULL(analytics_value);

        if (0 == strcmp("severity", analytics_value->value.key)) {
            genex_value.severity = analytics_value->value.data.string;
        }

        if( NULL != strstr(analytics_value->value.key, "message")) {
            genex_value.message = analytics_value->value.data.string;
        }

        if (NULL != genex_value.message && NULL != genex_value.severity) {
            regex_res=regexec(&regex_comp_wflow, genex_value.message, 0, NULL, 0);
            if (!regex_res && 0 == strcmp(genex_value.severity, workflow_value->severity))
            {
                OPAL_OUTPUT_VERBOSE((5,orcm_analytics_base_framework.framework_output,
                    "%s analytics:genex: MATCHES USER PARAMS: \"%s\" ",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), genex_value.message));

                rc = orcm_analytics_base_gen_notifier_event(analytics_value, caddy,
                           orcm_analytics_event_get_severity(workflow_value->severity),
                           (char*)genex_value.message, workflow_value->notifier, event_list);
                if ( ORCM_SUCCESS != rc ) {
                    return rc;
                }
             } else if(regex_res && regex_res != REG_NOMATCH) {
                error_buffer = malloc(100);
                IF_NULL_ALLOC(error_buffer);

                regerror(regex_res, &regex_comp_wflow, error_buffer, 100);
                OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                                     "%s analytics:genex: %s",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), error_buffer));
                regfree(&regex_comp_wflow);
                SAFEFREE(error_buffer);
                return ORCM_ERROR;
            }
        }
    }
    return ORCM_SUCCESS;
}

static int analyze(int sd, short args, void *cbdata)
{
    int rc = -1;
    orcm_workflow_caddy_t *genex_analyze_caddy = NULL;
    orcm_analytics_value_t *analytics_value_to_next = NULL;
    genex_workflow_value_t *workflow_value = NULL;
    opal_list_t* event_list = NULL;

    if (ORCM_SUCCESS != (rc = orcm_analytics_base_assert_caddy_data(cbdata))) {
        return ORCM_ERROR;
    }

    workflow_value = (genex_workflow_value_t *)malloc(sizeof(genex_workflow_value_t));
    IF_NULL_ALLOC(workflow_value);

    genex_analyze_caddy = (orcm_workflow_caddy_t *) cbdata;

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                        "%s analytics:%s(step %d):analyze ", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        genex_analyze_caddy->wf_step->analytic, genex_analyze_caddy->wf_step->step_id));

    rc = get_genex_policy(cbdata, workflow_value);
    if (ORCM_SUCCESS != rc) {
        dest_genex_workflow_value(workflow_value, genex_analyze_caddy);
        return ORCM_ERROR;
    }

    event_list = OBJ_NEW(opal_list_t);
    if (NULL == event_list) {
        dest_genex_workflow_value(workflow_value, genex_analyze_caddy);
        return ORCM_ERROR;
    }

    rc = monitor_genex(workflow_value, cbdata, event_list);
    if (ORCM_SUCCESS != rc) {
        dest_genex_workflow_value(workflow_value, genex_analyze_caddy);
        return ORCM_ERROR;
    }

    analytics_value_to_next = orcm_util_load_orcm_analytics_value(
        genex_analyze_caddy->analytics_value->key,
        genex_analyze_caddy->analytics_value->non_compute_data,
        genex_analyze_caddy->analytics_value->compute_data);

    if ( NULL == analytics_value_to_next ) {
            dest_genex_workflow_value(workflow_value, genex_analyze_caddy);
            return ORCM_ERR_OUT_OF_RESOURCE;
    }

    if( true == orcm_analytics_base_db_check(genex_analyze_caddy->wf_step)) {
        rc = orcm_analytics_base_log_to_database_event(analytics_value_to_next);
        if ( ORCM_SUCCESS != rc ) {
            dest_genex_workflow_value(workflow_value, genex_analyze_caddy);
            return ORCM_ERROR;
        }
    }
    if(0 == event_list->opal_list_length){
        SAFE_RELEASE(event_list);
    }
    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(genex_analyze_caddy->wf,genex_analyze_caddy->wf_step,
                                     genex_analyze_caddy->hash_key, analytics_value_to_next, NULL);

    dest_genex_workflow_value(workflow_value, genex_analyze_caddy);

    return ORCM_SUCCESS;
}
