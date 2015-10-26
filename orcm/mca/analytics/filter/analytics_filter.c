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
#include "orcm/util/utils.h"

static int init(orcm_analytics_base_module_t *imod);
static void finalize(orcm_analytics_base_module_t *imod);
static int analyze(int sd, short args, void *cbdata);

mca_analytics_filter_module_t orcm_analytics_filter_module = { { init, finalize,
        analyze } };

static int analytics_filter_data(opal_list_t *filter_list,
                                 filter_workflow_value_t *workflow_value,
                                 void* cbdata);


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
static int analytics_filter_data(opal_list_t *filter_list,
                                 filter_workflow_value_t *workflow_value,
                                 void* cbdata)
{
    int index=0;
    orcm_workflow_caddy_t *caddy = NULL;
    orcm_value_t *analytics_value = NULL;
    opal_value_t *analytics_opal_value = NULL;
    orcm_value_t *analytics_orcm_value = NULL;
    opal_list_t *sample_data_list = NULL;
    unsigned int node_index = 0;
    unsigned int sensor_index = 0;
    unsigned int core_index = 0;
    unsigned int nodeid_array_length = 0;
    unsigned int sensorname_array_length = 0;
    unsigned int coreid_array_length = 0;
    const int NUM_PARAMS = 3;
    const char *params[] = {
        "data_group",
        "hostname",
        "ctime"
    };
    opal_value_t *param_items[] = {NULL, NULL, NULL};
    opal_bitmap_t item_bm;
    int num_items;

    if (NULL == cbdata) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:NULL caddy data passed by the previous workflow step",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ORCM_ERROR;
    }

    caddy = (orcm_workflow_caddy_t *) cbdata;
    sample_data_list = caddy->data;
    if (NULL == sample_data_list) {
        return ORCM_ERROR;
    }

    /* Get the main parameters form the list */
    num_items = opal_list_get_size(sample_data_list);
    OBJ_CONSTRUCT(&item_bm, opal_bitmap_t);
    opal_bitmap_init(&item_bm, (int)num_items);
    orcm_util_find_items(params, NUM_PARAMS, sample_data_list, param_items, &item_bm);

    /*Find the array length of user inputs*/
    nodeid_array_length = opal_argv_count(workflow_value->nodeid);
    sensorname_array_length = opal_argv_count(workflow_value->sensorname);
    coreid_array_length = opal_argv_count(workflow_value->coreid);

    OPAL_LIST_FOREACH(analytics_value, sample_data_list, orcm_value_t) {
        /* Ignore the items that have already been processed */
        if (opal_bitmap_is_set_bit(&item_bm, index)) {
            index++;
            analytics_opal_value = orcm_util_copy_opal_value((opal_value_t *)analytics_value);
            if (NULL != analytics_opal_value) {
                opal_list_append(filter_list, &analytics_opal_value->super);
            }
            continue;
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
                                                          param_items[1]->data.string,
                                                          strlen(workflow_value->nodeid[node_index])))
                                         && (0 == strncmp(workflow_value->sensorname[sensor_index],
                                                          param_items[0]->data.string,
                                                          strlen(workflow_value->sensorname[sensor_index])))
                                         && (0 == strncmp(workflow_value->coreid[core_index],
                                                          analytics_value->value.key,
                                                          strlen(workflow_value->coreid[core_index])))) {
                                            analytics_orcm_value = orcm_util_copy_orcm_value(analytics_value);
                                            if (NULL != analytics_orcm_value) {
                                                opal_list_append(filter_list, (opal_list_item_t *)analytics_orcm_value);
                                            }

                                        }
                                    }
                                } else {
                                    /*If NodeID & SensorNameavailable save
                                     * the data to send it to next plug-in*/
                                    if ((0 == strncmp(workflow_value->nodeid[node_index],
                                                      param_items[1]->data.string,
                                                      strlen(workflow_value->nodeid[node_index])))
                                     && (0 == strncmp(workflow_value->sensorname[sensor_index],
                                                      param_items[0]->data.string,
                                                      strlen(workflow_value->sensorname[sensor_index])))) {
                                        analytics_orcm_value = orcm_util_copy_orcm_value(analytics_value);
                                        if (NULL != analytics_orcm_value) {
                                            opal_list_append(filter_list, (opal_list_item_t *)analytics_orcm_value);
                                        }
                                    }
                                }
                            }
                        } else if (0 < coreid_array_length) {
                            /*Check for CoreID present or not*/
                            for (core_index = 0; core_index < coreid_array_length; core_index++) {
                                /*If NodeID & CoreID available save the data to send it to next plug-in*/
                                if ((0 == strncmp(workflow_value->nodeid[node_index],
                                                  param_items[1]->data.string,
                                                  strlen(workflow_value->nodeid[node_index])))
                                 && (0 == strncmp(workflow_value->coreid[core_index],
                                                  analytics_value->value.key,
                                                  strlen(workflow_value->coreid[core_index])))) {
                                    analytics_orcm_value = orcm_util_copy_orcm_value(analytics_value);
                                    if (NULL != analytics_orcm_value) {
                                        opal_list_append(filter_list, (opal_list_item_t *)analytics_orcm_value);
                                    }
                                }
                            }
                        } else {
                            //*If only NodeID available then save the data to send it to next plug-in*/
                            if (0 == strncmp(workflow_value->nodeid[node_index],
                                             param_items[1]->data.string,
                                             strlen(workflow_value->nodeid[node_index]))) {
                                analytics_orcm_value = orcm_util_copy_orcm_value(analytics_value);
                                if (NULL != analytics_orcm_value) {
                                    opal_list_append(filter_list, (opal_list_item_t *)analytics_orcm_value);
                                }
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
                                          param_items[0]->data.string,
                                          strlen(workflow_value->sensorname[sensor_index])))
                         && (0 == strncmp(workflow_value->coreid[core_index],
                                          analytics_value->value.key,
                                          strlen(workflow_value->coreid[core_index])))) {
                            analytics_orcm_value = orcm_util_copy_orcm_value(analytics_value);
                            if (NULL != analytics_orcm_value) {
                                opal_list_append(filter_list, (opal_list_item_t *)analytics_orcm_value);
                            }
                        }
                    }
                } else {
                    /*If SensorName available then save the data to send it to next plug-in*/
                    if (0 == strncmp(workflow_value->sensorname[sensor_index],
                                     param_items[0]->data.string,
                                     strlen(workflow_value->sensorname[sensor_index]))) {
                        analytics_orcm_value = orcm_util_copy_orcm_value(analytics_value);
                        if (NULL != analytics_orcm_value) {
                            opal_list_append(filter_list, (opal_list_item_t *)analytics_orcm_value);
                        }
                    }
                }
            }

        } else if (0 < coreid_array_length) {
            /*If only CoreID present*/
            for (core_index = 0; core_index < coreid_array_length; core_index++) {
                /*If CoreID available save the data to send it to next plug-in*/
                if (0 == strncmp(workflow_value->coreid[core_index],
                                 analytics_value->value.key,
                                 strlen(workflow_value->coreid[core_index]))) {
                    analytics_orcm_value = orcm_util_copy_orcm_value(analytics_value);
                    if (NULL != analytics_orcm_value) {
                        opal_list_append(filter_list, (opal_list_item_t *)analytics_orcm_value);
                    }
                }
            }
        } else {
            analytics_orcm_value = orcm_util_copy_orcm_value(analytics_value);
            if (NULL != analytics_orcm_value) {
                opal_list_append(filter_list, (opal_list_item_t *)analytics_orcm_value);
            }
        }
        index++;
    }
    OBJ_DESTRUCT(&item_bm);
    return ORCM_SUCCESS;
}

static int analyze(int sd, short args, void *cbdata)
{
    orcm_workflow_caddy_t *filter_analyze_caddy = NULL;
    opal_list_t *filter_list;
    int rc = -1;

    filter_workflow_value_t *workflow_value = (filter_workflow_value_t *)malloc(
                                              sizeof(filter_workflow_value_t));
    if ( NULL == workflow_value) {
        OPAL_OUTPUT_VERBOSE((1, orcm_analytics_base_framework.framework_output,
                            "%s Insufficient data", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        free(workflow_value);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    if ( NULL == cbdata) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
            "%s analytics:average:NULL caddy data passed by the previous workflow step",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        free(workflow_value);
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

    filter_list= OBJ_NEW(opal_list_t);
    if (NULL == filter_list) {
        dest_filter_workflow_value(workflow_value);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    rc = analytics_filter_data(filter_list, workflow_value, cbdata);
    if (ORCM_SUCCESS != rc) {
        dest_filter_workflow_value(workflow_value);
        OPAL_LIST_RELEASE(filter_list);
        return ORCM_ERROR;
    }


    /* load data to database if needed */
    if (true == orcm_analytics_base_db_check(filter_analyze_caddy->wf_step)) {
        OBJ_RETAIN(filter_list);
        rc = orcm_analytics_base_store(filter_list);
        if (ORCM_SUCCESS != rc) {
             OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                                  "%s analytics:filter:Data can't be written to DB",
                                  ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
             dest_filter_workflow_value(workflow_value);
             OPAL_LIST_RELEASE(filter_list);
         }
    }

    ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(filter_analyze_caddy->wf,
                                     filter_analyze_caddy->wf_step, 0, filter_list);

    dest_filter_workflow_value(workflow_value);

    return ORCM_SUCCESS;
}
