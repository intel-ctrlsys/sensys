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

int orcm_ocli_resource_status(char **argv)
{
    orcm_node_t **nodes;
    orte_rml_recv_cb_t xfer;
    opal_buffer_t *buf;
    int rc, i, n, num_nodes;
    orcm_scd_cmd_flag_t command;

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SCD,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);

    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_NODE_INFO_COMMAND;
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

    /* unpack number of nodes */
    ORTE_WAIT_FOR_COMPLETION(xfer.active);
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &num_nodes, &n, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    printf("\n********\nNODES (%i)\n********\n", num_nodes);
    if (0 < num_nodes) {
        nodes = (orcm_node_t**)malloc(num_nodes * sizeof(orcm_node_t*));
        /* get the sessions on the queue */
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, nodes, &num_nodes, ORCM_NODE))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        for (i = 0; i < num_nodes; i++) {
            printf("node: %s \n\tSCD_STATE:\t\"%s\" \n\tRM_STATE:\t\"%s\"\n\n",
                   nodes[i]->name,
                   orcm_scd_node_state_to_str(nodes[i]->scd_state),
                   orcm_node_state_to_str(nodes[i]->state));
            OBJ_DESTRUCT(nodes[i]);
        }
        free(nodes);
    }

    OBJ_DESTRUCT(&xfer);

    return ORTE_SUCCESS;
}

int orcm_ocli_resource_availability(char **argv)
{
    return ORCM_ERR_NOT_IMPLEMENTED;
}
