/*
 * Copyright (c) 2014-2016  Intel Corporation.  All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "orte/mca/notifier/notifier.h"
#include "orcm/util/logical_group.h"

#include "orcm/tools/octl/common.h"

static int severity_string_to_enum(char *sevstr);
static char* severity_enum_to_string(orte_notifier_severity_t sev);
static int action_string_to_enum(char *action);

int orcm_octl_get_notifier_policy(int cmd, char **argv)
{
    orcm_cmd_server_flag_t command;
    opal_buffer_t *buf = NULL;
    int rc = ORCM_SUCCESS;
    int i = 0, cnt = 0, count = 0;
    int response = ORCM_SUCCESS;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;
    char *sev = NULL;
    char *action = NULL;
    char *error = NULL;

    orte_notifier_severity_t severity, sev_indx;

    if (4 != opal_argv_count(argv)) {
        rc = ORCM_ERR_BAD_PARAM;
        orcm_octl_usage("notifier-get", INVALID_USG);
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

    command = ORCM_GET_NOTIFIER_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_CMD_SERVER_T))) {
        orcm_octl_error("pack");
        error = PACKERR;
        goto done;
    }

    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_CMD_SERVER_T))) {
        orcm_octl_error("pack");
        goto done;
    }

    /* Loop through the nodelist to get notifier policy */
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    for (i = 0; i < opal_argv_count(nodelist); i++) {
        OBJ_RETAIN(buf);
        OBJ_RETAIN(xfer);

        xfer->active = true;
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_CMD_SERVER,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, xfer);

        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            orcm_octl_error("node-notfound", nodelist[i]);
            goto done;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_CMD_SERVER,
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
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &response,
                                                  &cnt, OPAL_INT))) {
            orcm_octl_error("unpacker");
            goto done;
        }

        if ( ORCM_SUCCESS != response ) {
             if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &error,
                                                   &cnt, OPAL_STRING))) {
                 orcm_octl_error("unpacker");
                 goto done;
             }
             orcm_octl_info("notifier-policy", "get", nodelist[i]);
             goto done;
        } else {
             cnt = 1;
             if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &count,
                                                      &cnt, OPAL_INT))) {
                 orcm_octl_error("unpacker");
                 goto done;
             }

             if (0 == count) {
                 orcm_octl_info("notifier-header");
                 printf("%-10s    %6s    %10s \n", nodelist[i], "n/a", "n/a");
                 goto done;
             }
             opal_buffer_t *result_buf = NULL;
             if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data,  &result_buf,
                                               &cnt, OPAL_BUFFER))) {
                 orcm_octl_error("unpacker");
                 goto done;
             }
             orcm_octl_info("notifier-header");
             for(sev_indx = ORTE_NOTIFIER_EMERG; sev_indx <= ORTE_NOTIFIER_DEBUG; sev_indx++) {
                /* unpack severity */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(result_buf,  &severity,
                                                  &cnt, ORTE_NOTIFIER_SEVERITY_T))) {
                    orcm_octl_error("unpacker");
                    goto done;
                }

                /* unpack action */
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(result_buf, &action,
                                                  &cnt, OPAL_STRING))) {
                    orcm_octl_error("unpacker");
                    goto done;
                }

                sev = (char *)  severity_enum_to_string(severity);
                if (NULL != sev) {
                    orcm_octl_info("notifier-data", nodelist[i], sev, action);
                }
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
    if (sev) {
        free(sev);
    }
    if (action) {
        free(action);
    }

    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_CMD_SERVER);

    return rc;
}


int orcm_octl_set_notifier_policy(int cmd, char **argv)
{
    orcm_cmd_server_flag_t command;
    opal_buffer_t *buf = NULL;
    int rc = ORCM_SUCCESS;
    int result = ORCM_SUCCESS;
    int i = 0, cnt = 0;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;
    int sev;
    int act;
    char *error = NULL;

    if (6 != opal_argv_count(argv)) {
        rc = ORCM_ERR_BAD_PARAM;
        orcm_octl_usage("notifier-set", INVALID_USG);
        return rc;
    }

    /* setup the receiver nodelist */
    orcm_logical_group_parse_array_string(argv[5], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        opal_argv_free(nodelist);
        nodelist = NULL;
        orcm_octl_error("nodelist-notfound", argv[5]);
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }

    /* pack the buffer to send */
    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_SET_NOTIFIER_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_CMD_SERVER_T))) {
        orcm_octl_error("pack");
        goto done;
    }

    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_CMD_SERVER_T))) {
        orcm_octl_error("pack");
        error = PACKERR;
        goto done;
    }

    /* notification severity string to enum */
    if (-1 == (sev = severity_string_to_enum(argv[3]))) {

        orcm_octl_error("item-notfound", "severity");
        rc = -1;
        goto done;
    }

    /* pack notification severity */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &sev,
                                            1, ORTE_NOTIFIER_SEVERITY_T))) {
        error = PACKERR;
        goto done;
    }
    /* notification action string to enum */
    if (-1 == (act = action_string_to_enum(argv[4]))) {

        orcm_octl_error("item-notfound", "action");
        rc = -1;
        goto done;
    }
    /* pack notification action */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &argv[4],
                                            1, OPAL_STRING))) {
        orcm_octl_error("pack");
        goto done;
    }

    /* Loop through the nodelist to set notifier policy */
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    for (i = 0; i < opal_argv_count(nodelist); i++) {
        OBJ_RETAIN(buf);
        OBJ_RETAIN(xfer);

        xfer->active = true;
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_CMD_SERVER,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, xfer);


        orcm_octl_info("notifier-policy", "set", nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            orcm_octl_error("node-notfound", nodelist[i]);
            goto done;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_CMD_SERVER,
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
            error = UNPACKERR;
            goto done;
        }
        if (ORCM_SUCCESS == result) {
            orcm_octl_info("success");
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
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_CMD_SERVER);
    return rc;
}

static int severity_string_to_enum(char *sevstr)
{
    orte_notifier_severity_t sev;
    /* pack severity level */
    if ( 0 == strcasecmp(sevstr, "emerg") ) {
        sev = ORTE_NOTIFIER_EMERG;
    } else if ( 0 == strcasecmp(sevstr, "alert") ) {
        sev = ORTE_NOTIFIER_ALERT;
    } else if ( 0 == strcasecmp(sevstr, "crit") ) {
        sev = ORTE_NOTIFIER_CRIT;
    } else if ( 0 == strcasecmp(sevstr, "error") ) {
        sev = ORTE_NOTIFIER_ERROR;
    } else if ( 0 == strcasecmp(sevstr, "warn") ) {
        sev = ORTE_NOTIFIER_WARN;
    } else if ( 0 == strcasecmp(sevstr, "notice") ) {
        sev = ORTE_NOTIFIER_NOTICE;
    } else if ( 0 == strcasecmp(sevstr, "info") ) {
        sev = ORTE_NOTIFIER_INFO;
    } else if ( 0 == strcasecmp(sevstr, "debug") ) {
        sev = ORTE_NOTIFIER_DEBUG;
    } else {
        return -1;
    }
    return sev;
}

static char* severity_enum_to_string(orte_notifier_severity_t severity)
{
    char* sev = NULL;
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
    return sev;
}

static int action_string_to_enum(char *action)
{
    int act;
    /* pack severity level */
    if ( 0 == strcasecmp(action, "syslog") ) {
        act = 1;
    } else if ( 0 == strcasecmp(action, "smtp") ) {
        act = 2;
    } else {
        return -1;
    }
    return act;
}
