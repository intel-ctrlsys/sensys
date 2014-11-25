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
#include "orcm/constants.h"

#include <sys/types.h>

#include "opal_stdint.h"
#include "opal/util/argv.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/cfgi/base/base.h"

int orcm_cfgi_base_read_config(opal_list_t *config)
{
    orcm_cfgi_base_active_t *a;
    bool rd;
    int rc;

    rd = false;
    OPAL_LIST_FOREACH(a, &orcm_cfgi_base.actives, orcm_cfgi_base_active_t) {
        if (NULL == a->mod->read_config) {
            continue;
        }
        if (ORCM_SUCCESS == (rc = a->mod->read_config(config))) {
            rd = true;
            break;
        }
        if (ORCM_ERR_TAKE_NEXT_OPTION != rc) {
            /* plugins return "next option" if they can't read
             * the config. anything else is a true error.
             */
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }

    if (!rd) {
        /* nobody could read the config */
        ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;
}

int orcm_cfgi_base_define_sys(opal_list_t *config,
                              orcm_node_t **mynode,
                              orte_vpid_t *num_procs,
                              opal_buffer_t *buf)
{
    orcm_cfgi_base_active_t *a;
    bool rd;
    int rc;

    rd = false;
    OPAL_LIST_FOREACH(a, &orcm_cfgi_base.actives, orcm_cfgi_base_active_t) {
        if (NULL == a->mod->define_system) {
            continue;
        }
        if (ORCM_SUCCESS == (rc = a->mod->define_system(config, mynode, num_procs, buf))) {
            rd = true;
            break;
        }
        if (ORCM_ERR_TAKE_NEXT_OPTION != rc) {
            /* plugins return "next option" if they can't read
             * the define system. anything else is a true error.
             */
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }

    if (!rd) {
        /* nobody could parse the config */
        ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;
}

/* given orte_process_name_t *proc, set hostname from config */
int orcm_cfgi_base_get_proc_hostname(orte_process_name_t *proc, char **hostname)
{
    orcm_cluster_t *cluster;
    orcm_row_t *row;
    orcm_rack_t *rack;
    orcm_node_t *node;

    OPAL_LIST_FOREACH(cluster, orcm_clusters, orcm_cluster_t) {
        node = &cluster->controller;
        if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL, &node->daemon, proc)) {
            *hostname = strdup(node->name);
            return ORCM_SUCCESS;
        }

        OPAL_LIST_FOREACH(row, &cluster->rows, orcm_row_t) {
            node = &row->controller;
            if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL, &node->daemon, proc)) {
                *hostname = strdup(node->name);
                return ORCM_SUCCESS;
            }

            OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
                node = &rack->controller;
                if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL, &node->daemon, proc)) {
                    *hostname = strdup(node->name);
                    return ORCM_SUCCESS;
                }

                OPAL_LIST_FOREACH(node, &rack->nodes, orcm_node_t) {
                    if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL, &node->daemon, proc)) {
                        *hostname = strdup(node->name);
                        return ORCM_SUCCESS;
                    }
                }
            }
        }
    }

    return ORCM_ERR_NOT_FOUND;
}

/* given hostname, set orte_process_name_t proc from config */
int orcm_cfgi_base_get_hostname_proc(char *hostname, orte_process_name_t *proc)
{
    orcm_cluster_t *cluster;
    orcm_row_t *row;
    orcm_rack_t *rack;
    orcm_node_t *node;

    OPAL_LIST_FOREACH(cluster, orcm_clusters, orcm_cluster_t) {
        node = &cluster->controller;
        if ((NULL != node->name) && (0 == strcmp(node->name, hostname))) {
            proc->vpid = node->daemon.vpid;
            proc->jobid = node->daemon.jobid;
            return ORCM_SUCCESS;
        }

        OPAL_LIST_FOREACH(row, &cluster->rows, orcm_row_t) {
            node = &row->controller;
            if ((NULL != node->name) && (0 == strcmp(node->name, hostname))) {
                proc->vpid = node->daemon.vpid;
                proc->jobid = node->daemon.jobid;
                return ORCM_SUCCESS;
            }

            OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
                node = &rack->controller;
                if ((NULL != node->name) && (0 == strcmp(node->name, hostname))) {
                    proc->vpid = node->daemon.vpid;
                    proc->jobid = node->daemon.jobid;
                    return ORCM_SUCCESS;
                }

                OPAL_LIST_FOREACH(node, &rack->nodes, orcm_node_t) {
                    if ((NULL != node->name) && (0 == strcmp(node->name, hostname))) {
                        proc->vpid = node->daemon.vpid;
                        proc->jobid = node->daemon.jobid;
                        return ORCM_SUCCESS;
                    }
                }
            }
        }
    }

    return ORCM_ERR_NOT_FOUND;
}
