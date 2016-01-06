/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved.
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

#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/mca/analytics/syslog/analytics_syslog.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"

#include "orte/mca/notifier/notifier.h"
#include "orte/mca/notifier/base/base.h"

static int init(orcm_analytics_base_module_t *imod);
static void finalize(orcm_analytics_base_module_t *imod);
static int analyze(int sd, short args, void *cbdata);

mca_analytics_syslog_module_t orcm_analytics_syslog_module = {{
    init,
    finalize,
    analyze
}};

static int analytics_syslog_data(syslog_workflow_value_t *workflow_value,
                                 void* cbdata);

static void dest_syslog_workflow_value(syslog_workflow_value_t *workflow_value,
                                       orcm_workflow_caddy_t *syslog_analyze_caddy)
{
    SAFEFREE(workflow_value->msg_regex_label);
    SAFEFREE(workflow_value->severity_label);
    SAFEFREE(workflow_value->facility_label);
    SAFEFREE(workflow_value->msg_regex);
    SAFEFREE(workflow_value->severity);
    SAFEFREE(workflow_value->facility);
    SAFEFREE(workflow_value);
    if (NULL != syslog_analyze_caddy) {
        OBJ_RELEASE(syslog_analyze_caddy);
    }
}

static int init(orcm_analytics_base_module_t *imod)
{
    return ORCM_SUCCESS;
}

static void finalize(orcm_analytics_base_module_t *imod)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
        "%s analytics:syslog:finalize", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
}

/**
 * @brief Function that parses the information (arguments) on the
 *        workflow file and loads it into the syslog-workflow structure.
 *        Usually this arguments are user defined.
 *
 * @param cbdata Pointer to a structure that contains the information
 *        of the arguments to be loaded to the syslog-workflow structure.
 *
 * @param workflow_value Pointer to the syslog-workflow structure in which
 *        the arguments will be loaded
 */
static int syslog_parse_workflow(void *cbdata, syslog_workflow_value_t *workflow_value)
{
    orcm_workflow_caddy_t *parse_workflow_caddy = NULL;
    opal_value_t *temp = NULL;

    workflow_value->msg_regex_label = strdup("msg_regex");
    workflow_value->severity_label = strdup("severity");
    workflow_value->facility_label = strdup("facility");
    workflow_value->msg_regex = NULL;
    workflow_value->severity = NULL;
    workflow_value->facility = NULL;

    if ( NULL == cbdata) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:NULL caddy data passed by the previous workflow step",
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
        } else if (0 == strncmp(temp->key, workflow_value->facility_label,strlen(temp->key))) {
            if (NULL == workflow_value->facility) {
                workflow_value->facility = strdup(temp->data.string);
            }
        }
    }

    return ORCM_SUCCESS;
}

/**
 * @brief Function that catches all the syslog entries sent by the
 *        sensor-syslog component. This function will split each syslog
 *        entry on its different components (Severity, Facility, Date,
 *        Message) into a syslog_value_t structure, then, it will compare
 *        those syslog values against the values provided by the user and
 *        loaded into the syslog_workflow_value_t structure, if they match,
 *        then that entry will be sent to the notifier framework.
 *
 * @param workflow_value Pointer to the structure that contains the
 *        data/arguments collected from the workflow file.
 *
 * @param cbdata Pointer to the data sent by the sensor-syslog component.
 *        It will contain the syslog messages to analyze.
 */
static int analytics_syslog_data(syslog_workflow_value_t *workflow_value,
                                 void* cbdata)
{
    orcm_workflow_caddy_t *caddy = NULL;
    orcm_value_t *analytics_value = NULL;
    opal_list_t *sample_data_list = NULL;
    syslog_value_t syslog_value;

    regex_t regex_comp_log;
    regex_t regex_comp_wflow;
    size_t log_parts = 5;
    regmatch_t log_matches[log_parts];
    char *str_aux;
    int regex_res;

    regcomp(&regex_comp_log,
        "<([0-9]+)>[[:blank:]]*([A-Za-z]{3}[[:blank:]]+[0-9]+[[:blank:]]+([0-9]{2}:?){3})[[:blank:]]+(.*)",
        REG_EXTENDED);

    if (NULL == workflow_value->msg_regex) {
        return ORCM_ERROR;
    }
    regex_res = regcomp(&regex_comp_wflow, workflow_value->msg_regex, REG_EXTENDED);
    if (regex_res) {
      OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                           "%s analytics:syslog:Invalid regular expression at workflow - msg_regex",
                           ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
      return ORCM_ERROR;
    }

    if (NULL == cbdata) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:NULL caddy data passed by the previous workflow step",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ORCM_ERROR;
    }

    caddy = (orcm_workflow_caddy_t *) cbdata;
    sample_data_list = caddy->analytics_value->compute_data;
    if (NULL == sample_data_list) {
        return ORCM_ERROR;
    }

    OPAL_LIST_FOREACH(analytics_value, sample_data_list, orcm_value_t) {
        if (NULL == analytics_value) {
           return ORCM_ERROR;
        }

        regex_res = regexec(&regex_comp_log, analytics_value->value.data.string, log_parts, log_matches,0);
        if (!regex_res) {
            /*If the syslog message matches with the syslog parts. Get severity and facility*/
            str_aux = strndup(&analytics_value->value.data.string[log_matches[1].rm_so],
                              (int)(log_matches[1].rm_eo - log_matches[1].rm_so));
            syslog_value.severity=atoi(str_aux);
            syslog_value.facility=syslog_value.severity/8;
            syslog_value.severity=syslog_value.severity%8;
            free(str_aux);

            /*Get the date*/
            syslog_value.date = strndup(&analytics_value->value.data.string[log_matches[2].rm_so],
                                        (int)(log_matches[2].rm_eo - log_matches[2].rm_so));

            /*Get the message*/
            syslog_value.message = strndup(&analytics_value->value.data.string[log_matches[4].rm_so],
                                           (int)(log_matches[4].rm_eo - log_matches[4].rm_so));

            regex_res=regexec(&regex_comp_wflow, syslog_value.message, 0, NULL, 0);
            if( ( workflow_value->msg_regex == NULL || !regex_res ) &&
	        ( workflow_value->severity == NULL || syslog_value.severity == atoi(workflow_value->severity) ) &&
	        ( workflow_value->facility == NULL || syslog_value.facility == atoi(workflow_value->facility) )) {
                    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                                         "%s analytics:syslog: MATCHES USER PARAMS: \"%s\" ",
                                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), syslog_value.message));
                ORTE_NOTIFIER_SYSTEM_EVENT(ORTE_NOTIFIER_WARN, analytics_value->value.data.string, "smtp");
            } else if(regex_res && regex_res != REG_NOMATCH) {
                str_aux=malloc(100);
                if (NULL == str_aux) {
                    return ORCM_ERR_OUT_OF_RESOURCE;
                }
                regerror(regex_res, &regex_comp_log, str_aux, 100);
                OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                                   "%s analytics:syslog: %s",
                                   ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), str_aux));
                SAFEFREE(str_aux);
                return ORCM_ERROR;
            }
        } else if(regex_res != REG_NOMATCH) {
            str_aux=malloc(100);
            if (NULL == str_aux) {
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            regerror(regex_res, &regex_comp_log, str_aux, 100);
            OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                               "%s analytics:syslog: %s",
                               ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), str_aux));
            SAFEFREE(str_aux);
            return ORCM_ERROR;
        }
    }

    return ORCM_SUCCESS;
}

static int analyze(int sd, short args, void *cbdata)
{
    int rc = -1;
    orcm_workflow_caddy_t *syslog_analyze_caddy = NULL;
    orcm_analytics_value_t *analytics_value_to_next = NULL;
    syslog_workflow_value_t *workflow_value = NULL;

    if (ORCM_SUCCESS != (rc = orcm_analytics_base_assert_caddy_data(cbdata))) {
        return ORCM_ERROR;
    }

    workflow_value = (syslog_workflow_value_t *)malloc(sizeof(syslog_workflow_value_t));
    if ( NULL == workflow_value) {
        OPAL_OUTPUT_VERBOSE((1, orcm_analytics_base_framework.framework_output,
                            "%s Insufficient data", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    syslog_analyze_caddy = (orcm_workflow_caddy_t *) cbdata;

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                        "%s analytics:%s:analyze ", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        syslog_analyze_caddy->wf_step->analytic));

    rc = syslog_parse_workflow(cbdata, workflow_value);
    if (ORCM_SUCCESS != rc) {
        dest_syslog_workflow_value(workflow_value,syslog_analyze_caddy);
        return ORCM_ERROR;
    }

    rc = analytics_syslog_data(workflow_value, cbdata);
    if (ORCM_SUCCESS != rc) {
        dest_syslog_workflow_value(workflow_value, syslog_analyze_caddy);
        return ORCM_ERROR;
    }

    if (NULL == (analytics_value_to_next = orcm_util_load_orcm_analytics_value(
        syslog_analyze_caddy->analytics_value->key,
        syslog_analyze_caddy->analytics_value->non_compute_data,
        syslog_analyze_caddy->analytics_value->compute_data))) {
            dest_syslog_workflow_value(workflow_value, syslog_analyze_caddy);
            return ORCM_ERR_OUT_OF_RESOURCE;
    }

    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(syslog_analyze_caddy->wf,
                                     syslog_analyze_caddy->wf_step, 0, analytics_value_to_next);

    dest_syslog_workflow_value(workflow_value, syslog_analyze_caddy);

    return ORCM_SUCCESS;
}
