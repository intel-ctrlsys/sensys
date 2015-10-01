/*
 * Copyright (c) 2014-2015  Intel, Inc.  All rights reserved.
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

/***
Remove 'implicit' warnings...
****/
int orcm_octl_sensor_inventory_get(int command, char** argv);

#define TAG  "octl:command-line:failure"
#define PACKERR  "internal buffer pack error"
#define UNPACKERR "internal buffer unpack error"

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
    char *error = NULL;

    orte_notifier_severity_t severity;

    if (4 != opal_argv_count(argv)) {
        error = "incorrect arguments!";
        rc = ORCM_ERR_BAD_PARAM;
        orte_show_help("help-octl.txt",
                       "octl:get:policy-usage",
                       true, error, ORTE_ERROR_NAME(rc), rc);
        return rc;
    }

    /* setup the receiver nodelist */
    orcm_logical_group_node_names(argv[3], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        opal_argv_free(nodelist);
        nodelist = NULL;
        error = "nodelist not found";
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }

    /* pack the buffer to send */
    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_GET_SENSOR_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        error = PACKERR;
        goto done;
    }

    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        error = PACKERR;
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

        printf("\nORCM getting sensor policy from Node:%s\n", nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            error = "incorrect node name";
            goto done;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
            error = "daemon contact failed";
            goto done;
        }

        /* wait for status message */
        ORTE_WAIT_FOR_COMPLETION(xfer->active);

        cnt=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &num,
                                                  &cnt, OPAL_INT))) {
            error = UNPACKERR;
            goto done;
        }

        if ( 0 == num ) {
            rc = ORCM_SUCCESS;
            printf("\nThere is no active policy!\n");
            goto done;
        } else {
             printf("\nSensor      Threshold        Hi/Lo    Max_Count/Time_Window    Severity      Action\n");
             printf("-----------------------------------------------------------------------------------\n");
            for (i = 0; i < num; i++) {
                /* unpack sensor name */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &sensor_name,
                                                  &cnt, OPAL_STRING))) {
                    error = UNPACKERR;
                    goto done;
                }

                /* unpack threshold */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &threshold,
                                                  &cnt, OPAL_FLOAT))) {
                    error = UNPACKERR;
                    goto done;
                }

                /* unpack threshold type */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &hi_thres,
                                                  &cnt, OPAL_BOOL))) {
                    error = UNPACKERR;
                    goto done;
                }

                /* unpack max count */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &max_count,
                                                  &cnt, OPAL_INT))) {
                    error = UNPACKERR;
                    goto done;
                }

                /* unpack time window */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &time_window,
                                                  &cnt, OPAL_INT))) {
                    error = UNPACKERR;
                    goto done;
                }

                /* unpack severity */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &severity,
                                                  &cnt, OPAL_INT))) {
                    error = UNPACKERR;
                    goto done;
                }

                /* unpack action */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &action,
                                                  &cnt, OPAL_STRING))) {
                    error = UNPACKERR;
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
                    printf("%-10s %8.3f %3s     %5s   %5d in %5d seconds     %6s    %10s \n", sensor_name,
                           threshold, " °C", threstype, max_count, time_window, sev, action);
                } else if ( 0 == strcmp(sensor_name, "corefreq") ) {
                    printf("%-10s %8.3f %3s     %5s   %5d in %5d seconds     %6s    %10s \n", sensor_name,
                           threshold, "GHz", threstype, max_count, time_window, sev, action);
                } else {
                    printf("%-10s %8.3f %3s     %5s   %5d in %5d seconds     %6s    %10s \n", sensor_name,
                           threshold, "   ", threstype, max_count, time_window, sev, action);
                }
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
    if (sev) {
        free(sev);
    }
    if (threstype) {
        free(threstype);
    }
    if (action) {
        free(action);
    }
    if (sensor_name) {
        free(sensor_name);
    }

    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SENSOR);

    if (ORCM_SUCCESS != rc) {
        orte_show_help("help-octl.txt",
                       TAG, true, error, ORTE_ERROR_NAME(rc), rc);
    }

    return rc;
}


int orcm_octl_sensor_policy_set(int cmd, char **argv)
{
    orcm_sensor_cmd_flag_t command;
    opal_buffer_t *buf = NULL;
    int cnt, i;
    int rc = ORCM_SUCCESS;
    int result = ORCM_SUCCESS;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;
    float threshold;
    bool hi_thres;
    int max_count, time_window;
    orte_notifier_severity_t sev;
    char *error = NULL;

    if (11 != opal_argv_count(argv)) {
        error = "incorrect arguments!";
        rc = ORCM_ERR_BAD_PARAM;
        orte_show_help("help-octl.txt",
                       "octl:set:policy-usage",
                       true, error, ORTE_ERROR_NAME(rc), rc);
        return rc;
    }

    /* setup the receiver nodelist */
    orcm_logical_group_node_names(argv[3], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        opal_argv_free(nodelist);
        nodelist = NULL;
        error = "nodelist not found";
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }

    /* pack the buffer to send */
    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_SET_SENSOR_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        error = PACKERR;
        goto done;
    }

    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        error = PACKERR;
        goto done;
    }

    /* pack sensor name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &argv[4],
                                            1, OPAL_STRING))) {
        error = PACKERR;
        goto done;
    }

    /* pack threshold value */
    threshold = (float)atof(argv[5]);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &threshold,
                                            1, OPAL_FLOAT))) {
        error = PACKERR;
        goto done;
    }

    /* pack threshold type */
    if ( 0 == strcmp(argv[6], "hi") ) {
        hi_thres = true;
    } else if ( 0 == strcmp(argv[6], "lo") ) {
        hi_thres = false;
    } else {
        error = "threshold type not found";
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &hi_thres,
                                            1, OPAL_BOOL))) {
        error = PACKERR;
        goto done;
    }

    /* pack max count */
    if (isdigit(argv[7][strlen(argv[7]) - 1])) {
        max_count = (int)strtol(argv[7], NULL, 10);
    } else {
        error = "max count is not an integer";
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &max_count,
                                            1, OPAL_INT))) {
        error = UNPACKERR;
        goto done;
    }

    /* pack time window */
    if (isdigit(argv[8][strlen(argv[8]) - 1])) {
        time_window = (int)strtol(argv[8], NULL, 10);
    } else {
        error = "time window is not an integer";
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &time_window,
                                            1, OPAL_INT))) {
        error = "internal buffer pack error";
        goto done;
    }

    /* pack severity level */
    if ( 0 == strcmp(argv[9], "emerg") ) {
        sev = ORTE_NOTIFIER_EMERG;
    } else if ( 0 == strcmp(argv[9], "alert") ) {
        sev = ORTE_NOTIFIER_ALERT;
    } else if ( 0 == strcmp(argv[9], "crit") ) {
        sev = ORTE_NOTIFIER_CRIT;
    } else if ( 0 == strcmp(argv[9], "error") ) {
        sev = ORTE_NOTIFIER_ERROR;
    } else if ( 0 == strcmp(argv[9], "warn") ) {
        sev = ORTE_NOTIFIER_WARN;
    } else if ( 0 == strcmp(argv[9], "notice") ) {
        sev = ORTE_NOTIFIER_NOTICE;
    } else if ( 0 == strcmp(argv[9], "info") ) {
        sev = ORTE_NOTIFIER_INFO;
    } else if ( 0 == strcmp(argv[9], "debug") ) {
        sev = ORTE_NOTIFIER_DEBUG;
    } else {
        error = "incorrect severity level";
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &sev,
                                            1, OPAL_INT))) {
        error = PACKERR;
        goto done;
    }

    /* pack notification action */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &argv[10],
                                            1, OPAL_STRING))) {
        error = PACKERR;
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

        printf("\nORCM setting sensor policy on Node:%s\n", nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            error = "incorrect node name";
            goto done;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
            error = "daemon contact failed";
            goto done;
        }

        /* wait for status message */
        ORTE_WAIT_FOR_COMPLETION(xfer->active);

        cnt=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &result,
                                                  &cnt, OPAL_INT))) {
            error = UNPACKERR;
            goto done;
        }
        if (ORCM_SUCCESS == result) {
            printf("\nSuccess\n");
        } else {
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &error,
                                                 &cnt, OPAL_STRING))) {
                error = UNPACKERR;
                goto done;
            }
            rc = result;
            goto done;
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
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SENSOR);
    if (ORCM_SUCCESS != rc) {
        orte_show_help("help-octl.txt",
                       TAG,
                       true, error, ORTE_ERROR_NAME(rc), rc);
    }
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
        error = "incorrect arguments!";
        rc = ORCM_ERR_BAD_PARAM;
        orte_show_help("help-octl.txt",
                       "octl:set:sample-rate-usage",
                       true, error, ORTE_ERROR_NAME(rc), rc);
        return rc;
    }

    if (isdigit(argv[4][strlen(argv[4]) - 1])) {
        sample_rate = (int)strtol(argv[4], NULL, 10);
    } else {
        error = "sample rate is not an integer";
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }

    orcm_logical_group_node_names(argv[5], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        opal_argv_free(nodelist);
        nodelist = NULL;
        error = "nodelist not found";
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }

    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_SET_SENSOR_COMMAND;
    /* pack sensor set command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        error = PACKERR;
        goto done;
    }
    command = cmd;
    /* pack sensor sample rate sub command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        error = PACKERR;
        goto done;
    }
    /* pack sensor name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &argv[3],
                                            1, OPAL_STRING))) {
        error = PACKERR;
        goto done;
    }

    /* pack sample_rate value */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &sample_rate,
                                            1, OPAL_INT))) {
        error = PACKERR;
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

        printf("\nORCM setting sensor:%s sample-rate on node:%s\n", argv[3], nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            error = "incorrect node name";
            goto done;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
            error = "daemon contact failed";
            goto done;
        }

        /* wait for status message */
        ORTE_WAIT_FOR_COMPLETION(xfer->active);

        cnt=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &result,
                                                  &cnt, OPAL_INT))) {
            error = UNPACKERR;
            goto done;
         }
         if (ORCM_SUCCESS == result) {
             printf("\nSuccess\n");
         } else {
             if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &error,
                                                  &cnt, OPAL_STRING))) {
                 error = UNPACKERR;
                 goto done;
             }
             rc = result;
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
    if (ORCM_SUCCESS != rc) {
        orte_show_help("help-octl.txt",
                       TAG, true, error, ORTE_ERROR_NAME(rc), rc);
    }
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
        error = "incorrect arguments!";
        rc = ORCM_ERR_BAD_PARAM;
        orte_show_help("help-octl.txt",
                       "octl:get:sample-rate-usage",
                       true, error, ORTE_ERROR_NAME(rc), rc);
       return rc;
    }

    /* setup the receiver nodelist */
    orcm_logical_group_node_names(argv[4], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        opal_argv_free(nodelist);
        nodelist = NULL;
        error = "nodelist not found";
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }

    /* pack the buffer to send */
    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_GET_SENSOR_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        error = PACKERR;
        goto done;
    }

    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        error = PACKERR;
        goto done;
    }

    /* pack sensor name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &argv[3],
                                            1, OPAL_STRING))) {
        error = PACKERR;
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

        printf("\nORCM getting sensor %s sample-rate  from node:%s\n",
                argv[3], nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            error = "incorrect node name";
            goto done;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
            error = "daemon contact failed";
            goto done;
        }

        /* wait for status message */
        ORTE_WAIT_FOR_COMPLETION(xfer->active);

        cnt=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &result,
                                                  &cnt, OPAL_INT))) {
            error = UNPACKERR;
            goto done;
        }

        if ( 0 != result ) {
            cnt=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &error,
                                                       &cnt, OPAL_STRING))) {
                error = UNPACKERR;
                goto done;
            }
            rc = result;
            goto done;
        } else {
             printf("\nSensor     sample-rate\n");
             printf("------------------------------------------------------------------------------\n");
            /* unpack sensor name */
            cnt = 1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &sensor_name,
                                              &cnt, OPAL_STRING))) {
                error = UNPACKERR;
                goto done;
            }

            /* unpack sample rate */
            cnt = 1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data,
                                                      &sample_rate,
                                                      &cnt, OPAL_INT))) {
                error = UNPACKERR;
                goto done;
            }

            printf("%s            %d\n", sensor_name, sample_rate);
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

    if (ORCM_SUCCESS != rc) {
        orte_show_help("help-octl.txt",
                       TAG, true, error, ORTE_ERROR_NAME(rc), rc);
    }
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
    ORTE_WAIT_FOR_COMPLETION(xfer->active);

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
        printf("* No Results Returned *\n");
        rc = ORCM_SUCCESS;
        *results = NULL;
    }
inv_list_cleanup:
    OBJ_RELEASE(xfer);
    OBJ_RELEASE(buffer);
    OPAL_LIST_RELEASE(filterlist);
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

    if(ORCM_GET_DB_SENSOR_INVENTORY_COMMAND != cmd) {
        fprintf(stdout, "ERROR: incorrect command argument: %d\n", cmd);
        rv = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_sensor_inventory_get_cleanup;
    }

    if (5 < argc || 4 > argc) {
        fprintf(stdout, "ERROR: incorrect arguments!\n  Usage: \"sensor get inventory nodelist [wildcard-filter]\"\n");
        rv = ORCM_ERR_BAD_PARAM;
        goto orcm_octl_sensor_inventory_get_cleanup;
    }

    raw_node_list = argv[3];
    if(5 == argc) {
        filter = argv[4];
    }
    orcm_logical_group_node_names(raw_node_list, &argv_node_list);
    node_count = opal_argv_count(argv_node_list);

    if (NULL == raw_node_list || 0 == node_count) {
        fprintf(stdout, "ERROR: unable to extract nodelist or an empty list was specified\n");
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
        OPAL_LIST_RELEASE(node_list);
    }
    /* Get list of results from scheduler (or other management node) */
    rv = get_inventory_list(cmd, filter_list, &returned_list);
    if(rv != ORCM_SUCCESS) {
        fprintf(stderr, "ERROR: unable to get results via ORCM scheduler\n");
        goto orcm_octl_sensor_inventory_get_cleanup;
    }

    /* Raw CSV output for now... */
    if(NULL != returned_list) {
        OPAL_LIST_FOREACH(line, returned_list, opal_value_t) {
            printf("%s\n", line->data.string);
        }
        OPAL_LIST_RELEASE(returned_list);
    }

orcm_octl_sensor_inventory_get_cleanup:
    if(NULL != raw_node_list) {
        opal_argv_free(argv_node_list);
    }
    return ORCM_SUCCESS; /* Seems octl prints an bad error message if you don't return success...*/
}
