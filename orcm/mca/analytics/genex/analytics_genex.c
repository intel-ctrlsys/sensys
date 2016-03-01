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

static int init(orcm_analytics_base_module_t *imod);
static void finalize(orcm_analytics_base_module_t *imod);
static int analyze(int sd, short args, void *cbdata);
static int monitor_genex(genex_workflow_value_t *workflow_value, void* cbdata);
static int generate_notification_event(orcm_analytics_value_t* analytics_value,
                                       int sev, char *msg,
                                       char *action);
static orcm_analytics_value_t* get_analytics_value(orcm_value_t* current_value,
                                                   opal_list_t* key,
                                                   opal_list_t* non_compute);

mca_analytics_genex_module_t orcm_analytics_genex_module = {{
    init,
    finalize,
    analyze,
    NULL,
    NULL
}};

static void dest_genex_workflow_value(genex_workflow_value_t *workflow_value,
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
    if (NULL != genex_analyze_caddy) {
        OBJ_RELEASE(genex_analyze_caddy);
    }
}

static int init(orcm_analytics_base_module_t *imod)
{
    return ORCM_SUCCESS;
}

static void finalize(orcm_analytics_base_module_t *imod)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
        "%s analytics:genex:finalize", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
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
static int get_genex_policy(void *cbdata, genex_workflow_value_t *workflow_value)
{
    orcm_workflow_caddy_t *parse_workflow_caddy = NULL;
    opal_value_t *temp = NULL;

    workflow_value->msg_regex_label = strdup("msg_regex");
    workflow_value->severity_label = strdup("severity");
    workflow_value->notifier_label = strdup("notifier");
    workflow_value->db_label = strdup("db");
    workflow_value->msg_regex = NULL;
    workflow_value->severity = NULL;
    workflow_value->notifier = NULL;
    workflow_value->db = NULL;

    if ( NULL == cbdata) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:NULL caddy data passed by previous workflow step",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ORCM_ERROR;
    }

    parse_workflow_caddy = (orcm_workflow_caddy_t *) cbdata;
    if (ORCM_SUCCESS != orcm_analytics_base_assert_caddy_data(cbdata)) {
        return ORCM_ERROR;
    }

    OPAL_LIST_FOREACH(temp, &parse_workflow_caddy->wf_step->attributes, opal_value_t) {
        if (NULL == temp || NULL == temp->key || NULL == temp->data.string) {
            return ORCM_ERROR;
        }
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

static orcm_analytics_value_t* get_analytics_value(orcm_value_t* current_value,
                                                   opal_list_t* key,
                                                   opal_list_t* non_compute)
{
    orcm_value_t* analytics_orcm_value = NULL;
    orcm_analytics_value_t* genex_value = NULL;
    opal_list_t* compute_data = OBJ_NEW(opal_list_t);

    if(NULL == compute_data) {
        return NULL;
    }

    analytics_orcm_value = orcm_util_copy_orcm_value(current_value);
    if(NULL != analytics_orcm_value) {
        opal_list_append(compute_data, (opal_list_item_t *)analytics_orcm_value);
    }

    genex_value = orcm_util_load_orcm_analytics_value_compute(key, non_compute,
                                                              compute_data);
    return genex_value;
}

static int generate_notification_event(orcm_analytics_value_t* analytics_value,
                                       int sev, char *msg,
                                       char *action)
{
    int rc = ORCM_SUCCESS;
    char *event_action = NULL;
    orcm_ras_event_t *event_data = NULL;

    if (NULL != msg) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                             "%s analytics:genex: %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), msg));
    }

    IF_NULL(action);
    event_action = strdup(action);
    event_data = orcm_analytics_base_event_create(analytics_value,
                                                  ORCM_RAS_EVENT_EXCEPTION, sev);
    if (NULL == event_data) {
       goto done;
    }

    if (0 != strcmp(event_action, "none")) {
        rc = orcm_analytics_base_event_set_storage(event_data,
                                                   ORCM_STORAGE_TYPE_NOTIFICATION);
        if (ORCM_SUCCESS != rc) {
            goto done;
        }
        rc = orcm_analytics_base_event_set_description(event_data, "notifier_msg",
                                                       msg, OPAL_STRING, NULL);
        if (ORCM_SUCCESS != rc) {
            goto done;
        }
        rc = orcm_analytics_base_event_set_description(event_data, "notifier_action",
                                                       event_action, OPAL_STRING,
                                                       NULL);
        if (ORCM_SUCCESS != rc) {
            goto done;
        }
        ORCM_RAS_EVENT(event_data);
    }
done:
    SAFEFREE(event_action);
    return rc;
}

static int monitor_genex(genex_workflow_value_t *workflow_value, void* cbdata)
{
    int rc = ORCM_SUCCESS;
    orcm_workflow_caddy_t *caddy = NULL;
    orcm_value_t *analytics_value = NULL;
    opal_list_t *sample_data_list = NULL;
    genex_value_t genex_value;
    regex_t regex_comp_wflow;
    char *error_buffer;
    int regex_res;
    orcm_analytics_value_t* genex_analytics_value = NULL;

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

    if (NULL == cbdata) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:genex:NULL caddy data passed by the previous workflow step",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ORCM_ERROR;
    }

    caddy = (orcm_workflow_caddy_t *) cbdata;
    sample_data_list = caddy->analytics_value->compute_data;
    IF_NULL(sample_data_list);

    OPAL_LIST_FOREACH(analytics_value, sample_data_list, orcm_value_t) {
        if (NULL == analytics_value) {
            return ORCM_ERROR;
        }

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

                genex_analytics_value =
                    get_analytics_value(analytics_value,
                                        caddy->analytics_value->key,
                                        caddy->analytics_value->non_compute_data);

                if(NULL == genex_analytics_value) {
                    return ORCM_ERR_OUT_OF_RESOURCE;
                }

                rc = generate_notification_event(genex_analytics_value,
                                                 orcm_analytics_event_get_severity(workflow_value->severity),
                                                 genex_value.message,
                                                 workflow_value->notifier);
                if ( ORCM_SUCCESS != rc ) {
                    SAFEFREE(genex_analytics_value);
                    return ORCM_ERROR;
                }
             } else if(regex_res && regex_res != REG_NOMATCH) {
                error_buffer = malloc(100);
                if (NULL == error_buffer) {
                    return ORCM_ERR_OUT_OF_RESOURCE;
                }

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

    if (ORCM_SUCCESS != (rc = orcm_analytics_base_assert_caddy_data(cbdata))) {
        return ORCM_ERROR;
    }

    workflow_value = (genex_workflow_value_t *)malloc(sizeof(genex_workflow_value_t));
    if ( NULL == workflow_value) {
        OPAL_OUTPUT_VERBOSE((1, orcm_analytics_base_framework.framework_output,
                            "%s Insufficient data", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    genex_analyze_caddy = (orcm_workflow_caddy_t *) cbdata;

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                        "%s analytics:%s:analyze ", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        genex_analyze_caddy->wf_step->analytic));

    rc = get_genex_policy(cbdata, workflow_value);
    if (ORCM_SUCCESS != rc) {
        dest_genex_workflow_value(workflow_value, genex_analyze_caddy);
        return ORCM_ERROR;
    }

    rc = monitor_genex(workflow_value, cbdata);
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

    if( true == orcm_analytics_base_db_check(genex_analyze_caddy->wf_step, true)) {
        rc = orcm_analytics_base_log_to_database_event(analytics_value_to_next);
        if ( ORCM_SUCCESS != rc ) {
            dest_genex_workflow_value(workflow_value, genex_analyze_caddy);
            return ORCM_ERROR;
        }
    }

    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(genex_analyze_caddy->wf,genex_analyze_caddy->wf_step,
                                     genex_analyze_caddy->hash_key, analytics_value_to_next, NULL);

    dest_genex_workflow_value(workflow_value, genex_analyze_caddy);

    return ORCM_SUCCESS;
}
