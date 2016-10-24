/*
 * Copyright (c) 2014-2016  Intel Corporation.  All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/tools/octl/common.h"
#include "orcm/util/utils.h"
#include "orcm/util/logical_group.h"

int orcm_octl_diag_cpu(char **argv)
{
    orcm_diag_cmd_flag_t command = ORCM_DIAG_START_COMMAND;
    char *comp;
    opal_buffer_t *buf = NULL;
    int rc = ORCM_SUCCESS, cnt, i, result;
    bool want_result = false;
    int numopts = 0;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;


    if (3 != opal_argv_count(argv)) {
        orcm_octl_usage("diag", INVALID_USG);
        return ORCM_ERR_BAD_PARAM;
    }

    orcm_logical_group_parse_array_string(argv[2], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        orcm_octl_error("nodelist-extract", argv[2]);
        opal_argv_free(nodelist);
        return ORCM_ERR_BAD_PARAM;
    }

    buf = OBJ_NEW(opal_buffer_t);

    /* pack idag start command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_DIAG_CMD_T))) {
        orcm_octl_error("pack");
        goto finish;
    }
    /* pack component */
    comp = strdup("cputest");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &comp,
                                            1, OPAL_STRING))) {
        orcm_octl_error("pack");
        goto finish;
    }
    free(comp);
    /* pack if we want to wait for results */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &want_result,
                                            1, OPAL_BOOL))) {
        orcm_octl_error("pack");
        goto finish;
    }
    /* pack number of options to send */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &numopts,
                                            1, OPAL_INT))) {
        orcm_octl_error("pack");
        goto finish;
    }
    /* pack options */
    /* -- */

    /* setup to receive the result */
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    for (i = 0; i < opal_argv_count(nodelist); i++) {
        OBJ_RETAIN(buf);
        OBJ_RETAIN(xfer);

        xfer->active = true;
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                                ORCM_RML_TAG_DIAG,
                                ORTE_RML_NON_PERSISTENT,
                                orte_rml_recv_callback, xfer);

        orcm_octl_info("executing-diag", "cpu", nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            goto finish;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_DIAG,
                                          orte_rml_send_callback, NULL))) {
            goto finish;
        }

        /* wait for status message */
        ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_error("connection-fail");
            goto finish;
        }

        if (want_result) {
            /* unpack results */
        } else {
            cnt=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &result,
                                                      &cnt, OPAL_INT))) {
                orcm_octl_error("unpack");
                goto finish;
            }
            if (ORCM_SUCCESS == result) {
                orcm_octl_info("success");
            }
        }
    }
    goto finish;

finish:
    SAFE_RELEASE(buf);
    SAFE_RELEASE(xfer);
    if(nodelist) opal_argv_free(nodelist);
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_DIAG);
    return rc;
}

int orcm_octl_diag_eth(char **argv)
{
    orcm_diag_cmd_flag_t command = ORCM_DIAG_START_COMMAND;
    char *comp;
    opal_buffer_t *buf = NULL;
    int rc = ORCM_SUCCESS, cnt, i, result;
    bool want_result = false;
    int numopts = 0;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;

    if (3 != opal_argv_count(argv)) {
        orcm_octl_usage("diag", INVALID_USG);
        return ORCM_ERR_BAD_PARAM;
    }

    orcm_logical_group_parse_array_string(argv[2], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        orcm_octl_error("nodelist-extract", argv[2]);
        opal_argv_free(nodelist);
        return ORCM_ERR_BAD_PARAM;
    }

    buf = OBJ_NEW(opal_buffer_t);

    /* pack idag start command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_DIAG_CMD_T))) {
        goto finish;
    }
    /* pack component */
    comp = strdup("ethtest");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &comp,
                                            1, OPAL_STRING))) {
        orcm_octl_error("pack");
        goto finish;
    }
    free(comp);
    /* pack if we want to wait for results */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &want_result,
                                            1, OPAL_BOOL))) {
        orcm_octl_error("pack");
        goto finish;
    }
    /* pack number of options to send */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &numopts,
                                            1, OPAL_INT))) {
        orcm_octl_error("pack");
        goto finish;
    }
    /* pack options */
    /* -- */

    /* setup to receive the result */
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    for (i = 0; i < opal_argv_count(nodelist); i++) {
        OBJ_RETAIN(buf);
        OBJ_RETAIN(xfer);

        xfer->active = true;
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                                ORCM_RML_TAG_DIAG,
                                ORTE_RML_NON_PERSISTENT,
                                orte_rml_recv_callback, xfer);

        orcm_octl_info("executing-diag", "eth", nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            orcm_octl_error("node-notfound", nodelist[i]);
            goto finish;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_DIAG,
                                          orte_rml_send_callback, NULL))) {
            orcm_octl_error("connection-fail");
            goto finish;
        }

        /* wait for status message */
        ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_error("connection-fail");
            goto finish;
        }

        if (want_result) {
            /* unpack results */
        } else {
            cnt=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &result,
                                                      &cnt, OPAL_INT))) {
                orcm_octl_error("unpack");
                goto finish;
            }
            if (ORCM_SUCCESS == result) {
                orcm_octl_info("success");
            }
        }
    }

    /* get the refcount correct */
    OBJ_RELEASE(buf);

    opal_argv_free(nodelist);
    return ORCM_SUCCESS;

finish:
    SAFE_RELEASE(buf);
    SAFE_RELEASE(xfer);
    if(nodelist) opal_argv_free(nodelist);
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_DIAG);
    return rc;
}

int orcm_octl_diag_mem(char **argv)
{
    orcm_diag_cmd_flag_t command = ORCM_DIAG_START_COMMAND;
    char *comp;
    opal_buffer_t *buf = NULL;
    int rc = ORCM_SUCCESS, cnt, i, result;
    bool want_result = false;
    int numopts = 0;
    orte_process_name_t tgt;
    orte_rml_recv_cb_t *xfer = NULL;
    char **nodelist = NULL;

    if (3 != opal_argv_count(argv)) {
        orcm_octl_usage("diag", INVALID_USG);
        return ORCM_ERR_BAD_PARAM;
    }

    orcm_logical_group_parse_array_string(argv[2], &nodelist);
    if (0 == opal_argv_count(nodelist)) {
        orcm_octl_error("nodelist-extract", argv[2]);
        opal_argv_free(nodelist);
        return ORCM_ERR_BAD_PARAM;
    }

    buf = OBJ_NEW(opal_buffer_t);

    /* pack idag start command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_DIAG_CMD_T))) {
        orcm_octl_error("pack");
        goto finish;
    }
    /* pack component */
    comp = strdup("memtest");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &comp,
                                            1, OPAL_STRING))) {
        orcm_octl_error("pack");
        goto finish;
    }
    free(comp);
    /* pack if we want to wait for results */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &want_result,
                                            1, OPAL_BOOL))) {
        orcm_octl_error("pack");
        goto finish;
    }
    /* pack number of options to send */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &numopts,
                                            1, OPAL_INT))) {
        orcm_octl_error("pack");
        goto finish;
    }
    /* pack options */
    /* -- */

    /* setup to receive the result */
    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    for (i = 0; i < opal_argv_count(nodelist); i++) {
        OBJ_RETAIN(buf);
        OBJ_RETAIN(xfer);

        xfer->active = true;
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                                ORCM_RML_TAG_DIAG,
                                ORTE_RML_NON_PERSISTENT,
                                orte_rml_recv_callback, xfer);

        orcm_octl_info("executing-diag", "mem", nodelist[i]);
        if (ORCM_SUCCESS != (rc = orcm_cfgi_base_get_hostname_proc(nodelist[i],
                                                                   &tgt))) {
            orcm_octl_error("node-notfound", nodelist[i]);
            goto finish;
        }

        /* send command to node daemon */
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(&tgt, buf,
                                          ORCM_RML_TAG_DIAG,
                                          orte_rml_send_callback, NULL))) {
            orcm_octl_error("connection-fail");
            goto finish;
        }

        /* wait for status message */
        ORCM_WAIT_FOR_COMPLETION(xfer->active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
        if (ORCM_SUCCESS != rc) {
            orcm_octl_error("connection-fail");
            goto finish;
        }

        if (want_result) {
            /* unpack results */
        } else {
            cnt=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer->data, &result,
                                                      &cnt, OPAL_INT))) {
                orcm_octl_error("unpack");
                goto finish;
            }
            if (ORCM_SUCCESS == result) {
                orcm_octl_info("success");
            } else {
                orcm_octl_error("diag-mem");
            }
        }
    }
    goto finish;

finish:
    SAFE_RELEASE(buf);
    SAFE_RELEASE(xfer);
    if(nodelist) opal_argv_free(nodelist);
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_DIAG);
    return rc;
}
