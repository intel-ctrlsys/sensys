/*
 * Copyright (c) 2014-2016  Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "orte/mca/notifier/notifier.h"

#include "orcm/tools/octl/common.h"
#include "orcm/util/logical_group.h"
#include "orcm/mca/db/base/base.h"
#include "orcm/mca/db/db.h"
#include "orcm/mca/analytics/analytics_types.h"

#define SAFE_OBJ_RELEASE(x) if(NULL!=x) { OBJ_RELEASE(x); x = NULL; }

int orcm_octl_sensor_policy_get(int cmd, char **argv)
{
    orcm_sensor_cmd_flag_t command;
    opal_buffer_t *buf = NULL;
    int rc = ORCM_SUCCESS;
    int cnt, i;
    int num = 0;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;
    char *sensor_name = NULL;
    char *sev = NULL;
    char *action = NULL;
    char *threstype = NULL;
    float threshold;
    bool hi_thres;
    int max_count, time_window;


    orte_notifier_severity_t severity;

    if (4 != opal_argv_count(argv)) {
        orcm_octl_usage("sensor-get-policy", INVALID_USG);
        rc = ORCM_ERR_BAD_PARAM;
        return rc;
    }

    /* setup the receiver nodelist */
    orcm_logical_group_parse_array_string(argv[3], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        opal_argv_free(nodelist);
        nodelist = NULL;
        orcm_octl_error("nodelist-notfound", argv[3]);
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }

    /* pack the buffer to send */
    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_GET_SENSOR_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        orcm_octl_error("pack");
        goto done;
    }

    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        orcm_octl_error("pack");
        goto done;
    }

    /* Loop through the nodelist to set sensor policy */
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    for (i = 0; i < opal_argv_count(nodelist); i++) {
        OBJ_RETAIN(buf);
        OBJ_RETAIN(xfer);

        xfer->active = true;
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SENSOR,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, xfer);

        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            orcm_octl_error("node-notfound", nodelist[i]);
            goto done;
        }

        orcm_octl_info("getting-policy", nodelist[i]);
        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
            orcm_octl_error("connection-fail");
            goto done;
        }

        /* wait for status message */
        ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_error("connection-fail");
            goto done;
        }

        cnt=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &num,
                                                  &cnt, OPAL_INT))) {
            orcm_octl_error("unpack");
            goto done;
        }

        if ( 0 == num ) {
            rc = ORCM_SUCCESS;
            orcm_octl_info("no-active-policy");
            goto done;
        } else {
             orcm_octl_info("policy-header");
            for (i = 0; i < num; i++) {
                /* unpack sensor name */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &sensor_name,
                                                  &cnt, OPAL_STRING))) {
                    orcm_octl_error("unpack");
                    goto done;
                }

                /* unpack threshold */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &threshold,
                                                  &cnt, OPAL_FLOAT))) {
                    orcm_octl_error("unpack");
                    goto done;
                }

                /* unpack threshold type */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &hi_thres,
                                                  &cnt, OPAL_BOOL))) {
                    orcm_octl_error("unpack");
                    goto done;
                }

                /* unpack max count */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &max_count,
                                                  &cnt, OPAL_INT))) {
                    orcm_octl_error("unpack");
                    goto done;
                }

                /* unpack time window */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &time_window,
                                                  &cnt, OPAL_INT))) {
                    orcm_octl_error("unpack");
                    goto done;
                }

                /* unpack severity */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &severity,
                                                  &cnt, OPAL_INT))) {
                    orcm_octl_error("unpack");
                    goto done;
                }

                /* unpack action */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &action,
                                                  &cnt, OPAL_STRING))) {
                    orcm_octl_error("unpack");
                    goto done;
                }

                switch ( severity ) {
                case ORTE_NOTIFIER_EMERG:
                    sev = strdup("EMERG");
                    break;
                case ORTE_NOTIFIER_ALERT:
                    sev = strdup("ALERT");
                    break;
                case ORTE_NOTIFIER_CRIT:
                    sev = strdup("CRIT");
                    break;
                case ORTE_NOTIFIER_ERROR:
                    sev = strdup("ERROR");
                    break;
                case ORTE_NOTIFIER_WARN:
                    sev = strdup("WARN");
                    break;
                case ORTE_NOTIFIER_NOTICE:
                    sev = strdup("NOTICE");
                    break;
                case ORTE_NOTIFIER_INFO:
                    sev = strdup("INFO");
                    break;
                case ORTE_NOTIFIER_DEBUG:
                    sev = strdup("DEBUG");
                    break;
                default:
                    sev = strdup("UNKNOWN");
                    break;
                }

                if ( hi_thres ) {
                    threstype = strdup("Hi");
                } else {
                    threstype = strdup("Lo");
                }

                if ( 0 == strcmp(sensor_name, "coretemp") ) {
                    orcm_octl_info("policy", sensor_name, threshold, " °C",
                                   threstype, max_count, time_window, sev, action);
                } else if ( 0 == strcmp(sensor_name, "corefreq") ) {
                    orcm_octl_info("policy", sensor_name, threshold, "GHz",
                                   threstype, max_count, time_window, sev, action);
                } else {
                   orcm_octl_info("policy", sensor_name, threshold, "   ",
                                 threstype, max_count, time_window, sev, action);
                }
                SAFEFREE(threstype);
                SAFEFREE(sev);
            }
        }
    }

done:
    if (buf) {
        OBJ_RELEASE(buf);
    }
    if (xfer) {
        OBJ_RELEASE(xfer);
    }
    if (nodelist) {
        opal_argv_free(nodelist);
    }
    SAFEFREE(sev);
    SAFEFREE(threstype);
    SAFEFREE(action);
    SAFEFREE(sensor_name);

    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SENSOR);

    return rc;
}

int orcm_octl_sensor_sample_rate_set(int cmd, char **argv)
{
    orcm_sensor_cmd_flag_t command;
    int sample_rate = 0;
    opal_buffer_t *buf = NULL;
    int cnt, i;
    int rc = ORCM_SUCCESS;
    int result = ORCM_SUCCESS;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;
    char *error = NULL;

    if (6 != opal_argv_count(argv)) {
        orcm_octl_usage("sensor-set-sample-rate", INVALID_USG);
        rc = ORCM_ERR_BAD_PARAM;
        return rc;
    }

    orcm_octl_info("sensor-set-sample-rate");

    if (isdigit(argv[4][strlen(argv[4]) - 1])) {
        sample_rate = (int)strtol(argv[4], NULL, 10);
    } else {
        orcm_octl_error("not-integer", "sample rate");
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }

    orcm_logical_group_parse_array_string(argv[5], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        opal_argv_free(nodelist);
        nodelist = NULL;
        orcm_octl_error("nodelist-notfound", argv[5]);
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }

    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_SET_SENSOR_COMMAND;
    /* pack sensor set command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        orcm_octl_error("pack");
        goto done;
    }
    command = cmd;
    /* pack sensor sample rate sub command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        orcm_octl_error("pack");
        goto done;
    }
    /* pack sensor name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &argv[3],
                                            1, OPAL_STRING))) {
        orcm_octl_error("pack");
        goto done;
    }

    /* pack sample_rate value */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &sample_rate,
                                            1, OPAL_INT))) {
        orcm_octl_error("pack");
        goto done;
    }

    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    /* Loop through the nodelist and set the sample rate */
    for (i = 0; i < opal_argv_count(nodelist); i++) {
        OBJ_RETAIN(buf);
        OBJ_RETAIN(xfer);
        /* setup to receive the rc */
        xfer->active = true;
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                                ORCM_RML_TAG_SENSOR,
                                ORTE_RML_NON_PERSISTENT,
                                orte_rml_recv_callback, xfer);

        orcm_octl_info("setting-sample-rate", argv[3], nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            orcm_octl_error("node-notfound", nodelist[i]);
            goto done;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
            orcm_octl_error("connection-fail");
            goto done;
        }

        /* wait for status message */
        ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_error("connection-fail");
            goto done;
        }

        cnt=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &result,
                                                  &cnt, OPAL_INT))) {
            orcm_octl_error("unpack");
            goto done;
         }
         if (ORCM_SUCCESS == result) {
             orcm_octl_info("success");
         } else {
             if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &error,
                                                  &cnt, OPAL_STRING))) {
                 orcm_octl_error("unpack");
                 goto done;
             }
             rc = result;
             if (ORCM_ERR_SENSOR_LIMIT_EXCEEDED == result) {
                 orcm_octl_error("sensor_limit_exceeded");
             }
             goto done;
         }
    }

done:
    if(buf) {
       OBJ_RELEASE(buf);
    }
    if(xfer) {
       OBJ_RELEASE(xfer);
    }
    if(nodelist) {
       opal_argv_free(nodelist);
    }
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SENSOR);
    return rc;
}

int orcm_octl_sensor_sample_rate_get(int cmd, char **argv)
{
    orcm_sensor_cmd_flag_t command;
    opal_buffer_t *buf = NULL;
    int rc = ORCM_SUCCESS;
    int result = ORCM_SUCCESS;
    int cnt, i;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;
    char *sensor_name = NULL;
    int sample_rate = 0;
    char *error = NULL;

    if (5 != opal_argv_count(argv)) {
        orcm_octl_usage("sensor-get-sample-rate", INVALID_USG);
        rc = ORCM_ERR_BAD_PARAM;
        return rc;
    }

    /* setup the receiver nodelist */
    orcm_logical_group_parse_array_string(argv[4], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        opal_argv_free(nodelist);
        nodelist = NULL;
        orcm_octl_error("nodelist-notfound", argv[4]);
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }

    /* pack the buffer to send */
    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_GET_SENSOR_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        orcm_octl_error("pack");
        goto done;
    }

    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        orcm_octl_error("pack");
        goto done;
    }

    /* pack sensor name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &argv[3],
                                            1, OPAL_STRING))) {
        orcm_octl_error("pack");
        goto done;
    }

    /* Loop through the nodelist to get sensor sample-rate */
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    for (i = 0; i < opal_argv_count(nodelist); i++) {
        OBJ_RETAIN(buf);
        OBJ_RETAIN(xfer);

        xfer->active = true;
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SENSOR,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, xfer);

       orcm_octl_info("getting-sample-rate", argv[3], nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            orcm_octl_error("node-notfound", nodelist[i]);
            goto done;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
            orcm_octl_error("connection-fail");
            goto done;
        }

        /* wait for status message */
        ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_error("connection-fail");
            goto done;
        }

        cnt=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &result,
                                                  &cnt, OPAL_INT))) {
            orcm_octl_error("unpack");
            goto done;
        }

        if ( 0 != result ) {
            cnt=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &error,
                                                       &cnt, OPAL_STRING))) {
                orcm_octl_error("unpack");
                goto done;
            }
            rc = result;
            goto done;
        } else {
            orcm_octl_info("sample-rate-header");
            /* unpack sensor name */
            cnt = 1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &sensor_name,
                                              &cnt, OPAL_STRING))) {
                orcm_octl_error("unpack");
                goto done;
            }

            /* unpack sample rate */
            cnt = 1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data,
                                                      &sample_rate,
                                                      &cnt, OPAL_INT))) {
                orcm_octl_error("unpack");
                goto done;
            }

            orcm_octl_info("sample-rate", sensor_name, sample_rate);
        }
    }

done:
    if(buf) {
       OBJ_RELEASE(buf);
    }
    if(xfer) {
       OBJ_RELEASE(xfer);
    }
    if(nodelist) {
       opal_argv_free(nodelist);
    }
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SENSOR);

    return rc;
}

static int get_inventory_list(int cmd, opal_list_t *filterlist, opal_list_t** results)
{
    int n = 1;
    orcm_rm_cmd_flag_t command = (orcm_rm_cmd_flag_t)cmd;
    orcm_db_filter_t *tmp_filter = NULL;
    int rc = -1;
    opal_buffer_t *buffer = OBJ_NEW(opal_buffer_t);
    uint16_t filterlist_count = 0;
    orte_rml_recv_cb_t *xfer = NULL;
    uint16_t results_count = 0;
    int returned_status = 0;

    if(NULL == filterlist || NULL == results)
    {
        rc = ORCM_ERR_BAD_PARAM;
        goto inv_list_cleanup;
    }
    filterlist_count = (uint16_t)opal_list_get_size(filterlist);

    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &command, 1, ORCM_RM_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto inv_list_cleanup;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &filterlist_count, 1, OPAL_UINT16))) {
        ORTE_ERROR_LOG(rc);
        goto inv_list_cleanup;
    }
    OPAL_LIST_FOREACH(tmp_filter, filterlist, orcm_db_filter_t) {
        uint8_t operation = (uint8_t)tmp_filter->op;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &tmp_filter->value.key, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            goto inv_list_cleanup;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &operation, 1, OPAL_UINT8))) {
            ORTE_ERROR_LOG(rc);
            goto inv_list_cleanup;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &tmp_filter->value.data.string, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            goto inv_list_cleanup;
        }
    }

    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    OBJ_RETAIN(xfer);
    OBJ_RETAIN(buffer);
    xfer->active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ORCMD_FETCH, ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, xfer);

    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buffer,
                                                      ORCM_RML_TAG_ORCMD_FETCH,
                                                      orte_rml_send_callback, xfer))) {
        ORTE_ERROR_LOG(rc);
        goto inv_list_cleanup;
    }

    /* wait for status message */
    ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
    if (ORCM_SUCCESS != rc) {
        ORTE_ERROR_LOG(rc);
        goto inv_list_cleanup;
    }

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &returned_status, &n, OPAL_INT))) {
        goto inv_list_cleanup;
    }
    if(0 == returned_status) {
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &results_count, &n, OPAL_UINT16))) {
            goto inv_list_cleanup;
        }
        (*results) = OBJ_NEW(opal_list_t);
        for(uint16_t i = 0; i < results_count; ++i) {
            char* tmp_str = NULL;
            opal_value_t *tmp_value = NULL;

            n = 1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &tmp_str, &n, OPAL_STRING))) {
                goto inv_list_cleanup;
            }
            tmp_value = OBJ_NEW(opal_value_t);
            tmp_value->type = OPAL_STRING;
            tmp_value->data.string = tmp_str;
            opal_list_append(*results, &tmp_value->super);
        }
    } else {
        orcm_octl_info("no-results");
        rc = ORCM_SUCCESS;
        *results = NULL;
    }
inv_list_cleanup:
    if (NULL != xfer) {
        OBJ_RELEASE(xfer);
    }
    OBJ_RELEASE(buffer);
    return rc;
}

int orcm_octl_sensor_inventory_get(int cmd, char **argv)
{
    char* default_filter="%"; /* SQL default filter */
    int rv = ORCM_SUCCESS;
    int argc = opal_argv_count(argv);
    char *raw_node_list = NULL;
    int node_count = 0;
    char *filter = NULL;
    char *filter2 = NULL;
    char **argv_node_list = NULL;
    opal_list_t *node_list = NULL;
    opal_list_t *filter_list = NULL;
    orcm_db_filter_t *filter_item = NULL;
    opal_list_t *returned_list = NULL;
    opal_value_t *line = NULL;


    if (5 < argc || 4 > argc) {
        orcm_octl_usage("sensor-get-inventory", INVALID_USG);
        rv = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_sensor_inventory_get_cleanup;
    }

    raw_node_list = argv[3];
    if(5 == argc) {
        filter = argv[4];
    }
    orcm_logical_group_parse_array_string(raw_node_list, &argv_node_list);
    node_count = opal_argv_count(argv_node_list);

    if (NULL == raw_node_list || 0 == node_count) {
        orcm_octl_error("nodelist-extract");
        rv = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_sensor_inventory_get_cleanup;
    }

    /* Replace wildcard '*' with SQL wildcard '%' */
    bool found_wildcard = false;
    if(NULL == filter) {
        filter = default_filter;
    } else {
        for(size_t i = 0; i < strlen(filter); ++i) {
            if('*' == filter[i]) {
                filter[i] = '%';
                found_wildcard = true;
            }
        }
    }

    /* Build input node list */
    node_list = OBJ_NEW(opal_list_t);
    for(int i = 0; i < node_count; ++i) {
        opal_value_t *item = OBJ_NEW(opal_value_t);
        item->type = OPAL_STRING;
        item->data.string = strdup(argv_node_list[i]);
        opal_list_append(node_list, &item->super);
    }

    /* Build query filter list (currently one item) */
    filter_list = OBJ_NEW(opal_list_t);
    filter_item = OBJ_NEW(orcm_db_filter_t);
    filter_item->value.type = OPAL_STRING;
    filter_item->value.key = strdup("feature");
    if(true == found_wildcard) {
        asprintf(&filter2, "sensor_%s", filter);
    } else {
        asprintf(&filter2, "sensor_%s%%", filter);
    }
    filter_item->value.data.string = filter2;
    filter_item->op = STARTS_WITH;
    opal_list_append(filter_list, (opal_list_item_t*)filter_item);
    if(NULL != node_list && 0 < node_count) {
        opal_value_t *node = NULL;
        size_t length = node_count;
        OPAL_LIST_FOREACH(node, node_list, opal_value_t) {
            length += strlen(node->data.string) + 2;
        }
        filter = malloc(length + 1);
        if (NULL == filter) {
            if(NULL != node_list) {
                OBJ_RELEASE(node_list);
            }
            goto orcm_octl_sensor_inventory_get_cleanup;
        }
        filter[0] = '\0';
        OPAL_LIST_FOREACH(node, node_list, opal_value_t) {
            strcat(filter, "'");
            strcat(filter, node->data.string);
            strcat(filter, "',");
        }
        filter[strlen(filter) - 1] = '\0'; /* Remove trailing comma */
        filter_item = OBJ_NEW(orcm_db_filter_t);
        filter_item->value.type = OPAL_STRING;
        filter_item->value.key = strdup("hostname");
        filter_item->value.data.string = filter;
        filter_item->op = IN;
        opal_list_append(filter_list, &filter_item->value.super);
    }
    if(NULL != node_list) {
        OBJ_RELEASE(node_list);
    }
    /* Get list of results from scheduler (or other management node) */
    rv = get_inventory_list(cmd, filter_list, &returned_list);
    if(rv != ORCM_SUCCESS) {
        orcm_octl_error("orcmsched-fail");
        goto orcm_octl_sensor_inventory_get_cleanup;
    }

    /* Raw CSV output for now... */
    if(NULL != returned_list) {
        printf("\n");
        OPAL_LIST_FOREACH(line, returned_list, opal_value_t) {
            printf("%s\n", line->data.string);
        }
        OBJ_RELEASE(returned_list);
    }

orcm_octl_sensor_inventory_get_cleanup:
    opal_argv_free(argv_node_list);
    if(NULL != filter_list) {
        OBJ_RELEASE(filter_list);
    }
    return ORCM_SUCCESS; /* Seems octl prints an bad error message if you don't return success...*/
}


int orcm_octl_sensor_change_sampling(int command, char** cmdlist)
{
    int rv = ORCM_SUCCESS;
    int argc = opal_argv_count(cmdlist);
    char **nodelist = NULL;
    int cmd = command + ORCM_ENABLE_SENSOR_SAMPLING_COMMAND;
    char* build_str = NULL;
    opal_buffer_t* buf = NULL;
    orte_rml_recv_cb_t* xfer = NULL;
    orte_process_name_t tgt;
    int result = ORCM_SUCCESS;
    int cnt = 0;

    if(2 < command || 0 > command) {
        rv = ORCM_ERR_BAD_PARAM;
        orcm_octl_usage("sensor", INVALID_USG);
        goto change_sampling_cleanup;
    }


    if (4 != argc) {
        rv = ORCM_ERR_BAD_PARAM;
        orcm_octl_usage("sensor", INVALID_USG);
        goto change_sampling_cleanup;
    }

    orcm_logical_group_parse_array_string(cmdlist[2], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        opal_argv_free(nodelist);
        nodelist = NULL;
        SAFEFREE(build_str);
        orcm_octl_error("nodelist-notfound", cmdlist[2]);
        rv = ORCM_ERR_BAD_PARAM;
        goto change_sampling_cleanup;
    }

    buf = OBJ_NEW(opal_buffer_t);
    if(NULL == buf) {
        orcm_octl_error("allocate-memory", "command");
        rv = ORCM_ERR_OUT_OF_RESOURCE;
        goto change_sampling_cleanup;
    }

    if (ORCM_SUCCESS != (rv = opal_dss.pack(buf, &cmd, 1, ORCM_SENSOR_CMD_T))) {
        orcm_octl_error("pack");
        goto change_sampling_cleanup;
    }

    if (OPAL_SUCCESS != (rv = opal_dss.pack(buf, &cmdlist[3], 1, OPAL_STRING))) {
        orcm_octl_error("pack");
        goto change_sampling_cleanup;
    }

    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    if(NULL == xfer) {
        orcm_octl_error("allocate-memory","command" );
        rv = ORCM_ERR_OUT_OF_RESOURCE;
        goto change_sampling_cleanup;
    }

    for (int i = 0; i < opal_argv_count(nodelist); ++i) {
        OBJ_RETAIN(xfer);
        /* setup to receive the result */
        xfer->active = true;
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                                ORCM_RML_TAG_SENSOR,
                                ORTE_RML_NON_PERSISTENT,
                                orte_rml_recv_callback, xfer);

        if (ORCM_SUCCESS != (rv = orcm_cfgi_base_get_hostname_proc(nodelist[i], &tgt))) {
            orcm_octl_error("node-notfound", nodelist[i]);
            goto change_sampling_cleanup;
        }

        /* send command to node daemon */
        OBJ_RETAIN(buf);
        if (ORCM_SUCCESS !=
            (rv = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
            opal_buffer_t* tmp = buf;
            OBJ_RELEASE(tmp); // There are 2 ref counts here; dispose of one.

            orcm_octl_error("connection-fail");
            goto change_sampling_cleanup;
        }

        /* wait for status message */
        ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rv);
        if (ORCM_SUCCESS != rv) {
            orcm_octl_error("connection-fail");
            goto change_sampling_cleanup;
        }

        cnt=1;
        if (ORCM_SUCCESS != (rv = opal_dss.unpack(&xfer->data, &result,
                                                  &cnt, OPAL_INT))) {
            orcm_octl_error("unpack");
            goto change_sampling_cleanup;
        }
        if (ORCM_SUCCESS == result) {
            orcm_octl_info("sampling-success", nodelist[i]);
        } else {
            orcm_octl_error("sampling-fail", nodelist[i], cmdlist[3]);
        }
    }

change_sampling_cleanup:
    opal_argv_free(nodelist);
    SAFEFREE(build_str);
    SAFE_OBJ_RELEASE(xfer);
    SAFE_OBJ_RELEASE(buf);
    return rv;
}


static void orcm_octl_sensor_store_process_error(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer, char **list)
{

    SAFE_OBJ_RELEASE(buf);
    SAFE_OBJ_RELEASE(xfer);
    SAFEFREE (list);

    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
}

static void orcm_octl_sensor_analytics_output_setup(orte_rml_recv_cb_t *xfer)
{
    /* setup to receive the result */
    xfer->active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_ANALYTICS,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, xfer);

}

static int orcm_octl_sensor_parse_nodelist(char **value, char ***nodelist)
{
    int rc;

    if (strcmp(value[3], "")) {
        rc = orcm_logical_group_parse_array_string(value[3], nodelist);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_error("nodelist-extract");
            return ORCM_ERROR;
        }

    }
    else {
        orcm_octl_error("nodelist-extract");
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;
}

static int orcm_octl_sensor_store_pack_buffer(opal_buffer_t *buf, orte_rml_recv_cb_t *xfer, int storage_command)
{
    int rc;
    orcm_analytics_cmd_flag_t command;

    command = ORCM_ANALYTICS_SENSOR_STORAGE_POLICY;
    /* pack the  command flag */
    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, OPAL_UINT8))) {

        return rc;
    }

    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &storage_command, 1, OPAL_UINT8))) {

        return rc;
    }
    return ORCM_SUCCESS;
}

static int orcm_octl_sensor_store_send_buffer(orte_process_name_t *wf_agg,
                                              opal_buffer_t *buf, orte_rml_recv_cb_t *xfer)
{
    int rc;

    /* send it to the aggregator */
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(wf_agg, buf,
                                                      ORCM_RML_TAG_ANALYTICS,
                                                      orte_rml_send_callback, NULL))) {
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

static int orcm_octl_sensor_store_unpack_buffer(orte_rml_recv_cb_t *xfer)
{
    int n;
    int rc;
    int response;

    n=1;
    if (ORCM_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &response, &n, OPAL_INT))) {
        return ORCM_ERROR;
    }
    orcm_octl_info("storage-response", response);
    return ORCM_SUCCESS;

}

int orcm_octl_sensor_store(int storage_command, char** cmdlist)
{
    orte_rml_recv_cb_t *xfer = NULL;
    opal_buffer_t *buf;
    orte_process_name_t wf_agg;
    char **nodelist=NULL;
    int node_index;
    int rc;

    if (4 != opal_argv_count(cmdlist) || 0 > storage_command) {
        orcm_octl_usage("sensor-store", INVALID_USG);
        return ORCM_ERR_BAD_PARAM;
    }

    rc = orcm_octl_sensor_parse_nodelist(cmdlist, &nodelist);
    if (ORCM_SUCCESS != rc) {
        SAFEFREE(nodelist);
        return rc;
    }

    buf = OBJ_NEW(opal_buffer_t);
    if (NULL == buf) {
        SAFEFREE (nodelist);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    for (node_index = 0; node_index < opal_argv_count(nodelist); node_index++) {
        xfer = OBJ_NEW(orte_rml_recv_cb_t);
        if (NULL == xfer) {
            SAFEFREE (nodelist);
            OBJ_RELEASE(buf);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        orcm_octl_sensor_analytics_output_setup(xfer);

        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[node_index],
                                                                   &wf_agg))) {
            orcm_octl_sensor_store_process_error(buf, xfer, nodelist);
            return rc;
        }

        OBJ_RETAIN(buf);

        rc = orcm_octl_sensor_store_pack_buffer(buf, xfer, storage_command);
        if (ORCM_SUCCESS != rc) {
            SAFE_OBJ_RELEASE(buf);
            orcm_octl_sensor_store_process_error(buf, xfer, nodelist);
            return rc;
        }

        rc = orcm_octl_sensor_store_send_buffer(&wf_agg, buf, xfer);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_sensor_store_process_error(buf, xfer, nodelist);
            return rc;
        }

        ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_sensor_store_process_error(buf, xfer, nodelist);
            return rc;
        }

        rc = orcm_octl_sensor_store_unpack_buffer(xfer);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_sensor_store_process_error(buf, xfer, nodelist);
            return rc;
        }
        SAFE_OBJ_RELEASE(xfer);
        orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_ANALYTICS);
    }
    SAFE_OBJ_RELEASE(buf);
    SAFEFREE (nodelist);
    return ORCM_SUCCESS;
}
