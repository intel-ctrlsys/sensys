/*
 * Copyright (c) 2004-2010 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006-2013 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007      Sun Microsystems, Inc.  All rights reserved.
 * Copyright (c) 2007      Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * Copyright (c) 2010      Oracle and/or its affiliates.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm/tools/octl/common.h"
#include "orcm/util/attr.h"
#include "orcm/mca/scd/base/base.h"

int orcm_octl_session_status(char **argv)
{
    return ORCM_ERR_NOT_IMPLEMENTED;
}

int orcm_octl_session_cancel(char **argv)
{
    orcm_scd_cmd_flag_t command;
    orcm_alloc_id_t id;
    opal_buffer_t *buf;
    orte_rml_recv_cb_t xfer;
    long session;
    int rc, n, result;
    
    if (3 != opal_argv_count(argv)) {
        fprintf(stderr, "incorrect arguments to \"session cancel\"\n");
        return ORCM_ERROR;
    }
    session = strtol(argv[2], NULL, 10);
    // FIXME: validate session id better
    if (session > 0) {
        /* setup to receive the result */
        OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
        xfer.active = true;
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                                ORCM_RML_TAG_SCD,
                                ORTE_RML_NON_PERSISTENT,
                                orte_rml_recv_callback, &xfer);
        
        /* send it to the scheduler */
        buf = OBJ_NEW(opal_buffer_t);
        
        command = ORCM_SESSION_CANCEL_COMMAND;
        /* pack the cancel command flag */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                                1, ORCM_SCD_CMD_T))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        
        id = (orcm_alloc_id_t)session;
        /* pack the session id */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &id, 1, ORCM_ALLOC_ID_T))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER,
                                                          buf,
                                                          ORCM_RML_TAG_SCD,
                                                          orte_rml_send_callback,
                                                          NULL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        /* get result */
        n=1;
        ORTE_WAIT_FOR_COMPLETION(xfer.active);
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &result,
                                                  &n, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        if (0 == result) {
            printf("Success\n");
        } else {
            printf("Failure\n");
        }
    } else {
        fprintf(stderr, "Invalid SESSION ID\n");
        return ORCM_ERROR;
    }
    
    return ORCM_SUCCESS;
}

int orcm_octl_session_set(int cmd, char **argv)
{
    orcm_scd_cmd_flag_t command;
    orcm_alloc_id_t id;
    opal_buffer_t *buf;
    orte_rml_recv_cb_t xfer;
    int rc, n, result, int_param;
    long session;
    double double_param;
    bool per_session = true;
    
    if (4 != opal_argv_count(argv)) {
        fprintf(stderr, "incorrect arguments to \"session set\"\n");
        return ORCM_ERROR;
    }

    session = strtol(argv[2], NULL, 10);

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SCD,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);
    
    /* send it to the scheduler */
    buf = OBJ_NEW(opal_buffer_t);
    
    command = ORCM_SET_POWER_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
      
    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    /* This is a session specific power setting, not global */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &per_session,
                                            1, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    id = (orcm_alloc_id_t)session;
    /* Pack the session ID */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &id,
                                            1, ORCM_ALLOC_ID_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    switch(cmd) {
    case ORCM_SET_POWER_BUDGET_COMMAND:
         double_param = (double)strtod(argv[3], NULL);
        // FIXME: validate that power budget is reasonable
            
        /* pack the power budget */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &double_param, 1, OPAL_DOUBLE))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_MODE_COMMAND:
        int_param = (int)strtol(argv[3], NULL, 10);
        // FIXME: validate that power mode is valid
            
        /* pack the power mode */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &int_param, 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_WINDOW_COMMAND:
        int_param = (int)strtol(argv[3], NULL, 10);
        // FIXME: validate that power window is reasonable
            
        /* pack the power window */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &int_param, 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_OVERAGE_COMMAND:
        double_param = (double)strtod(argv[3], NULL);
        // FIXME: validate that power overage is reasonable
            
        /* pack the power overage */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &double_param, 1, OPAL_DOUBLE))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_UNDERAGE_COMMAND:
        double_param = (double)strtod(argv[3], NULL);
        // FIXME: validate that power underage is reasonable
            
        /* pack the power underage */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &double_param, 1, OPAL_DOUBLE))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_OVERAGE_TIME_COMMAND:
        int_param = (int)strtol(argv[3], NULL, 10);
        // FIXME: validate that power overage time is reasonable
            
        /* pack the power overage time */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &int_param, 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_UNDERAGE_TIME_COMMAND:
        int_param = (int)strtol(argv[3], NULL, 10);
        // FIXME: validate that power underage time is reasonable
            
        /* pack the power underage time */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &int_param, 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_FREQUENCY_COMMAND:
        double_param = (double)strtod(argv[3], NULL);
        // FIXME: validate that power freq is reasonable
            
        /* pack the power freq */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &double_param, 1, OPAL_DOUBLE))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    default:
        printf("Illegal power command: %d\n", cmd);
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return ORTE_ERR_BAD_PARAM;
    }

    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER,
                                                      buf,
                                                      ORCM_RML_TAG_SCD,
                                                      orte_rml_send_callback,
                                                      NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
    /* get result */
    n=1;
    ORTE_WAIT_FOR_COMPLETION(xfer.active);
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &result,
                                              &n, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
    if (0 == result) {
        printf("Success\n");
    } else {
        printf("Failure\n");
    }
    
    return ORCM_SUCCESS;
}

int orcm_octl_session_get(int cmd, char **argv)
{
    orcm_scd_cmd_flag_t command;
    opal_buffer_t *buf;
    orte_rml_recv_cb_t xfer;
    int rc, n, int_param;
    double double_param;
    long session;
    orcm_alloc_id_t id;
    bool per_session = true;
    
    if (3 != opal_argv_count(argv)) {
        fprintf(stderr, "incorrect arguments to \"session get\"\n");
        return ORCM_ERROR;
    }

    session = strtol(argv[2], NULL, 10);

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SCD,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);
    
    /* send it to the scheduler */
    buf = OBJ_NEW(opal_buffer_t);
    
    command = ORCM_GET_POWER_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
    
    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    /* This is a session specific power setting, not global */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &per_session,
                                            1, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    id = (orcm_alloc_id_t)session;
    /* Pack the session ID */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &id,
                                            1, ORCM_ALLOC_ID_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER,
                                                      buf,
                                                      ORCM_RML_TAG_SCD,
                                                      orte_rml_send_callback,
                                                      NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    /* get result */
    n=1;
    ORTE_WAIT_FOR_COMPLETION(xfer.active);

    switch(cmd) {
    case ORCM_GET_POWER_BUDGET_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &double_param,
                                                  &n, OPAL_DOUBLE))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Session %ld current cluster power budget: %f watts\n", session, double_param);
    break;
    case ORCM_GET_POWER_MODE_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &int_param,
                                                  &n, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Session %ld current default power mode: %s\n", session, orcm_attr_key_print(int_param));
    break;
    case ORCM_GET_POWER_WINDOW_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &int_param,
                                                  &n, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Session %ld current default power window: %d ms\n", session, int_param);
    break;
    case ORCM_GET_POWER_OVERAGE_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &double_param,
                                                  &n, OPAL_DOUBLE))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Session %ld c default power budget overage limit: %f watts\n", session, double_param);
    break;
    case ORCM_GET_POWER_UNDERAGE_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &double_param,
                                                  &n, OPAL_DOUBLE))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Session %ld current default power budget underage limit: %f watts\n", session, double_param);
    break;
    case ORCM_GET_POWER_OVERAGE_TIME_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &int_param,
                                                  &n, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Session %ld current default power overage time limit: %d ms\n", session, int_param);
    break;
    case ORCM_GET_POWER_UNDERAGE_TIME_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &int_param,
                                                  &n, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Session %ld current default power underage time limit: %d ms\n", session, int_param);
    break;
    case ORCM_GET_POWER_FREQUENCY_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &double_param,
                                                  &n, OPAL_DOUBLE))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Session %ld current default power frequency: %f MHz\n", session, double_param);
    break;
    default:
        printf("Illegal power command: %d\n", cmd);
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return ORTE_ERR_BAD_PARAM;
    }

    return ORCM_SUCCESS;
}

