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

#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/mca/analytics/filter/analytics_filter.h"

#include "orcm/runtime/orcm_globals.h"

static int init(orcm_analytics_base_module_t *imod);
static void finalize(orcm_analytics_base_module_t *imod);
static int analyze(int sd, short args, void *cbdata);

mca_analytics_filter_module_t orcm_analytics_filter_module = { { init, finalize,
        analyze } };

static int analytics_filter_data(opal_value_array_t *filter_sample_array,
                                 filter_workflow_value_t *workflow_value,
                                 void* cbdata);

static int filter_analytics_array_create(opal_value_array_t **analytics_sample_array)
{
    int rc = -1;

    /*Init the analytics array and set its size */
    *analytics_sample_array = OBJ_NEW(opal_value_array_t);

    if (NULL == *analytics_sample_array) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    rc = opal_value_array_init(*analytics_sample_array, sizeof(orcm_analytics_value_t));
    if (OPAL_SUCCESS != rc) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    return ORCM_SUCCESS;
}

static void dest_filter_workflow_value(filter_workflow_value_t *workflow_value)
{
    free(workflow_value->nodeid_label);
    free(workflow_value->sensor_label);
    free(workflow_value->coreid_label);
    opal_argv_free(workflow_value->nodeid);
    opal_argv_free(workflow_value->sensorname);
    opal_argv_free(workflow_value->coreid);
    free(workflow_value);
}

static int init(orcm_analytics_base_module_t *imod)
{
    return ORCM_SUCCESS;
}

static void finalize(orcm_analytics_base_module_t *imod)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
        "%s analytics:filter:finalize", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
}

/*Function to parse the data received from OFLOW*/
static int filter_parse_workflow(void *cbdata, filter_workflow_value_t *workflow_value)
{
    orcm_workflow_caddy_t *parse_workflow_caddy = NULL;
    opal_value_t *temp = NULL;
    workflow_value->nodeid_label = strdup("nodeid");
    workflow_value->sensor_label = strdup("sensorname");
    workflow_value->coreid_label = strdup("coreid");
    workflow_value->nodeid = NULL;
    workflow_value->sensorname = NULL;
    workflow_value->coreid = NULL;

    if ( NULL == cbdata) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:NULL caddy data passed by the previous workflow step",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ORCM_ERROR;
    }

    parse_workflow_caddy = (orcm_workflow_caddy_t *) cbdata;

    OPAL_LIST_FOREACH(temp, &parse_workflow_caddy->wf_step->attributes, opal_value_t) {
        if (NULL == temp) {
            return ORCM_ERROR;
        }
        if (0 == strncmp(temp->key, workflow_value->nodeid_label,strlen(temp->key))) {
            workflow_value->nodeid = opal_argv_split(temp->data.string, ';');
        } else if (0 == strncmp(temp->key, workflow_value->sensor_label,strlen(temp->key))) {
            workflow_value->sensorname = opal_argv_split(temp->data.string,';');
        } else if (0 == strncmp(temp->key, workflow_value->coreid_label,strlen(temp->key))) {
            workflow_value->coreid = opal_argv_split(temp->data.string, ';');
        }
    }

    return ORCM_SUCCESS;
}

/*Function to filter sample data received based on user request*/
static int analytics_filter_data(opal_value_array_t *filter_sample_array,
                                 filter_workflow_value_t *workflow_value,
                                 void* cbdata)
{
    int rc = -1;
    orcm_workflow_caddy_t *caddy = NULL;
    orcm_analytics_value_t *analytics_value = NULL;
    opal_value_array_t *data_sample_array = NULL;
    unsigned int item_index = 0;
    unsigned int node_index = 0;
    unsigned int sensor_index = 0;
    unsigned int core_index = 0;
    unsigned int nodeid_array_length = 0;
    unsigned int sensorname_array_length = 0;
    unsigned int coreid_array_length = 0;
    unsigned int index = 0;

    if ( NULL == cbdata) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:NULL caddy data passed by the previous workflow step",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ORCM_ERROR;
    }

    caddy = (orcm_workflow_caddy_t *) cbdata;
    data_sample_array = caddy->data;
    if (NULL == data_sample_array) {
        return ORCM_ERROR;
    }

    /*Find the array length of user inputs*/
    nodeid_array_length = opal_argv_count(workflow_value->nodeid);
    sensorname_array_length = opal_argv_count(workflow_value->sensorname);
    coreid_array_length = opal_argv_count(workflow_value->coreid);

    for (index = 0; index < data_sample_array->array_size; index++) {
        analytics_value = opal_value_array_get_item(data_sample_array, index);
        if (NULL == analytics_value) {
            return ORCM_ERROR;
        }

        if (0 < nodeid_array_length) {
            /*Check for NodeId present or not*/
            for (node_index = 0; node_index < nodeid_array_length; node_index++) {
                /*Check for SensorName present or not*/
                if (0 < sensorname_array_length) {
                    for (sensor_index = 0; sensor_index < sensorname_array_length; sensor_index++) {
                        /*Check for CoreID present or not*/
                        if (0 < coreid_array_length) {
                            for (core_index = 0;core_index < coreid_array_length; core_index++) {
                                /*If NodeID, SensorName & CoreID available save
                                 * the data to send it to next plug-in*/
                                if ((0 == strncmp(workflow_value->nodeid[node_index],
                                                  analytics_value->node_regex,
                                                  strlen(workflow_value->nodeid[node_index])))
                                 && (0 == strncmp(workflow_value->sensorname[sensor_index],
                                                  analytics_value->sensor_name,
                                                  strlen(workflow_value->sensorname[sensor_index])))
                                 && (0 == strncmp(workflow_value->coreid[core_index],
                                                  analytics_value->data.value.key,
                                                  strlen(workflow_value->coreid[core_index])))) {
                                    rc = opal_value_array_set_item(filter_sample_array,
                                                                   item_index,
                                                                   analytics_value);
                                    if (OPAL_SUCCESS != rc) {
                                        return ORCM_ERR_BAD_PARAM;
                                    }
                                    item_index++;
                                }
                            }
                        } else {
                            /*If NodeID & SensorNameavailable save
                             * the data to send it to next plug-in*/
                            if ((0 == strncmp(workflow_value->nodeid[node_index],
                                              analytics_value->node_regex,
                                              strlen(workflow_value->nodeid[node_index])))
                             && (0 == strncmp(workflow_value->sensorname[sensor_index],
                                              analytics_value->sensor_name,
                                              strlen(workflow_value->sensorname[sensor_index])))) {
                                rc = opal_value_array_set_item(filter_sample_array,
                                                               item_index,
                                                               analytics_value);
                                if (OPAL_SUCCESS != rc) {
                                    return ORCM_ERR_BAD_PARAM;
                                }
                                item_index++;
                            }
                        }
                    }
                } else if (0 < coreid_array_length) {
                    /*Check for CoreID present or not*/
                    for (core_index = 0; core_index < coreid_array_length; core_index++) {
                        /*If NodeID & CoreID available save the data to send it to next plug-in*/
                        if ((0 == strncmp(workflow_value->nodeid[node_index],
                                          analytics_value->node_regex,
                                          strlen(workflow_value->nodeid[node_index])))
                         && (0 == strncmp(workflow_value->coreid[core_index],
                                          analytics_value->data.value.key,
                                          strlen(workflow_value->coreid[core_index])))) {
                            rc = opal_value_array_set_item(filter_sample_array,
                                                           item_index,
                                                           analytics_value);
                            if (OPAL_SUCCESS != rc) {
                                return ORCM_ERR_BAD_PARAM;
                            }
                            item_index++;
                        }
                    }
                } else {
                    //*If only NodeID available then save the data to send it to next plug-in*/
                    if (0 == strncmp(workflow_value->nodeid[node_index],
                                     analytics_value->node_regex,
                                     strlen(workflow_value->nodeid[node_index]))) {
                        rc = opal_value_array_set_item(filter_sample_array,
                                                       item_index,
                                                       analytics_value);
                        if (OPAL_SUCCESS != rc) {
                            return ORCM_ERR_BAD_PARAM;
                        }
                        item_index++;
                    }
                }
            }
        } else if (0 < sensorname_array_length) {
            /*Check for SensorName present or not*/
            for (sensor_index = 0; sensor_index < sensorname_array_length; sensor_index++) {
                /*Check for CoreID present or not*/
                if (0 < coreid_array_length) {
                    for (core_index = 0; core_index < coreid_array_length; core_index++) {
                        /*If SensorName & CoreID available save
                         * the data to send it to next plug-in*/
                        if ((0 == strncmp(workflow_value->sensorname[sensor_index],
                                          analytics_value->sensor_name,
                                          strlen(workflow_value->sensorname[sensor_index])))
                         && (0 == strncmp(workflow_value->coreid[core_index],
                                          analytics_value->data.value.key,
                                          strlen(workflow_value->coreid[core_index])))) {
                            rc = opal_value_array_set_item(filter_sample_array,
                                                           item_index,
                                                           analytics_value);
                            if (OPAL_SUCCESS != rc) {
                                return ORCM_ERR_BAD_PARAM;
                            }
                            item_index++;
                        }
                    }
                } else {
                    /*If SensorName available then save the data to send it to next plug-in*/
                    if (0 == strncmp(workflow_value->sensorname[sensor_index],
                                     analytics_value->sensor_name,
                                     strlen(workflow_value->sensorname[sensor_index]))) {
                        rc = opal_value_array_set_item(filter_sample_array,
                                                       item_index,
                                                       analytics_value);
                        if (OPAL_SUCCESS != rc) {
                            return ORCM_ERR_BAD_PARAM;
                        }
                        item_index++;
                    }
                }
            }

        } else if (0 < coreid_array_length) {
            /*If only CoreID present*/
            for (core_index = 0; core_index < coreid_array_length; core_index++) {
                /*If CoreID available save the data to send it to next plug-in*/
                if (0 == strncmp(workflow_value->coreid[core_index],
                                 analytics_value->data.value.key,
                                 strlen(workflow_value->coreid[core_index]))) {
                    rc = opal_value_array_set_item(filter_sample_array,
                                                   item_index,
                                                   analytics_value);
                    if (OPAL_SUCCESS != rc) {
                        return ORCM_ERR_BAD_PARAM;
                    }
                    item_index++;
                }
            }
        } else {
            rc = opal_value_array_set_item(filter_sample_array, item_index, analytics_value);
            if (OPAL_SUCCESS != rc) {
                return ORCM_ERR_BAD_PARAM;
            }
            item_index++;
        }
    }

    return ORCM_SUCCESS;
}

static int analyze(int sd, short args, void *cbdata)
{
    orcm_workflow_caddy_t *filter_analyze_caddy = NULL;
    opal_value_array_t *filter_sample_array = NULL;
    orcm_analytics_value_t *verbose_value;
    unsigned int verbose_count;
    int analytics_rc = -1, rc = -1;

    filter_workflow_value_t *workflow_value = (filter_workflow_value_t *)malloc(
                                              sizeof(filter_workflow_value_t));
    if ( NULL == workflow_value) {
        OPAL_OUTPUT_VERBOSE((1, orcm_analytics_base_framework.framework_output,
                            "%s Insufficient data", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        dest_filter_workflow_value(workflow_value);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    if ( NULL == cbdata) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:NULL caddy data passed by the previous workflow step",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        dest_filter_workflow_value(workflow_value);
        return ORCM_ERROR;
    }

    filter_analyze_caddy = (orcm_workflow_caddy_t *) cbdata;

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                        "%s analytics:%s:analyze ", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        filter_analyze_caddy->wf_step->analytic));

    rc = filter_parse_workflow(cbdata, workflow_value);
    if (ORCM_SUCCESS != rc) {
        dest_filter_workflow_value(workflow_value);
        return ORCM_ERROR;
    }

    analytics_rc = filter_analytics_array_create(&filter_sample_array);
    if (ORCM_SUCCESS != analytics_rc) {
        dest_filter_workflow_value(workflow_value);
        return ORCM_ERROR;
    }

    rc = analytics_filter_data(filter_sample_array, workflow_value, cbdata);
    if (ORCM_SUCCESS != rc) {
        dest_filter_workflow_value(workflow_value);
        OBJ_RELEASE(filter_sample_array);
        return ORCM_ERROR;
    }

    for(verbose_count = 0; verbose_count < filter_sample_array->array_size; verbose_count++) {
        verbose_value = opal_value_array_get_item(filter_sample_array, verbose_count);

        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output, "%s"
            "FILTERED DATA Based on Sensor Name: %s | Hostname %s | Data %f",
              ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), verbose_value->sensor_name, verbose_value->node_regex,
              verbose_value->data.value.data.fval));
    }

    /* load data to database if needed */
    orcm_analytics_base_store(filter_analyze_caddy->wf,
                              filter_analyze_caddy->wf_step,
                              filter_sample_array);

    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(filter_analyze_caddy->wf,
                                     filter_analyze_caddy->wf_step, filter_sample_array);

    dest_filter_workflow_value(workflow_value);
    OBJ_RELEASE(filter_analyze_caddy);

    return ORCM_SUCCESS;
}
