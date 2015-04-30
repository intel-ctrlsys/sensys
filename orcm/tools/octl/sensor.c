/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm/tools/octl/common.h"
#include "orte/mca/notifier/notifier.h"

int orcm_octl_sensor_policy_get(int cmd, char **argv)
{
    orcm_sensor_cmd_flag_t command;
    opal_buffer_t *buf = NULL;
    int rc, cnt, i;
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
        fprintf(stderr, "incorrect arguments to \"sensor get policy\"\n");
        return ORCM_ERR_BAD_PARAM;
    }

    /* setup the receiver nodelist */
    orte_regex_extract_node_names (argv[3], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        fprintf(stdout, "Error: unable to extract nodelist\n");
        opal_argv_free(nodelist);
        return ORCM_ERR_BAD_PARAM;
    }

    /* pack the buffer to send */
    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_GET_SENSOR_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto done;
    }

    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        ORTE_ERROR_LOG(rc);
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

        fprintf(stdout, "ORCM getting sensor policy from Node:%s\n", nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            ORTE_ERROR_LOG(rc);
            goto done;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            goto done;
        }

        /* wait for status message */
        ORTE_WAIT_FOR_COMPLETION(xfer->active);

        cnt=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &num,
                                                  &cnt, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            goto done;
        }

        if ( 0 == num ) {
            rc = ORCM_SUCCESS;
            fprintf(stdout, "There is no active policy!\n");
            goto done;
        } else {
             printf("Sensor      Threshold        Hi/Lo    Max_Count/Time_Window    Severity      Action\n");
             printf("-----------------------------------------------------------------------------------\n");
            for (i = 0; i < num; i++) {
                /* unpack sensor name */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &sensor_name,
                                                  &cnt, OPAL_STRING))) {
                    ORTE_ERROR_LOG(rc);
                    goto done;
                }

                /* unpack threshold */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &threshold,
                                                  &cnt, OPAL_FLOAT))) {
                    ORTE_ERROR_LOG(rc);
                    goto done;
                }

                /* unpack threshold type */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &hi_thres,
                                                  &cnt, OPAL_BOOL))) {
                    ORTE_ERROR_LOG(rc);
                    goto done;
                }

                /* unpack max count */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &max_count,
                                                  &cnt, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    goto done;
                }

                /* unpack time window */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &time_window,
                                                  &cnt, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    goto done;
                }

                /* unpack severity */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &severity,
                                                  &cnt, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    goto done;
                }

                /* unpack action */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &action,
                                                  &cnt, OPAL_STRING))) {
                    ORTE_ERROR_LOG(rc);
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
    if ( buf ) {
        OBJ_RELEASE(buf);
    }
    if ( xfer ) {
        OBJ_RELEASE(xfer);
    }
    if ( nodelist ) {
        opal_argv_free(nodelist);
    }

    if ( ORCM_SUCCESS != rc ) {
        fprintf(stdout, "Error\n");
    }
    return rc;
}


int orcm_octl_sensor_policy_set(int cmd, char **argv)
{
    orcm_sensor_cmd_flag_t command;
    opal_buffer_t *buf = NULL;
    int rc, cnt, i, result;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;
    float threshold;
    bool hi_thres;
    int max_count, time_window;
    orte_notifier_severity_t sev;

    if (11 != opal_argv_count(argv)) {
        fprintf(stderr, "incorrect arguments to \"sensor set policy\"\n");
        return ORCM_ERR_BAD_PARAM;
    }

    /* setup the receiver nodelist */
    orte_regex_extract_node_names (argv[3], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        fprintf(stdout, "Error: unable to extract nodelist\n");
        opal_argv_free(nodelist);
        return ORCM_ERR_BAD_PARAM;
    }

    /* pack the buffer to send */
    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_SET_SENSOR_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto done;
    }

    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto done;
    }

    /* pack sensor name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &argv[4],
                                            1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto done;
    }

    /* pack threshold value */
    threshold = (float)atof(argv[5]);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &threshold,
                                            1, OPAL_FLOAT))) {
        ORTE_ERROR_LOG(rc);
        goto done;
    }

    /* pack threshold type */
    if ( 0 == strcmp(argv[6], "hi") ) {
        hi_thres = true;
    } else if ( 0 == strcmp(argv[6], "lo") ) {
        hi_thres = false;
    } else {
        fprintf(stderr, "incorrect argument of threshold type to \"sensor set policy\"\n");
        return ORCM_ERR_BAD_PARAM;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &hi_thres,
                                            1, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        goto done;
    }

    /* pack max count */
    if (isdigit(argv[7][strlen(argv[7]) - 1])) {
        max_count = (int)strtol(argv[7], NULL, 10);
    } else {
        fprintf(stderr, "incorrect argument of max count is not an integer \n");
        return ORCM_ERR_BAD_PARAM;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &max_count,
                                            1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        goto done;
    }

    /* pack time window */
    if (isdigit(argv[8][strlen(argv[8]) - 1])) {
        time_window = (int)strtol(argv[8], NULL, 10);
    } else {
        fprintf(stderr, "incorrect argument of time window is not an integer \n");
        return ORCM_ERR_BAD_PARAM;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &time_window,
                                            1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
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
        fprintf(stderr, "incorrect argument of severity level to \"sensor set policy\"\n");
        return ORCM_ERR_BAD_PARAM;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &sev,
                                            1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        goto done;
    }

    /* pack notification action */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &argv[10],
                                            1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
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

        fprintf(stdout, "ORCM setting sensor policy on Node:%s\n", nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            ORTE_ERROR_LOG(rc);
            goto done;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            goto done;
        }

        /* wait for status message */
        ORTE_WAIT_FOR_COMPLETION(xfer->active);

        cnt=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &result,
                                                  &cnt, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            goto done;
        }
        if (ORCM_SUCCESS == result) {
            fprintf(stdout, "Success\n");
        } else {
            fprintf(stdout, "Failure\n");
        }
    }

done:
    if ( buf ) {
        OBJ_RELEASE(buf);
    }
    if ( xfer ) {
        OBJ_RELEASE(xfer);
    }
    if ( nodelist ) {
        opal_argv_free(nodelist);
    }

    if ( ORCM_SUCCESS != rc ) {
        fprintf(stdout, "Error\n");
    }
    return rc;
}

int orcm_octl_sensor_sample_rate_set(int cmd, char **argv)
{
    orcm_sensor_cmd_flag_t command;
    int sample_rate = 0;
    opal_buffer_t *buf = NULL;
    int rc = ORCM_SUCCESS;
    int cnt, i, result;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;

    if (6 != opal_argv_count(argv)) {
        fprintf(stderr, "incorrect arguments! \n  usage:\"sensor set sample-rate <sensor-name> <sample-rate> <node-name>\"\n");
        return ORCM_ERR_BAD_PARAM;
    }

    if (isdigit(argv[4][strlen(argv[4]) - 1])) {
        sample_rate = (int)strtol(argv[4], NULL, 10);
    } else {
        fprintf(stderr, "incorrect argument sample rate!\n \"%s\" is not an integer \n", argv[4]);
        return ORCM_ERR_BAD_PARAM;
    }

    orte_regex_extract_node_names (argv[5], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        fprintf(stderr, "incorrect argument nodelist! \n unable to extract nodelist\n");
        opal_argv_free(nodelist);
        return ORCM_ERR_BAD_PARAM;
    }

    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_SET_SENSOR_COMMAND;
    /* pack sensor set command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto finish;
    }
    command = cmd;
    /* pack sensor sample rate sub command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto finish;
    }
    /* pack sensor name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &argv[3],
                                            1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto finish;
    }

    /* pack sample_rate value */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &sample_rate,
                                            1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        goto finish;
    }

    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    /* Loop through the nodelist and set the sample rate */
    for (i = 0; i < opal_argv_count(nodelist); i++) {
        OBJ_RETAIN(buf);
        OBJ_RETAIN(xfer);
        /* setup to receive the result */
        xfer->active = true;
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                                ORCM_RML_TAG_SENSOR,
                                ORTE_RML_NON_PERSISTENT,
                                orte_rml_recv_callback, xfer);

        fprintf(stdout, "ORCM setting sensor:%s sample-rate on node:%s\n", argv[3], nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            ORTE_ERROR_LOG(rc);
            goto finish;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            goto finish;
        }

        /* wait for status message */ 
        ORTE_WAIT_FOR_COMPLETION(xfer->active);

        cnt=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &result,
                                                  &cnt, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            goto finish;
         }
         if (ORCM_SUCCESS == result) {
             fprintf(stdout, "Success\n");
         } else {
             fprintf(stdout, "Failure\n");
         }
    }

finish:
    if(buf) OBJ_RELEASE(buf);
    if(xfer) OBJ_RELEASE(xfer);
    if(nodelist) opal_argv_free(nodelist);
    if (ORCM_SUCCESS != rc) {
        fprintf(stdout, "Failure\n");
    }
    return rc;
}

int orcm_octl_sensor_sample_rate_get(int cmd, char **argv)
{
    orcm_sensor_cmd_flag_t command;
    opal_buffer_t *buf = NULL;
    int rc, response, cnt, i;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;
    char *sensor_name = NULL; 
    int sample_rate = 0;

    if (5 != opal_argv_count(argv)) {
        fprintf(stderr, "incorrect arguments! \n\n usage: \"sensor get sample-rate <sensor-name> nodelist\"\n");
        return ORCM_ERR_BAD_PARAM;
    }

    /* setup the receiver nodelist */
    orte_regex_extract_node_names (argv[4], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        fprintf(stdout, "Error: unable to extract nodelist\n");
        opal_argv_free(nodelist);
        return ORCM_ERR_BAD_PARAM;
    }

    /* pack the buffer to send */
    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_GET_SENSOR_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto done;
    }

    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto done;
    }

    /* pack sensor name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &argv[3],
                                            1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
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

        fprintf(stdout, "ORCM getting sensor %s sample-rate  from node:%s\n", 
                argv[3], nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            ORTE_ERROR_LOG(rc);
            goto done;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            goto done;
        }

        /* wait for status message */
        ORTE_WAIT_FOR_COMPLETION(xfer->active);

        cnt=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &response,
                                                  &cnt, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            goto done;
        }

        if ( 0 != response ) {
            rc = ORCM_SUCCESS;
            fprintf(stdout, "Bad parameter error\n");
            goto done;
        } else {
             printf("Sensor     sample-rate\n");
             printf("------------------------------------------------------------------------------\n");
            /* unpack sensor name */
            cnt = 1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &sensor_name,
                                              &cnt, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                goto done;
            }

            /* unpack sample rate */
            cnt = 1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, 
                                                      &sample_rate,
                                                      &cnt, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                goto done;
            }

            printf("%s            %d\n", sensor_name, sample_rate);
        }
    }
done:
    return ORCM_SUCCESS;
}
