/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "opal/dss/dss.h"
#include "orte/mca/notifier/notifier.h"
#include "orcm/util/logical_group.h"

#include "orcm/tools/octl/common.h"

int orcm_octl_get_notifier_smtp(int cmd, char **argv)
{
    orcm_cmd_server_flag_t command;
    opal_buffer_t *buf = NULL;
    int rc = ORCM_SUCCESS;
    int i = 0, cnt = 0, count = 0;
    int response = ORCM_SUCCESS;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    opal_value_t *kv = NULL;
    char **nodelist = NULL;
    char *error = NULL;
    int indx =0;

    if (4 != opal_argv_count(argv)) {
        rc = ORCM_ERR_BAD_PARAM;
        orcm_octl_usage("notifier-get-smtp", INVALID_USG);
        return rc;
    }

    /* setup the receiver nodelist */
    orcm_logical_group_parse_array_string(argv[3], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        orcm_octl_error("nodelist-notfound");
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }

    /* pack the buffer to send */
    buf = OBJ_NEW(opal_buffer_t);
    if (NULL == buf) {
        orcm_octl_error("allocate-memory", "buffer");
        rc = ORCM_ERROR;
        goto done;
    }
    command = ORCM_GET_NOTIFIER_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_CMD_SERVER_T))) {
        error = PACKERR;
        goto done;
    }

    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_CMD_SERVER_T))) {
        error = PACKERR;
        goto done;
    }

    /* Loop through the nodelist to get notifier smtp policy */
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    if (NULL == xfer) {
        orcm_octl_error("allocate-memory", "buffer");
        rc = ORCM_ERROR;
        goto done;
    }
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
            orcm_octl_error("node-notfound");
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
            error = UNPACKERR;
            orcm_octl_error("unpack");
            goto done;
        }

        if ( ORCM_SUCCESS != response ) {
             if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &error,
                                                   &cnt, OPAL_STRING))) {
                 orcm_octl_error("unpack");
                 error = UNPACKERR;
                 goto done;
             }
             orcm_octl_error("notifier-get-smpt-policy", nodelist[i]);
             goto done;
        } else {
             cnt = 1;
             if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &count,
                                                      &cnt, OPAL_INT))) {
                 orcm_octl_error("unpack");
                 goto done;
             }

             if (0 == count) {
                 orcm_octl_info("smpt-header");
                 printf("%-10s    %6s    %10s \n", nodelist[i], "n/a", "n/a");
                 goto done;
             }
             opal_buffer_t *result_buf = NULL;
             if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data,  &result_buf,
                                               &cnt, OPAL_BUFFER))) {
                 orcm_octl_error("unpack");
                 error = UNPACKERR;
                 goto done;
             }
             orcm_octl_info("smpt-header");
             for(indx = 0; indx < count; indx++) {
                cnt = 1;
                if (OPAL_SUCCESS != (rc = opal_dss.unpack(result_buf,  &kv,
                                                  &cnt, OPAL_VALUE))) {
                    orcm_octl_error("unpack");
                    error = UNPACKERR;
                    goto done;
                }

                if(OPAL_INT == kv->type) {
                    printf("%-10s    %-10s    %-30d \n", nodelist[i], kv->key, kv->data.integer);
                } else if(OPAL_STRING == kv->type) {
                    printf("%-10s    %-10s    %-30s \n", nodelist[i], kv->key, kv->data.string);
                } else {
                    printf("%-10s    %-10s    %-30s \n", nodelist[i], "n/a", "n/a");
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
    if (kv) {
        OBJ_RELEASE(kv);
    }
    if (nodelist) {
        opal_argv_free(nodelist);
    }

    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_CMD_SERVER);
    return rc;
}


int orcm_octl_set_notifier_smtp(int cmd, char **argv)
{
    orcm_cmd_server_flag_t command;
    opal_buffer_t *buf = NULL;
    int rc = ORCM_SUCCESS;
    int result = ORCM_SUCCESS;
    int i = 0, cnt = 0;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;
    char *error = NULL;
    opal_value_t *kv = NULL;

    if (6 != opal_argv_count(argv)) {
        rc = ORCM_ERR_BAD_PARAM;
        orcm_octl_usage("notifier-set-smtp", INVALID_USG);
        return rc;
    }

    /* setup the receiver nodelist */
    orcm_logical_group_parse_array_string(argv[5], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        opal_argv_free(nodelist);
        nodelist = NULL;
        orcm_octl_error("nodelist-notfound");
        rc = ORCM_ERR_BAD_PARAM;
        goto done;
    }

    /* pack the buffer to send */
    buf = OBJ_NEW(opal_buffer_t);
    if (NULL == buf) {
        orcm_octl_error("allocate-memory", "buffer");
        rc = ORCM_ERROR;
        goto done;
    }

    command = ORCM_SET_NOTIFIER_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_CMD_SERVER_T))) {
        error = PACKERR;
        goto done;
    }

    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_CMD_SERVER_T))) {
        error = PACKERR;
        goto done;
    }

    kv = OBJ_NEW(opal_value_t);
    if (NULL == kv) {
        orcm_octl_error("allocate-memory", "buffer");
        rc = ORCM_ERROR;
        goto done;
    }

    kv->key = strdup(argv[3]);
    if (0 == strcasecmp(argv[3], "server_port") ||
        0 == strcasecmp(argv[3], "priority")) {
        kv->type = OPAL_INT;
        kv->data.integer = atoi(argv[4]);
        if (!kv->data.integer) {
            rc = ORCM_ERR_BAD_PARAM;
            orcm_octl_error("no-integer", argv[3]);
            goto done;
        }
    } else {
        kv->type = OPAL_STRING;
        kv->data.string = strdup(argv[4]);
    }
    /* pack notification severity */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &kv,
                                            1, OPAL_VALUE))) {
        orcm_octl_error("pack");
        goto done;
    }

    /* Loop through the nodelist to set notifier policy */
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    if (NULL == xfer) {
        orcm_octl_error("allocate-memory", "buffer");
        rc = ORCM_ERROR;
        goto done;
    }
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
            orcm_octl_error("node-notfound");
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
    if (kv) {
        OBJ_RELEASE(kv);
    }
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_CMD_SERVER);
    return rc;
}
