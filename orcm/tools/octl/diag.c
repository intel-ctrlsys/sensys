/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm/tools/octl/common.h"

int orcm_octl_diag_cpu(char **argv)
{
    orcm_diag_cmd_flag_t command = ORCM_DIAG_START_COMMAND;
    char *comp;
    opal_buffer_t *buf = NULL;
    int rc, cnt, i, result;
    bool want_result = false;
    int numopts = 0;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t xfer;
    char **nodelist = NULL;

    if (3 != opal_argv_count(argv)) {
        fprintf(stderr, "incorrect arguments to \"diag cpu\", expecting node regex\n");
        return ORCM_ERROR;
    }

    orte_regex_extract_node_names (argv[2], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        fprintf(stdout, "Error: unable to extract nodelist\n");
        return ORCM_ERR_BAD_PARAM;
    }

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_DIAG,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);

    buf = OBJ_NEW(opal_buffer_t);

    /* pack idag start command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_DIAG_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        goto fail;
    }
    /* pack component */
    comp = strdup("cpu");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &comp,
                                            1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        goto fail;
    }
    free(comp);
    /* pack if we want to wait for results */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &want_result,
                                            1, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        goto fail;
    }
    /* pack number of options to send */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &numopts,
                                            1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        goto fail;
    }
    /* pack options */
    /* -- */

    for (i = 0; i < opal_argv_count(nodelist); i++) {
        OBJ_RETAIN(buf);
        fprintf(stdout, "ORCM Executing Diag:cpu on Node:%s\n", nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            goto fail;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_DIAG,
                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            goto fail;
        }

        /* wait for status message */
        ORTE_WAIT_FOR_COMPLETION(xfer.active);

        if (want_result) {
            /* unpack results */
        } else {
            cnt=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &result,
                                                      &cnt, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_DESTRUCT(&xfer);
                return rc;
            }
            if (ORCM_SUCCESS == result) {
                fprintf(stdout, "Success\n");
            } else {
                fprintf(stdout, "Failure\n");
            }
        }
    }

    /* get the refcount correct */
    OBJ_RELEASE(buf);

    return ORCM_SUCCESS;

fail:
    fprintf(stdout, "Error\n");
    return rc;
}

int orcm_octl_diag_eth(char **argv)
{
    orcm_diag_cmd_flag_t command = ORCM_DIAG_START_COMMAND;
    char *comp;
    opal_buffer_t *buf = NULL;
    int rc, cnt, i, result;
    bool want_result = false;
    int numopts = 0;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t xfer;
    char **nodelist = NULL;

    if (3 != opal_argv_count(argv)) {
        fprintf(stderr, "incorrect arguments to \"diag eth\", expecting node regex\n");
        return ORCM_ERROR;
    }

    orte_regex_extract_node_names (argv[2], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        fprintf(stdout, "Error: unable to extract nodelist\n");
        return ORCM_ERR_BAD_PARAM;
    }

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_DIAG,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);

    buf = OBJ_NEW(opal_buffer_t);

    /* pack idag start command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_DIAG_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        goto fail;
    }
    /* pack component */
    comp = strdup("eth");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &comp,
                                            1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        goto fail;
    }
    free(comp);
    /* pack if we want to wait for results */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &want_result,
                                            1, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        goto fail;
    }
    /* pack number of options to send */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &numopts,
                                            1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        goto fail;
    }
    /* pack options */
    /* -- */

    for (i = 0; i < opal_argv_count(nodelist); i++) {
        OBJ_RETAIN(buf);
        fprintf(stdout, "ORCM Executing Diag:eth on Node:%s\n", nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            goto fail;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_DIAG,
                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            goto fail;
        }

        /* wait for status message */
        ORTE_WAIT_FOR_COMPLETION(xfer.active);

        if (want_result) {
            /* unpack results */
        } else {
            cnt=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &result,
                                                      &cnt, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_DESTRUCT(&xfer);
                return rc;
            }
            if (ORCM_SUCCESS == result) {
                fprintf(stdout, "Success\n");
            } else {
                fprintf(stdout, "Failure\n");
            }
        }
    }

    /* get the refcount correct */
    OBJ_RELEASE(buf);

    return ORCM_SUCCESS;

fail:
    fprintf(stdout, "Error\n");
    return rc;
}

int orcm_octl_diag_mem(char **argv)
{
    orcm_diag_cmd_flag_t command = ORCM_DIAG_START_COMMAND;
    char *comp;
    opal_buffer_t *buf = NULL;
    int rc, cnt, i, result;
    bool want_result = false;
    int numopts = 0;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t xfer;
    char **nodelist = NULL;

    if (3 != opal_argv_count(argv)) {
        fprintf(stderr, "incorrect arguments to \"diag mem\", expecting node regex\n");
        return ORCM_ERROR;
    }

    orte_regex_extract_node_names (argv[2], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        fprintf(stdout, "Error: unable to extract nodelist\n");
        return ORCM_ERR_BAD_PARAM;
    }

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_DIAG,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);

    buf = OBJ_NEW(opal_buffer_t);

    /* pack idag start command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_DIAG_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        goto fail;
    }
    /* pack component */
    comp = strdup("mem");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &comp,
                                            1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        goto fail;
    }
    free(comp);
    /* pack if we want to wait for results */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &want_result,
                                            1, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        goto fail;
    }
    /* pack number of options to send */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &numopts,
                                            1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        goto fail;
    }
    /* pack options */
    /* -- */

    for (i = 0; i < opal_argv_count(nodelist); i++) {
        OBJ_RETAIN(buf);
        fprintf(stdout, "ORCM Executing Diag:mem on Node:%s\n", nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            goto fail;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_DIAG,
                                          orte_rml_send_callback, NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            goto fail;
        }

        /* wait for status message */
        ORTE_WAIT_FOR_COMPLETION(xfer.active);

        if (want_result) {
            /* unpack results */
        } else {
            cnt=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &result,
                                                      &cnt, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_DESTRUCT(&xfer);
                return rc;
            }
            if (ORCM_SUCCESS == result) {
                fprintf(stdout, "Success\n");
            } else {
                fprintf(stdout, "Failure\n");
            }
        }
    }

    /* get the refcount correct */
    OBJ_RELEASE(buf);

    return ORCM_SUCCESS;

fail:
    fprintf(stdout, "Error\n");
    return rc;
}
