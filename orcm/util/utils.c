/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */


#include "orcm_config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#include <ctype.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif

#include "opal/dss/dss.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal/util/opal_environ.h"
#include "opal/util/if.h"
#include "opal/util/net.h"
#include "opal/util/show_help.h"
#include "opal/mca/if/if.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"

#define ORCM_MAX_LINE_LENGTH  512


void orcm_util_construct_uri(opal_buffer_t *buf, orcm_node_t *node)
{
    char *uri;
    char *proc_name;
    char *addr;
    struct hostent *h;

    if (0 == strcmp(node->name, orte_process_info.nodename) ||
        0 == strcmp(node->name, "localhost") ||
        opal_ifislocal(node->name)) {
        /* just use "localhost" */
        addr = "localhost";
    } else if (opal_net_isaddr(node->name)) {
        addr = node->name;
    } else {
        /* lookup the address of this node */
        if (NULL == (h = gethostbyname(node->name))) {
            opal_output_verbose(2, orcm_debug_output,
                                "%s cannot resolve node %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), node->name);
            return;
        }
        addr = inet_ntoa(*(struct in_addr*)h->h_addr_list[0]);
    }
    orte_util_convert_process_name_to_string(&proc_name, &node->daemon);
    asprintf(&uri, "%s;tcp://%s:%s", proc_name, addr, node->config.port);
    opal_output_verbose(2, orcm_debug_output,
                        "%s orcm:util: node %s addr %s uri %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        node->name, addr, uri);
    opal_dss.pack(buf, &uri, 1, OPAL_STRING);
    free(proc_name);
    free(uri);
}

int orcm_util_get_dependents(opal_list_t *targets,
                             orte_process_name_t *root)
{
    orcm_cluster_t *cluster;
    orcm_row_t *row;
    orcm_rack_t *rack;
    orcm_node_t *node;
    orte_namelist_t *nm;

    /* cycle thru the clusters until we find the controller
     * that matches the given name - dependents include
     * everything below it
     */
    OPAL_LIST_FOREACH(cluster, orcm_clusters, orcm_cluster_t) {
        OPAL_LIST_FOREACH(row, &cluster->rows, orcm_row_t) {
            if (row->controller.daemon.jobid == root->jobid &&
                row->controller.daemon.vpid == root->vpid) {
                /* collect the daemons from all the racks in this row */
                OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
                    nm = OBJ_NEW(orte_namelist_t);
                    nm->name.jobid = rack->controller.daemon.jobid;
                    nm->name.vpid = rack->controller.daemon.vpid;
                    opal_list_append(targets, &nm->super);
                    OPAL_LIST_FOREACH(node, &rack->nodes, orcm_node_t) {
                        nm = OBJ_NEW(orte_namelist_t);
                        nm->name.jobid = node->daemon.jobid;
                        nm->name.vpid = node->daemon.vpid;
                        opal_list_append(targets, &nm->super);
                    }
                }
                return ORCM_SUCCESS;
            }
        }
        /* if we get here, we did not find it as a row
         * controller, so see if it is a rack
         */
        OPAL_LIST_FOREACH(row, &cluster->rows, orcm_row_t) {
            OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
                if (rack->controller.daemon.jobid == root->jobid &&
                    rack->controller.daemon.vpid == root->vpid) {
                    /* collect the daemons from all the nodes in this rack */
                    OPAL_LIST_FOREACH(node, &rack->nodes, orcm_node_t) {
                        nm = OBJ_NEW(orte_namelist_t);
                        nm->name.jobid = node->daemon.jobid;
                        nm->name.vpid = node->daemon.vpid;
                        opal_list_append(targets, &nm->super);
                    }
                    return ORCM_SUCCESS;
                }
            }
        }
        /* if we get here, then it is not a row or a rack - see
         * if it is in another cluster
         */
    }
    return ORCM_ERR_NOT_FOUND;
}

void orcm_util_print_xml(orcm_cfgi_xml_parser_t *x, char *pfx)
{
    int i;
    orcm_cfgi_xml_parser_t *y;
    char *p2;

    if (NULL == pfx) {
        opal_output(0, "tag: %s", x->name);
    } else {
        opal_output(0, "%stag: %s", pfx, x->name);
    }
    if (NULL != x->value) {
        for (i=0; NULL != x->value[i]; i++) {
            if (NULL == pfx) {
                opal_output(0, "    value: %s", x->value[i]);
            } else {
                opal_output(0, "%s    value: %s", pfx, x->value[i]);
            }
        }
    }
    if (NULL == pfx) {
        p2 = strdup("    ");
    } else {
        asprintf(&p2, "%s    ", pfx);
    }
    OPAL_LIST_FOREACH(y, &x->subvals, orcm_cfgi_xml_parser_t) {
        orcm_util_print_xml(y, p2);
    }
    free(p2);
}
