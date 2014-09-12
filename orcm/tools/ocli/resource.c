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

#include "orte/util/regex.h"

#include "orcm/tools/ocli/common.h"

#include "orcm/mca/scd/base/base.h"

/* create a container for resource states */
typedef struct {
    opal_list_item_t super;
    orcm_node_t template;
    char** resources;
} orcm_resource_container_t;
static void resourcecon(orcm_resource_container_t *p)
{
    OBJ_CONSTRUCT(&p->template, orcm_node_t);
    p->resources = NULL;
}
static void resourcedes(orcm_resource_container_t *p)
{
    OBJ_DESTRUCT(&p->template);
    if (NULL != p->resources) {
        opal_argv_free(p->resources);
    }
}
OBJ_CLASS_INSTANCE(orcm_resource_container_t,
                   opal_list_item_t,
                   resourcecon, resourcedes);

static opal_list_t containers;
static bool inited = false;

int orcm_ocli_resource_status(char **argv)
{
    orcm_node_t **nodes;
    orcm_resource_container_t *container;
    char *regexp;
    char *nodelist;
    orte_rml_recv_cb_t xfer;
    opal_buffer_t *buf;
    int rc, i, n, num_nodes;
    orcm_scd_cmd_flag_t command;
    bool found;

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SCD,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);

    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_NODE_INFO_COMMAND;
    /* pack the node info command flag */
    if (OPAL_SUCCESS !=
        (rc = opal_dss.pack(buf, &command, 1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    /* send it to the scheduler */
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

    /* unpack number of nodes */
    ORTE_WAIT_FOR_COMPLETION(xfer.active);
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &num_nodes,
                                              &n, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    printf("\nTOTAL NODES : %i\n", num_nodes);
    if (0 < num_nodes) {
        if (!inited) {
            OBJ_CONSTRUCT(&containers, opal_list_t);
            inited = true;
        }
        nodes = (orcm_node_t**)malloc(num_nodes * sizeof(orcm_node_t*));
        /* get the sessions on the queue */
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, nodes,
                                                  &num_nodes, ORCM_NODE))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        /* We want to combine all the nodes with the same attributes,
         * so we containerize them based on their attributes
         * the template of the container is a nameless node object with all
         * the attributes of the nodes in that container
         * if we match a template, put the node in that conatiner
         * otherwise create a new container with a template that matches the
         * node we are trying to insert */
        for (i = 0; i < num_nodes; i++) {
            found = false;
            OPAL_LIST_FOREACH(container, &containers, orcm_resource_container_t) {
                if (container->template.scd_state == nodes[i]->scd_state &&
                    container->template.state == nodes[i]->state) {
                    opal_argv_append_nosize(&container->resources,
                                            nodes[i]->name);
                    found = true;
                }
            }
            if (false == found) {
                container = OBJ_NEW(orcm_resource_container_t);
                container->template.scd_state = nodes[i]->scd_state;
                container->template.state = nodes[i]->state;
                opal_argv_append_nosize(&container->resources, nodes[i]->name);
                opal_list_append(&containers, &container->super);
            }
        }
        printf("NODES                : STATE  SCHED_STATE\n");
        printf("-----------------------------------------\n");
        /* print out nodes by containter
         * since they all have the same attributes in the container,
         * we can combine the node list by using regex and list this unique
         * combination of attributes just once 
         * may need to sort at some point, but for now scheduler gives us
         * nodes in sorted order */
        OPAL_LIST_FOREACH(container, &containers, orcm_resource_container_t) {
            nodelist = opal_argv_join(container->resources, ',');
            if (ORTE_SUCCESS != (rc = orte_regex_create(nodelist, &regexp))) {
                ORTE_ERROR_LOG(rc);
                OPAL_LIST_DESTRUCT(&containers);
                OBJ_DESTRUCT(&xfer);
                return rc;
            }
            if (21 > strnlen(regexp, 21)) {
                printf("%-20s : %s %16s\n",
                       regexp,
                       orcm_node_state_to_char(container->template.state),
                       orcm_scd_node_state_to_str(container->template.scd_state));
            } else {
                printf("%s\n                     : %s %16s\n",
                       regexp,
                       orcm_node_state_to_char(container->template.state),
                       orcm_scd_node_state_to_str(container->template.scd_state));
            }
            free(nodelist);
            free(regexp);
        }
        OPAL_LIST_DESTRUCT(&containers);
        inited = false;
    }

    OBJ_DESTRUCT(&xfer);

    return ORCM_SUCCESS;
}

int orcm_ocli_resource_availability(char **argv)
{
    return ORCM_ERR_NOT_IMPLEMENTED;
}
