/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm/tools/ocli/common.h"

#include "orcm/mca/scd/base/base.h"

int orcm_ocli_queue_status(char **argv)
{
    orcm_alloc_t **allocs;
    orte_rml_recv_cb_t xfer;
    opal_buffer_t *buf;
    int rc, i, j, n, num_queues, num_sessions;
    orcm_scd_cmd_flag_t command;
    char *name;

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SCD,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);

    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_SESSION_INFO_COMMAND;
    /* pack the alloc command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    /* send it to the scheduler */
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                                                      ORCM_RML_TAG_SCD,
                                                      orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    /* unpack number of queues */
    ORTE_WAIT_FOR_COMPLETION(xfer.active);
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &num_queues, &n, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    fprintf(stdout, "********\nQUEUES\n********\n");

    /* for each queue */
    for (i = 0; i < num_queues; i++) {
        /* get the name */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &name, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        /* get the number of sessions on the queue */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &num_sessions, &n, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }

        fprintf(stdout,"%s (%i sessions)\n----------\n", name, num_sessions);
        
        if (0 < num_sessions) {
            allocs = (orcm_alloc_t**)malloc(num_sessions * sizeof(orcm_alloc_t*));
            if (NULL == allocs) {
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            /* get the sessions on the queue */
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, allocs, &num_sessions, ORCM_ALLOC))) {
                ORTE_ERROR_LOG(rc);
                OBJ_DESTRUCT(&xfer);
                return rc;
            }
            for (j = 0; j < num_sessions; j++) {
                fprintf(stdout, "%d\t%u|%u\t%i\t%s\t%s\n", 
                        (int)allocs[j]->id, 
                        allocs[j]->caller_uid, 
                        allocs[j]->caller_gid, 
                        allocs[j]->min_nodes, 
                        allocs[j]->exclusive ? "EX" : "SH",
                        allocs[j]->interactive ? "I" : "B" );
                OBJ_DESTRUCT(allocs[j]);
            }
            free(allocs);
        }
    }

    OBJ_DESTRUCT(&xfer);

    return ORTE_SUCCESS;
}

int orcm_ocli_queue_policy(char **argv)
{
    return ORCM_ERR_NOT_IMPLEMENTED;
}
