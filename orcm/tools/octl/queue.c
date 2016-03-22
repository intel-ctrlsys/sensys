/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/tools/octl/common.h"
#include "orcm/util/utils.h"

#include "orcm/mca/scd/base/base.h"


int orcm_octl_queue_status(char **argv)
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
    /* pack the session info command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, ORCM_SCD_CMD_T))) {
        OBJ_RELEASE(buf);
        orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SCD);
        return rc;
    }
    /* send it to the scheduler */
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                                                      ORCM_RML_TAG_SCD,
                                                      orte_rml_send_callback, NULL))) {
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SCD);
        return rc;
    }

    /* unpack number of queues */
    ORCM_WAIT_FOR_COMPLETION(xfer.active, ORCM_OCTL_WAIT_TIMEOUT, &rc);
    if (ORCM_SUCCESS != rc) {
        OBJ_DESTRUCT(&xfer);
        orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SCD);
        return rc;
    }
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &num_queues, &n, OPAL_INT))) {
        OBJ_DESTRUCT(&xfer);
        orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SCD);
        return rc;
    }

    ORCM_UTIL_MSG("********\nQUEUES\n********\n");

    /* for each queue */
    for (i = 0; i < num_queues; i++) {
        /* get the name */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &name, &n, OPAL_STRING))) {
            OBJ_DESTRUCT(&xfer);
            orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SCD);
            return rc;
        }
        /* get the number of sessions on the queue */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &num_sessions, &n, OPAL_INT))) {
            OBJ_DESTRUCT(&xfer);
            orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SCD);
            return rc;
        }

        ORCM_UTIL_MSG("%s (%i sessions)\n----------\n", name, num_sessions);

        if (0 < num_sessions) {
            allocs = (orcm_alloc_t**)malloc(num_sessions * sizeof(orcm_alloc_t*));
            if (NULL == allocs) {
                orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SCD);
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            /* get the sessions on the queue */
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, allocs, &num_sessions, ORCM_ALLOC))) {
                OBJ_DESTRUCT(&xfer);
                orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SCD);
                return rc;
            }
            /* loop through sessions, and print them */
            for (j = 0; j < num_sessions; j++) {
                ORCM_UTIL_MSG("%d\t%u|%u\t%i\t%s\t%s\t\"%s\"\n",
                       (int)allocs[j]->id,
                       allocs[j]->caller_uid,
                       allocs[j]->caller_gid,
                       allocs[j]->min_nodes,
                       allocs[j]->exclusive ? "EX" : "SH",
                       allocs[j]->interactive ? "I" : "B",
                       allocs[j]->notes ? allocs[j]->notes : "");
                OBJ_DESTRUCT(allocs[j]);
            }
            free(allocs);
        }
    }

    OBJ_DESTRUCT(&xfer);
    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SCD);

    return ORTE_SUCCESS;
}

int orcm_octl_queue_policy(char **argv)
{
    return ORCM_ERR_NOT_IMPLEMENTED;
}

int orcm_octl_queue_define(char **argv)
{
    return ORCM_ERR_NOT_IMPLEMENTED;
}

int orcm_octl_queue_add(char **argv)
{
    return ORCM_ERR_NOT_IMPLEMENTED;
}

int orcm_octl_queue_remove(char **argv)
{
    return ORCM_ERR_NOT_IMPLEMENTED;
}

int orcm_octl_queue_acl(char **argv)
{
    return ORCM_ERR_NOT_IMPLEMENTED;
}

int orcm_octl_queue_priority(char **argv)
{
    return ORCM_ERR_NOT_IMPLEMENTED;
}
