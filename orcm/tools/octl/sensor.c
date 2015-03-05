/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm/tools/octl/common.h"

int orcm_octl_sensor_sample_rate(char **argv)
{
    orcm_sensor_cmd_flag_t command = ORCM_SENSOR_SAMPLE_RATE_COMMAND;
    int sample_rate = 0;
    char *regex = 0;
    char *comp;
    opal_buffer_t *buf = NULL;
    int rc = ORCM_SUCCESS;
    int cnt, i, result;
    bool want_result = false;
    int numopts = 0;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;

    if (4 != opal_argv_count(argv)) {
        fprintf(stderr, "incorrect arguments to \"sensor sample-rate <value> <node regex>\"\n");
        return ORCM_ERR_BAD_PARAM;
    }

    if (isdigit(argv[2][strlen(argv[2]) - 1])) {
        sample_rate = (int)strtol(argv[2], NULL, 10);
    } else {
        fprintf(stderr, "incorrect argument sample rate is not an integer \n");
        return ORCM_ERR_BAD_PARAM;
    }

    orte_regex_extract_node_names (argv[3], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        fprintf(stdout, "Error: unable to extract nodelist\n");
        opal_argv_free(nodelist);
        return ORCM_ERR_BAD_PARAM;
    }

    buf = OBJ_NEW(opal_buffer_t);

    /* pack sensor sample rate command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SENSOR_CMD_T))) {
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

        fprintf(stdout, "ORCM setting sensor:sample-rate on Node:%s\n", nodelist[i]);
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
    goto finish;

finish:
    if(buf) OBJ_RELEASE(buf);
    if(xfer) OBJ_RELEASE(xfer);
    if(nodelist) opal_argv_free(nodelist);
    if (ORCM_SUCCESS != rc) {
        fprintf(stdout, "Error\n");
    }
    return rc;
}
