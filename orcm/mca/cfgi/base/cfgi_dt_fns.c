/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/types.h"

#include <sys/types.h>

#include "opal_stdint.h"
#include "opal/util/argv.h"
#include "opal/dss/dss.h"
#include "opal/dss/dss_internal.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/cfgi/base/base.h"


/*****    PACKING ROUTINES     ******/
static int pack_config(orcm_config_t *config, opal_buffer_t *buf)
{
    int ret;
    int32_t i, k;

    k = opal_argv_count(config->mca_params);
    if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buf, &k, 1, OPAL_INT32))) {
        ORTE_ERROR_LOG(ret);
        return ret;
    }
    if (0 < k) {
        for (i=0; i < k; i++) {
            if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buf, &config->mca_params[i], 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
        }
    }
    k = opal_argv_count(config->env);
    if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buf, &k, 1, OPAL_INT32))) {
        ORTE_ERROR_LOG(ret);
        return ret;
    }
    if (0 < k) {
        for (i=0; i < k; i++) {
            if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buf, &config->env[i], 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
        }
    }
    if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buf, &config->port, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        return ret;
    }
    if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buf, &config->aggregator, 1, OPAL_BOOL))) {
        ORTE_ERROR_LOG(ret);
        return ret;
    }
    return ORCM_SUCCESS;
}

int orcm_pack_node(opal_buffer_t *buffer, const void *src,
                   int32_t num_vals, opal_data_type_t type)
{
    int ret = OPAL_SUCCESS;
    orcm_node_t **nodes, *node;
    int i;

    /* array of pointers to orcm_node_t objects - need
     * to pack the objects a set of fields at a time
     */
    nodes = (orcm_node_t**)src;

    for (i=0; i < num_vals; i++) {
        node = nodes[i];
        /* pack the name */
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, &node->name, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the daemon name */
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&node->daemon, 1, ORTE_NAME))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the config */
        if (OPAL_SUCCESS != (ret = pack_config(&node->config, buffer))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the node rm state */
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&node->state, 1, OPAL_INT8))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the node scd state */
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&node->scd_state, 1, OPAL_INT8))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
    }

    return ret;
}

int orcm_pack_rack(opal_buffer_t *buffer, const void *src,
                   int32_t num_vals, opal_data_type_t type)
{
    int ret = OPAL_SUCCESS;
    orcm_rack_t **racks, *rack;
    orcm_node_t *controller;
    int i;
    int32_t j;
    orcm_node_t *node;

    /* array of pointers to orcm_rack_t objects - need
     * to pack the objects a set of fields at a time
     */
    racks = (orcm_rack_t**)src;

    for (i=0; i < num_vals; i++) {
        rack = racks[i];
        /* pack the name */
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, &rack->name, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the controller */
        controller = &rack->controller;
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&controller, 1, ORCM_NODE))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the nodes in the rack */
        j = opal_list_get_size(&rack->nodes);
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&j, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        OPAL_LIST_FOREACH(node, &rack->nodes, orcm_node_t) {
            if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&node, 1, ORCM_NODE))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
        }
    }

    return ret;
}

int orcm_pack_row(opal_buffer_t *buffer, const void *src,
                   int32_t num_vals, opal_data_type_t type)
{
    int ret = OPAL_SUCCESS;
    orcm_row_t **rows, *row;
    orcm_node_t *controller;
    int i;
    int32_t j;
    orcm_rack_t *rack;

    /* array of pointers to orcm_row_t objects - need
     * to pack the objects a set of fields at a time
     */
    rows = (orcm_row_t**)src;

    for (i=0; i < num_vals; i++) {
        row = rows[i];
        /* pack the name */
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, &row->name, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the controller */
        controller = &row->controller;
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&controller, 1, ORCM_NODE))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the racks in the row */
        j = opal_list_get_size(&row->racks);
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&j, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
            if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&rack, 1, ORCM_RACK))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
        }
    }

    return ret;
}

int orcm_pack_cluster(opal_buffer_t *buffer, const void *src,
                   int32_t num_vals, opal_data_type_t type)
{
    int ret = OPAL_SUCCESS;
    orcm_cluster_t **clusters, *cluster;
    orcm_node_t *controller;
    int i;
    int32_t j;
    orcm_row_t *row;

    /* array of pointers to orcm_cluster_t objects - need
     * to pack the objects a set of fields at a time
     */
    clusters = (orcm_cluster_t**)src;

    for (i=0; i < num_vals; i++) {
        cluster = clusters[i];
        /* pack the name */
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, &cluster->name, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the controller */
        controller = &cluster->controller;
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&controller, 1, ORCM_NODE))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the rows in the cluster */
        j = opal_list_get_size(&cluster->rows);
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&j, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        OPAL_LIST_FOREACH(row, &cluster->rows, orcm_row_t) {
            if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&row, 1, ORCM_ROW))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
        }
    }

    return ret;
}

int orcm_pack_scheduler(opal_buffer_t *buffer, const void *src,
                   int32_t num_vals, opal_data_type_t type)
{
    int ret = OPAL_SUCCESS;
    orcm_scheduler_t **schedulers, *scheduler;
    orcm_node_t *controller;
    int i;
    int32_t j, k;

    /* array of pointers to orcm_scheduler_t objects - need
     * to pack the objects a set of fields at a time
     */
    schedulers = (orcm_scheduler_t**)src;

    for (i=0; i < num_vals; i++) {
        scheduler = schedulers[i];
        /* pack the controller */
        controller = &scheduler->controller;
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&controller, 1, ORCM_NODE))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the queues */
        j = opal_argv_count(scheduler->queues);
        if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&j, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        for (k=0; k < j; k++) {
            if (OPAL_SUCCESS != (ret = opal_dss_pack_buffer(buffer, (void*)&scheduler->queues[k], 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
        }
    }

    return ret;
}


/*****    UNPACKING ROUTINES     ******/
static int unpack_config(orcm_config_t *config, opal_buffer_t *buf)
{
    int ret;
    int32_t i, k, n;
    char *tmp;

    n=1;
    if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buf, &k, &n, OPAL_INT32))) {
        ORTE_ERROR_LOG(ret);
        return ret;
    }
    if (0 < k) {
        for (i=0; i < k; i++) {
            n=1;
            if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buf, &tmp, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
            opal_argv_append_nosize(&config->mca_params, tmp);
            free(tmp);
        }
    }
    n=1;
    if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buf, &k, &n, OPAL_INT32))) {
        ORTE_ERROR_LOG(ret);
        return ret;
    }
    if (0 < k) {
        for (i=0; i < k; i++) {
            n=1;
            if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buf, &tmp, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
            opal_argv_append_nosize(&config->env, tmp);
            free(tmp);
        }
    }
    n=1;
    if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buf, &config->port, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        return ret;
    }
    n=1;
    if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buf, &config->aggregator, &n, OPAL_BOOL))) {
        ORTE_ERROR_LOG(ret);
        return ret;
    }
    return ORCM_SUCCESS;
}


int orcm_unpack_node(opal_buffer_t *buffer, void *dest,
                     int32_t *num_vals, opal_data_type_t type)
{
    int ret;
    int32_t i, n;
    orcm_node_t **nodes, *node;

    /* unpack into array of orcm_node_t objects */
    nodes = (orcm_node_t**) dest;
    for (i=0; i < *num_vals; i++) {

        /* create the orcm_node_t object */
        nodes[i] = OBJ_NEW(orcm_node_t);
        if (NULL == nodes[i]) {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            return ORTE_ERR_OUT_OF_RESOURCE;
        }
        node = nodes[i];

        /* unpack the name */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &node->name, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the daemon name */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &node->daemon, &n, ORTE_NAME))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the config */
        n=1;
        if (OPAL_SUCCESS != (ret = unpack_config(&node->config, buffer))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the node rm state */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &node->state, &n, OPAL_INT8))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the node scd state */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &node->scd_state, &n, OPAL_INT8))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
    }

    return ORTE_SUCCESS;
}

int orcm_unpack_rack(opal_buffer_t *buffer, void *dest,
                     int32_t *num_vals, opal_data_type_t type)
{
    int ret;
    int32_t i, n;
    orcm_rack_t **racks, *rack;
    int32_t j, k;
    orcm_node_t *node;

    /* unpack into array of orcm_rack_t objects */
    racks = (orcm_rack_t**) dest;
    for (i=0; i < *num_vals; i++) {

        /* create the orcm_rack_t object */
        racks[i] = OBJ_NEW(orcm_rack_t);
        if (NULL == racks[i]) {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            return ORTE_ERR_OUT_OF_RESOURCE;
        }
        rack = racks[i];

        /* unpack the name */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &rack->name, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the controller */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &rack->controller, &n, ORCM_NODE))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the nodes */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &j, &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        for (k=0; k < j; k++) {
            n=1;
            if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &node, &n, ORCM_NODE))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
            opal_list_append(&rack->nodes, &node->super);
        }
    }

    return ORTE_SUCCESS;
}

int orcm_unpack_row(opal_buffer_t *buffer, void *dest,
                    int32_t *num_vals, opal_data_type_t type)
{
    int ret;
    int32_t i, n;
    orcm_row_t **rows, *row;
    int32_t j, k;
    orcm_rack_t *rack;

    /* unpack into array of orcm_row_t objects */
    rows = (orcm_row_t**) dest;
    for (i=0; i < *num_vals; i++) {

        /* create the orcm_row_t object */
        rows[i] = OBJ_NEW(orcm_row_t);
        if (NULL == rows[i]) {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            return ORTE_ERR_OUT_OF_RESOURCE;
        }
        row = rows[i];

        /* unpack the name */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &row->name, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the controller */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &row->controller, &n, ORCM_NODE))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the racks */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &j, &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        for (k=0; k < j; k++) {
            n=1;
            if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &rack, &n, ORCM_RACK))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
            opal_list_append(&row->racks, &rack->super);
        }
    }

    return ORTE_SUCCESS;
}

int orcm_unpack_cluster(opal_buffer_t *buffer, void *dest,
                        int32_t *num_vals, opal_data_type_t type)
{
    int ret;
    int32_t i, n;
    orcm_cluster_t **clusters, *cluster;
    orcm_node_t *controller;
    int32_t j, k;
    orcm_row_t *row;

    /* unpack into array of orcm_cluster_t objects */
    clusters = (orcm_cluster_t**) dest;
    for (i=0; i < *num_vals; i++) {

        /* create the orcm_cluster_t object */
        clusters[i] = OBJ_NEW(orcm_cluster_t);
        if (NULL == clusters[i]) {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            return ORTE_ERR_OUT_OF_RESOURCE;
        }
        cluster = clusters[i];

        /* unpack the name */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &cluster->name, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the controller */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, controller, &n, ORCM_NODE))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }

        &cluster->controller = *controller;

        /* unpack the rows */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &j, &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        for (k=0; k < j; k++) {
            n=1;
            if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &row, &n, ORCM_ROW))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
            opal_list_append(&cluster->rows, &row->super);
        }
    }

    return ORTE_SUCCESS;
}

int orcm_unpack_scheduler(opal_buffer_t *buffer, void *dest,
                          int32_t *num_vals, opal_data_type_t type)
{
    int ret;
    int32_t i, n;
    orcm_scheduler_t **schedulers;
    orcm_scheduler_t *scheduler = NULL;
    orcm_node_t *controller;
    int32_t j, k;
    char *tmp;

    /* unpack into array of orcm_scheduler_t objects */
    schedulers = (orcm_scheduler_t**) dest;
    for (i=0; i < *num_vals; i++) {

        /* create the orcm_scheduler_t object */
        schedulers[i] = OBJ_NEW(orcm_scheduler_t);
        if (NULL == schedulers[i]) {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            return ORTE_ERR_OUT_OF_RESOURCE;
        }
        scheduler = schedulers[i];

    }
    if (scheduler) {
        /* unpack the controller */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &controller, &n, ORCM_NODE))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }

        scheduler->controller = *controller;

        /* unpack the queues */
        n=1;
        if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &k, &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        if (0 < k) {
            for (j=0; j < k; j++) {
                n=1;
                if (OPAL_SUCCESS != (ret = opal_dss_unpack_buffer(buffer, &tmp, &n, OPAL_STRING))) {
                    ORTE_ERROR_LOG(ret);
                    return ret;
                }
                opal_argv_append_nosize(&scheduler->queues, tmp);
                free(tmp);
            }
        }
    } else {
        /* FIXME: better return value? or other error handling? */
        return ORTE_ERR_OUT_OF_RESOURCE;
    }

    return ORTE_SUCCESS;
}


/*****     COMPARE ROUTINES    *****/
int orcm_compare_node(orcm_node_t *value1, orcm_node_t *value2, opal_data_type_t type)
{
    int rc;
    int size1;
    int size2;
    char *string_comp1;
    char *string_comp2;

    if (NULL == value1 || NULL == value2) {
        if (NULL == value1 && NULL == value2) {
            return OPAL_EQUAL;
        } else if (NULL == value1 && NULL != value2) {
            return OPAL_VALUE2_GREATER;
        } else { /* NULL != value1 && NULL == VALUE2 */
            return OPAL_VALUE1_GREATER;
        }
    }

    if (NULL == value1->name || NULL == value2->name) {
        if (NULL != value1->name && NULL == value2->name) {
            return OPAL_VALUE1_GREATER;
        } else if (NULL == value1->name && NULL != value2->name) {
            return OPAL_VALUE2_GREATER;
        }
    } else {
        rc = strcmp(value1->name, value2->name);
        if (0 < rc) {
            return OPAL_VALUE1_GREATER;
        } else if (0 > rc) {
            return OPAL_VALUE2_GREATER;
        }
    }

    if (value1->daemon.jobid > value2->daemon.jobid) {
        return OPAL_VALUE1_GREATER;
    } else if (value1->daemon.jobid < value2->daemon.jobid) {
        return OPAL_VALUE2_GREATER;
    }

    if (value1->daemon.vpid > value2->daemon.vpid) {
        return OPAL_VALUE1_GREATER;
    } else if (value1->daemon.vpid < value2->daemon.vpid) {
        return OPAL_VALUE2_GREATER;
    }

    if (NULL == value1->config.mca_params || NULL == value2->config.mca_params) {
        if (NULL != value1->config.mca_params && NULL == value2->config.mca_params) {
            return OPAL_VALUE1_GREATER;
        } else if (NULL == value1->config.mca_params && NULL != value2->config.mca_params) {
            return OPAL_VALUE2_GREATER;
        }
    } else {
        size1 = opal_argv_count(value1->config.mca_params);
        size2 = opal_argv_count(value2->config.mca_params);

        if (size1 > size2) {
            return OPAL_VALUE1_GREATER;
        } else if (size1 < size2) {
            return OPAL_VALUE2_GREATER;
        }

        /* Compare the full "argv" list of mca_params. Must be terminated with a NULL "sentinel" element */
        string_comp1 = opal_argv_join(value1->config.mca_params, 0);
        string_comp2 = opal_argv_join(value2->config.mca_params, 0);

        rc = strcmp(string_comp1, string_comp2);

        free(string_comp1);
        free(string_comp2);
        if (0 < rc) {
            return OPAL_VALUE1_GREATER;
        } else if (0 > rc) {
            return OPAL_VALUE2_GREATER;
        }
    }

    if (NULL == value1->config.env || NULL == value2->config.env) {
        if (NULL != value1->config.env && NULL == value2->config.env) {
            return OPAL_VALUE1_GREATER;
        } else if (NULL == value1->config.env && NULL != value2->config.env) {
            return OPAL_VALUE2_GREATER;
        }
    } else {
        size1 = opal_argv_count(value1->config.env);
        size2 = opal_argv_count(value2->config.env);

        if (size1 > size2) {
            return OPAL_VALUE1_GREATER;
        } else if (size1 < size2) {
            return OPAL_VALUE2_GREATER;
        }

        /* Compare the full "argv" list of env. This must be terminated with a NULL "sentinel" element */
        string_comp1 = opal_argv_join(value1->config.env, 0);
        string_comp2 = opal_argv_join(value2->config.env, 0);

        rc = strcmp(string_comp1, string_comp2);

        free(string_comp1);
        free(string_comp2);

        if (0 < rc) {
            return OPAL_VALUE1_GREATER;
        } else if (0 > rc) {
            return OPAL_VALUE2_GREATER;
        }
    }

    if (NULL == value1->config.port || NULL == value2->config.port) {
        if (NULL != value1->config.port && NULL == value2->config.port) {
            return OPAL_VALUE1_GREATER;
        } else if (NULL == value1->config.port && NULL != value2->config.port) {
            return OPAL_VALUE2_GREATER;
        }
    } else {
        rc = strcmp(value1->config.port, value2->config.port);

        if (0 < rc) {
            return OPAL_VALUE1_GREATER;
        } else if (0 > rc) {
            return OPAL_VALUE2_GREATER;
        }
    }

    if (value1->config.aggregator > value2->config.aggregator) {
        return OPAL_VALUE1_GREATER;
    } else if (value1->config.aggregator < value2->config.aggregator) {
        return OPAL_VALUE2_GREATER;
    }

    if (value1->state > value2->state) {
        return OPAL_VALUE1_GREATER;
    } else if (value1->state < value2->state) {
        return OPAL_VALUE2_GREATER;
    }

    if (value1->scd_state > value2->scd_state) {
        return OPAL_VALUE1_GREATER;
    } else if (value1->scd_state < value2->scd_state) {
        return OPAL_VALUE2_GREATER;
    }

    return OPAL_EQUAL;
}


int orcm_compare_rack(orcm_rack_t *value1, orcm_rack_t *value2, opal_data_type_t type)
{
    int rc;

    rc = strcmp(value1->name, value2->name);
    if (0 < rc) {
        return OPAL_VALUE1_GREATER;
    } else if (rc < 0) {
        return OPAL_VALUE2_GREATER;
    }
    return OPAL_EQUAL;
}

int orcm_compare_row(orcm_row_t *value1, orcm_row_t *value2, opal_data_type_t type)
{
    int rc;

    rc = strcmp(value1->name, value2->name);
    if (0 < rc) {
        return OPAL_VALUE1_GREATER;
    } else if (rc < 0) {
        return OPAL_VALUE2_GREATER;
    }
    return OPAL_EQUAL;
}

int orcm_compare_cluster(orcm_cluster_t *value1, orcm_cluster_t *value2, opal_data_type_t type)
{
    int rc;

    rc = strcmp(value1->name, value2->name);
    if (0 < rc) {
        return OPAL_VALUE1_GREATER;
    } else if (rc < 0) {
        return OPAL_VALUE2_GREATER;
    }
    return OPAL_EQUAL;
}

int orcm_compare_scheduler(orcm_scheduler_t *value1, orcm_scheduler_t *value2, opal_data_type_t type)
{
    int rc;
    int size1, size2;
    orcm_node_t *node1, *node2;
    char *string_comp1, *string_comp2;

    node1 = &value1->controller;
    node2 = &value2->controller;

    rc = opal_dss.compare(node1, node2, ORCM_NODE);

    if (NULL == value1->queues || NULL == value2->queues) {
        if (NULL != value1->queues && NULL == value2->queues) {
            return OPAL_VALUE1_GREATER;
        } else if (NULL == value1->queues && NULL != value2->queues) {
            return OPAL_VALUE2_GREATER;
        }
    } else {
        size1 = opal_argv_count(value1->queues);
        size2 = opal_argv_count(value2->queues);

        if (size1 > size2) {
            return OPAL_VALUE1_GREATER;
        } else if (size1 < size2) {
            return OPAL_VALUE2_GREATER;
        }

        string_comp1 = opal_argv_join(value1->queues, 0);
        string_comp2 = opal_argv_join(value2->queues, 0);

        rc = strcmp(string_comp1, string_comp2);

        free(string_comp1);
        free(string_comp2);

        if (0 < rc) {
            return OPAL_VALUE1_GREATER;
        } else if (0 > rc) {
            return OPAL_VALUE2_GREATER;
        }
    }

    return OPAL_EQUAL;
}

/*****     COPY ROUTINES    *****/
int orcm_copy_node(orcm_node_t **dest, orcm_node_t *src, opal_data_type_t type)
{
    (*dest) = src;
    OBJ_RETAIN(src);
    
    return ORTE_SUCCESS;
}

int orcm_copy_rack(orcm_rack_t **dest, orcm_rack_t *src, opal_data_type_t type)
{
    (*dest) = src;
    OBJ_RETAIN(src);
    
    return ORTE_SUCCESS;
}

int orcm_copy_row(orcm_row_t **dest, orcm_row_t *src, opal_data_type_t type)
{
    (*dest) = src;
    OBJ_RETAIN(src);
    
    return ORTE_SUCCESS;
}

int orcm_copy_cluster(orcm_cluster_t **dest, orcm_cluster_t *src, opal_data_type_t type)
{
    (*dest) = src;
    OBJ_RETAIN(src);
    
    return ORTE_SUCCESS;
}

int orcm_copy_scheduler(orcm_scheduler_t **dest, orcm_scheduler_t *src, opal_data_type_t type)
{
    (*dest) = src;
    OBJ_RETAIN(src);
    
    return ORTE_SUCCESS;
}


/*****     PRINT ROUTINES    *****/
static int print_config(char **output, char *prefix, orcm_config_t *config)
{
    char *tmp, *tmp2, *pfx2;
    int i;

    /* protect against NULL prefix */
    if (NULL == prefix) {
        asprintf(&pfx2, " ");
    } else {
        asprintf(&pfx2, "%s    ", prefix);
    }
    asprintf(&tmp, "\n%sMCA PARAMS", pfx2);
    if (NULL != config->mca_params) {
        for (i=0; NULL != config->mca_params[i]; i++) {
            asprintf(&tmp2, "%s\n%s    %s", tmp, pfx2, config->mca_params[i]);
            free(tmp);
            tmp = tmp2;
        }
    }
    asprintf(&tmp2, "%s\n%sENV", tmp, pfx2);
    free(tmp);
    tmp = tmp2;
    if (NULL != config->env) {
        for (i=0; NULL != config->env[i]; i++) {
            asprintf(&tmp2, "%s\n%s    %s", tmp, pfx2, config->env[i]);
            free(tmp);
            tmp = tmp2;
        }
    }
    asprintf(&tmp2, "%s\n%sPORT: %s AGGREGATOR: %c", tmp, pfx2,
             (NULL == config->port) ? "N/A" : config->port,
             config->aggregator ? 'Y' : 'N');
    free(tmp);
    tmp = tmp2;
    free(pfx2);
    *output = tmp;
    return ORCM_SUCCESS;
}

int orcm_print_node(char **output, char *prefix, orcm_node_t *src, opal_data_type_t type)
{
    char *tmp, *pfx2;

    /* set default result */
    *output = NULL;

    /* protect against NULL prefix */
    if (NULL == prefix) {
        print_config(&tmp, NULL, &src->config);
        pfx2 = strdup(" ");
    } else {
        asprintf(&pfx2, "%s    ", prefix);
        print_config(&tmp, pfx2, &src->config);
    }

    if (NULL != tmp) {
        asprintf(output, "%sData for node: %s  Daemon: %s%s", pfx2,
                 (NULL == src->name) ? "N/A" : src->name, ORTE_NAME_PRINT(&src->daemon), tmp);
        free(tmp);
    } else {
        asprintf(output, "%sData for node: %s  Daemon: %s", pfx2,
                 (NULL == src->name) ? "N/A" : src->name, ORTE_NAME_PRINT(&src->daemon));
    }
    free(pfx2);

    return ORTE_SUCCESS;
}

int orcm_print_rack(char **output, char *prefix, orcm_rack_t *src, opal_data_type_t type)
{
    char *tmp, *tmp2, *tmp3, *pfx2;
    int k;
    orcm_node_t *node;

    /* set default result */
    *output = NULL;

    /* protect against NULL prefix */

    k = opal_list_get_size(&src->nodes);
    if (NULL == prefix) {
        asprintf(&tmp, "\nData for rack: %s  #nodes: %d",
                 (NULL == src->name) ? "N/A" : src->name, k);
        asprintf(&pfx2, "    ");
    } else {
        asprintf(&tmp, "\n%sData for rack: %s  #nodes: %d", prefix,
                 (NULL == src->name) ? "N/A" : src->name, k);
        asprintf(&pfx2, "%s    ", prefix);
    }
    orcm_print_node(&tmp2, pfx2, &src->controller, ORCM_NODE);
    if (NULL != tmp2) {
        asprintf(&tmp3, "%s\n%sController:\n%s", tmp, pfx2, tmp2);
        free(tmp);
        free(tmp2);
        tmp = tmp3;
    }
    asprintf(&tmp2, "%s\n%sNodes:", tmp, pfx2);
    free(tmp);
    tmp = tmp2;
    OPAL_LIST_FOREACH(node, &src->nodes, orcm_node_t) {
        orcm_print_node(&tmp2, pfx2, node, ORCM_NODE);
        if (NULL != tmp2) {
            asprintf(&tmp3, "%s\n%s", tmp, tmp2);
            free(tmp);
            free(tmp2);
            tmp = tmp3;
        }
    }
    *output = tmp;
    free(pfx2);

    return ORTE_SUCCESS;
}

int orcm_print_row(char **output, char *prefix, orcm_row_t *src, opal_data_type_t type)
{
    char *tmp, *tmp2, *tmp3, *pfx2;
    int k;
    orcm_rack_t *rack;

    /* set default result */
    *output = NULL;

    k = opal_list_get_size(&src->racks);
    if (NULL == prefix) {
        asprintf(&tmp, "\nData for row: %s  #racks: %d",
                 (NULL == src->name) ? "N/A" : src->name, k);
        asprintf(&pfx2, "   ");
    } else {
        asprintf(&tmp, "\n%sData for row: %s  #racks: %d", prefix,
                 (NULL == src->name) ? "N/A" : src->name, k);
        asprintf(&pfx2, "%s   ", prefix);
    }
    orcm_print_node(&tmp2, pfx2, &src->controller, ORCM_NODE);
    if (NULL != tmp2) {
        asprintf(&tmp3, "%s\n%sController:\n%s", tmp, pfx2, tmp2);
        free(tmp);
        free(tmp2);
        tmp = tmp3;
    }
    OPAL_LIST_FOREACH(rack, &src->racks, orcm_rack_t) {
        orcm_print_rack(&tmp2, pfx2, rack, ORCM_RACK);
        if (NULL != tmp2) {
            asprintf(&tmp3, "%s\n%s", tmp, tmp2);
            free(tmp);
            free(tmp2);
            tmp = tmp3;
        }
    }
    *output = tmp;
    free(pfx2);

    return ORTE_SUCCESS;
}

int orcm_print_cluster(char **output, char *prefix, orcm_cluster_t *src, opal_data_type_t type)
{
    char *tmp, *tmp2, *tmp3, *pfx2;
    int k;
    orcm_row_t *row;

    /* set default result */
    *output = NULL;

    k = opal_list_get_size(&src->rows);
    if (NULL == prefix) {
        asprintf(&tmp, "\nData for cluster: %s  #rows: %d",
                 (NULL == src->name) ? "N/A" : src->name, k);
        asprintf(&pfx2, "   ");
    } else {
        asprintf(&tmp, "\n%sData for cluster: %s  #rows: %d", prefix,
                 (NULL == src->name) ? "N/A" : src->name, k);
        asprintf(&pfx2, "%s   ", prefix);
    }
    orcm_print_node(&tmp2, pfx2, &src->controller, ORCM_NODE);
    if (NULL != tmp2) {
        asprintf(&tmp3, "%s\n%sController:\n%s", tmp, pfx2, tmp2);
        free(tmp);
        free(tmp2);
        tmp = tmp3;
    }
    OPAL_LIST_FOREACH(row, &src->rows, orcm_row_t) {
        orcm_print_row(&tmp2, pfx2, row, ORCM_ROW);
        if (NULL != tmp2) {
            asprintf(&tmp3, "%s\n%s", tmp, tmp2);
            free(tmp);
            free(tmp2);
            tmp = tmp3;
        }
    }
    *output = tmp;
    free(pfx2);

    return ORTE_SUCCESS;
}

int orcm_print_scheduler(char **output, char *prefix, orcm_scheduler_t *src, opal_data_type_t type)
{
    char *tmp, *tmp2, *tmp3, *pfx2;
    int k;

    /* set default result */
    *output = NULL;

    k = opal_argv_count(src->queues);
    if (NULL == prefix) {
        asprintf(&tmp, "\nData for scheduler: #queues: %d", k);
        asprintf(&pfx2, "    ");
    } else {
        asprintf(&tmp, "\n%sData for scheduler: #queues: %d", prefix, k);
        asprintf(&pfx2, "%s    ", prefix);
    }
    orcm_print_node(&tmp2, pfx2, &src->controller, ORCM_NODE);
    if (NULL != tmp2) {
        asprintf(&tmp3, "%s\n%sController:\n%s", tmp, pfx2, tmp2);
        free(tmp);
        free(tmp2);
        tmp = tmp3;
    }
    if (NULL == src->queues) {
        asprintf(&tmp2, "%s\n%sQUEUES: NONE", tmp, pfx2);
    } else {
        asprintf(&tmp2, "%s\n%sQUEUES", tmp, pfx2);
        free(tmp);
        tmp = tmp2;
        for (k=0; NULL != src->queues[k]; k++) {
            asprintf(&tmp2, "%s\n%s    %s", tmp, pfx2, src->queues[k]);
            free(tmp);
            tmp = tmp2;
        }
    }
    *output = tmp;
    if (tmp != tmp2) {
        if (tmp2) {
            free (tmp2);
        }
    }
    free(pfx2);

    return ORTE_SUCCESS;
}
