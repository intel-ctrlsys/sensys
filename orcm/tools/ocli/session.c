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

#include "orcm/tools/ocli/common.h"

#include "orcm/mca/scd/base/base.h"

int orcm_ocli_session_status(char **argv)
{
    return ORCM_ERR_NOT_IMPLEMENTED;
}

int orcm_ocli_session_cancel(char **argv)
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
