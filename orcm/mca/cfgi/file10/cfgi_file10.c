/*
 * Copyright (c) 2014 - 2016     Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

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
#include "opal/util/if.h"
#include "opal/util/net.h"
#include "opal/util/opal_environ.h"
#include "opal/util/show_help.h"

#include "orte/runtime/orte_globals.h"
#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/routed/routed.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"

#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/cfgi/file10/cfgi_file10.h"

#include "orcm/mca/parser/parser.h"
#include "orcm/mca/parser/base/base.h"

#define ORCM_MAX_LINE_LENGTH  512

/* API functions */

static int file10_init(void);
static void file10_finalize(void);
static int read_config(opal_list_t *config);
static int define_system(opal_list_t *config,
                         orcm_node_t **mynode,
                         orte_vpid_t *num_procs,
                         opal_buffer_t *buf);
static opal_list_t* file10_read_configuration_section(void);
static int file10_read_version(opal_list_t *result_list, int num_loop);

/* The module struct */

orcm_cfgi_base_module_t orcm_cfgi_file10_module = {
    file10_init,
    file10_finalize,
    read_config,
    define_system
};

static opal_list_t* file10_read_configuration_section(void)
{

    opal_list_t *result_list = NULL;
    int file_id;

    if (NULL == orcm_cfgi_base.config_file) {
        opal_output(orcm_cfgi_base_framework.framework_output, "NULL FILE");
        return NULL;
    }

    if (0 > (file_id = orcm_parser.open(orcm_cfgi_base.config_file))) {
        opal_output(orcm_cfgi_base_framework.framework_output,
                    "Can't open %s", orcm_cfgi_base.config_file);
        return NULL;
    }

    result_list = orcm_parser.retrieve_document(file_id);

    orcm_parser.close(file_id);
    return result_list;
}

static int file10_read_version(opal_list_t *result_list, int num_loop)
{
    orcm_value_t *list_item = NULL;

    /*restrict the "version" tag to be checked for one loop */
    if (1 < num_loop) {
        orcm_cfgi_base.version = 1.0;
        return ORCM_SUCCESS;
    }

    OPAL_LIST_FOREACH(list_item, result_list, orcm_value_t) {

        if (OPAL_PTR == list_item->value.type) {
            return file10_read_version((opal_list_t *)list_item->value.data.ptr, num_loop+1);
        }
        else if (0 == strcasecmp("version", list_item->value.key)) {
            opal_output(orcm_cfgi_base_framework.framework_output,
                        "CFGI1.0 shouldn't have version tag.");
            return ORCM_ERR_TAKE_NEXT_OPTION;
        }
    }
    return ORCM_SUCCESS;
}

static int file10_init(void)
{
    opal_list_t *result_list = NULL;
    int rc = ORCM_SUCCESS;

    result_list = file10_read_configuration_section();
    if (NULL == result_list) {
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }

    rc = file10_read_version(result_list, 0);

    SAFE_RELEASE_NESTED_LIST(result_list);
    /*return success if there is no version tag available*/
    return rc;
}

static void file10_finalize(void)
{
}

static int parse_configuration(orcm_cfgi_xml_parser_t *xml,
                               orcm_row_t *row);
static int parse_config(opal_list_t *result_list,
                         orcm_cfgi_xml_parser_t *xml_in);
static bool check_me(orcm_config_t *config, char *node,
                     orte_vpid_t vpid, char *my_ip);

static int read_config(opal_list_t *config)
{
    orcm_cfgi_xml_parser_t *xml = NULL;
    orcm_cfgi_xml_parser_t *config_xml = NULL;
    opal_list_t *result_list = NULL;
    int rc = ORCM_SUCCESS;
    opal_list_item_t *first_item;

    opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                        "cfgi:file10 reading config");

    /* open and parse the config file - this file contains a
     * list of all the nodes in this orcm "group", a groupID
     * that we will use as our jobid, and the static port(s)
     * assigned to orcm.
     */
    result_list = file10_read_configuration_section();
    if (NULL == result_list) {
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }

    config_xml = OBJ_NEW(orcm_cfgi_xml_parser_t);

    rc = parse_config(result_list, config_xml);
    SAFE_RELEASE_NESTED_LIST(result_list);
    if (ORCM_SUCCESS != rc) {
        OBJ_RELEASE(config_xml);
        return rc;
    }

    first_item = opal_list_remove_first(&config_xml->subvals);
    if (NULL == first_item) {
        OBJ_RELEASE(config_xml);
        return ORCM_ERROR;
    }
    opal_list_append(config, first_item);


    /* print it out, if requested */
    if (30 < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
        OPAL_LIST_FOREACH(xml, config, orcm_cfgi_xml_parser_t) {
            orcm_util_print_xml(xml, NULL);
        }
    }

    OBJ_RELEASE(config_xml);
    return rc;
}

static char *log=NULL;
static orcm_config_t daemon_cfg;
static orcm_config_t agg_cfg;
static orcm_config_t scd_cfg;
static char **aggregators=NULL;

static int define_system(opal_list_t *config,
                         orcm_node_t **mynode,
                         orte_vpid_t *num_procs,
                         opal_buffer_t *buf)
{
    struct hostent *h;
    char *my_ip;
    orte_vpid_t vpid;
    bool found_me;
    int rc, nagg;
    orcm_cfgi_xml_parser_t *x;
    orcm_cluster_t *cluster;
    orcm_scheduler_t *scheduler;
    orcm_row_t *row;
    orcm_rack_t *rack;
    orcm_node_t *node;
    int32_t num;
    opal_buffer_t uribuf, schedbuf, clusterbuf, rowbuf, rackbuf;
    opal_buffer_t *bptr;

    /* set default */
    *mynode = NULL;

    /* convert my hostname to its IP form just in case we need it */
    if (opal_net_isaddr(orte_process_info.nodename)) {
        my_ip = orte_process_info.nodename;
    } else {
        /* lookup the address of this node */
        if (NULL == (h = gethostbyname(orte_process_info.nodename))) {
            /* if we can't get it for some reason, don't worry about it */
            my_ip = orte_process_info.nodename;
        } else {
            my_ip = inet_ntoa(*(struct in_addr*)h->h_addr_list[0]);
        }
    }

    /* setup a cluster and scheduler object - we can only have
     * one of each in this format, and the cluster has a single
     * row and rack
     */
    cluster = OBJ_NEW(orcm_cluster_t);
    cluster->name = strdup("DELINEATED");
    opal_list_append(orcm_clusters, &cluster->super);
    row = OBJ_NEW(orcm_row_t);
    row->name = strdup("A");
    opal_list_append(&cluster->rows, &row->super);

    /* setup config */
    OBJ_CONSTRUCT(&daemon_cfg, orcm_config_t);
    OBJ_CONSTRUCT(&agg_cfg, orcm_config_t);
    OBJ_CONSTRUCT(&scd_cfg, orcm_config_t);
    OBJ_CONSTRUCT(&uribuf, opal_buffer_t);
    OBJ_CONSTRUCT(&clusterbuf, opal_buffer_t);

    /* parse the config to extract the daemon, aggregator, and scheduler info */
    OPAL_LIST_FOREACH(x, config, orcm_cfgi_xml_parser_t) {
        if (ORTE_SUCCESS != (rc = parse_configuration(x, row))) {
            return rc;
        }
    }

    /* track the vpids for the daemons */
    vpid = 0;

    /* cycle thru the list of schedulers - they will take the leading positions
     * in the assignment of process names
     */
    found_me = false;
    OBJ_CONSTRUCT(&schedbuf, opal_buffer_t);
    OPAL_LIST_FOREACH(scheduler, orcm_schedulers, orcm_scheduler_t) {
        if (ORTE_NODE_STATE_UNDEF != scheduler->controller.state) {
            /* the cluster includes a scheduler */
            scheduler->controller.daemon.jobid = 0;
            scheduler->controller.daemon.vpid = vpid;
            if (NULL != scd_cfg.port) {
                scheduler->controller.config.port = strdup(scd_cfg.port);
                orcm_util_construct_uri(&uribuf, &scheduler->controller);  // record its URI
            }
            if (NULL != scd_cfg.mca_params) {
                scheduler->controller.config.mca_params = opal_argv_copy(scd_cfg.mca_params);
            }
            if (NULL != scd_cfg.env) {
                scheduler->controller.config.env = opal_argv_copy(scd_cfg.env);
            }
            /* have to treat scheduler separately as we allow daemons
             * and aggregators to share location with a scheduler
             */
            if (!found_me && ORTE_PROC_IS_SCHEDULER) {
                found_me = check_me(&scheduler->controller.config,
                                    scheduler->controller.name, vpid, my_ip);
                if (found_me) {
                    *mynode = &scheduler->controller;
                    OBJ_RETAIN(*mynode);
                }
            }
            /* add to the scheduler definitions */
            opal_dss.pack(&schedbuf, &scheduler->controller.daemon, 1, ORTE_NAME);
            /* roll to the next vpid */
            ++vpid;
            if (30 < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
                opal_dss.dump(0, scheduler, ORCM_SCHEDULER);
            }
        }
    }
    /* transfer the scheduler section to the cluster definition */
    bptr = &schedbuf;
    opal_dss.pack(&clusterbuf, &bptr, 1, OPAL_BUFFER);
    OBJ_DESTRUCT(&schedbuf);
    /* take the first scheduler as our own */
    if (0 < opal_list_get_size(orcm_schedulers)) {
        scheduler = (orcm_scheduler_t*)opal_list_get_first(orcm_schedulers);
    } else {
        scheduler = NULL;
        /* since no scheduler is in the system, let's assume
         * for now that orun will act as the HNP and adjust
         * all the vpid's for the daemons
         */
        vpid = 1;
    }

    /* cycle thru the cluster setting up the remaining names */
    nagg = 0;
    found_me = false;
    OPAL_LIST_FOREACH(cluster, orcm_clusters, orcm_cluster_t) {
        /* this format doesn't have a way of defining a cluster controller */
        opal_dss.pack(&clusterbuf, ORTE_NAME_INVALID, 1, ORTE_NAME);
        /* pack the number of rows */
        num = opal_list_get_size(&cluster->rows);
        opal_dss.pack(&clusterbuf, &num, 1, OPAL_INT32);
        OPAL_LIST_FOREACH(row, &cluster->rows, orcm_row_t) {
            OBJ_CONSTRUCT(&rowbuf, opal_buffer_t);
            /* pack the number of racks */
            num = opal_list_get_size(&row->racks);
            opal_dss.pack(&rowbuf, &num, 1, OPAL_INT32);
            /* if we have aggregators, assign one to each row */
            if (NULL != aggregators &&
                nagg < opal_argv_count(aggregators) &&
                NULL != aggregators[nagg]) {
                /* the row includes a controller */
                row->controller.name = strdup(aggregators[nagg]);
                ++nagg;
                row->controller.daemon.jobid = 0;
                row->controller.daemon.vpid = vpid;
                row->controller.config.aggregator = true;
                row->controller.state = ORTE_NODE_STATE_UNKNOWN;
                if (NULL != agg_cfg.port) {
                    row->controller.config.port = strdup(agg_cfg.port);
                    orcm_util_construct_uri(&uribuf, &row->controller);
                }
                if (NULL != agg_cfg.mca_params) {
                    row->controller.config.mca_params = opal_argv_copy(agg_cfg.mca_params);
                }
                if (NULL != agg_cfg.env) {
                    row->controller.config.env = opal_argv_copy(agg_cfg.env);
                }
                if (!found_me && ORCM_PROC_IS_DAEMON) {
                    found_me = check_me(&row->controller.config,
                                        row->controller.name, vpid, my_ip);
                    if (found_me) {
                        *mynode = &row->controller;
                        OBJ_RETAIN(*mynode);
                        if (NULL != scheduler) {
                            /* define my HNP to be the scheduler, if available */
                            ORTE_PROC_MY_HNP->jobid = scheduler->controller.daemon.jobid;
                            ORTE_PROC_MY_HNP->vpid = scheduler->controller.daemon.vpid;
                        } else {
                            /* otherwise, it is just myself */
                            ORTE_PROC_MY_HNP->jobid = ORTE_PROC_MY_NAME->jobid;
                            ORTE_PROC_MY_HNP->vpid = ORTE_PROC_MY_NAME->vpid;
                        }
                        /* define my daemon as myself */
                        ORTE_PROC_MY_DAEMON->jobid = ORTE_PROC_MY_NAME->jobid;
                        ORTE_PROC_MY_DAEMON->vpid = ORTE_PROC_MY_NAME->vpid;
                    }
                }
                ++vpid;
            }
            /* pack the name of the row controller, which will be invalid
             * if we don't have one in the config */
            opal_dss.pack(&rowbuf, &row->controller.daemon, 1, ORTE_NAME);
            OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
                OBJ_CONSTRUCT(&rackbuf, opal_buffer_t);
                /* if we have aggregators, assign one to each rack */
                if (NULL != aggregators &&
                    nagg < opal_argv_count(aggregators) &&
                    NULL != aggregators[nagg]) {
                    /* the rack includes a controller */
                    rack->controller.name = strdup(aggregators[nagg]);
                    ++nagg;
                    rack->controller.daemon.jobid = 0;
                    rack->controller.daemon.vpid = vpid;
                    rack->controller.config.aggregator = true;
                    rack->controller.state = ORTE_NODE_STATE_UNKNOWN;
                    if (NULL != agg_cfg.port) {
                        rack->controller.config.port = strdup(agg_cfg.port);
                        orcm_util_construct_uri(&uribuf, &rack->controller);
                    }
                    if (NULL != agg_cfg.mca_params) {
                        rack->controller.config.mca_params = opal_argv_copy(agg_cfg.mca_params);
                    }
                    if (NULL != agg_cfg.env) {
                        rack->controller.config.env = opal_argv_copy(agg_cfg.env);
                    }
                    if (!found_me && ORCM_PROC_IS_DAEMON) {
                        found_me = check_me(&rack->controller.config,
                                            rack->controller.name, vpid, my_ip);
                        if (found_me) {
                            *mynode = &rack->controller;
                            OBJ_RETAIN(*mynode);
                            (*mynode)->rack = rack;
                            OBJ_RETAIN(rack);
                            if (NULL != scheduler) {
                                /* define my HNP to be the scheduler, if available */
                                ORTE_PROC_MY_HNP->jobid = scheduler->controller.daemon.jobid;
                                ORTE_PROC_MY_HNP->vpid = scheduler->controller.daemon.vpid;
                            } else {
                                /* otherwise, it is just myself */
                                ORTE_PROC_MY_HNP->jobid = ORTE_PROC_MY_NAME->jobid;
                                ORTE_PROC_MY_HNP->vpid = ORTE_PROC_MY_NAME->vpid;
                            }
                            /* define my row controller as my aggregator, if available */
                            if (ORTE_NODE_STATE_UNDEF != row->controller.state) {
                                ORTE_PROC_MY_DAEMON->jobid = row->controller.daemon.jobid;
                                ORTE_PROC_MY_DAEMON->vpid = row->controller.daemon.vpid;
                            } else {
                                ORTE_PROC_MY_DAEMON->jobid = ORTE_PROC_MY_NAME->jobid;
                                ORTE_PROC_MY_DAEMON->vpid = ORTE_PROC_MY_NAME->vpid;
                            }
                        }
                    }
                    ++vpid;
                }
                /* pack the name of the rack controller, which will be invalid
                 * if we don't have one in the config */
                opal_dss.pack(&rackbuf, &rack->controller.daemon, 1, ORTE_NAME);
                OPAL_LIST_FOREACH(node, &rack->nodes, orcm_node_t) {
                    /* nodes always include a daemon */
                    node->daemon.jobid = 0;
                    node->daemon.vpid = vpid;
                    if (NULL != daemon_cfg.port) {
                        node->config.port = strdup(daemon_cfg.port);
                    }
                    if (NULL != daemon_cfg.mca_params) {
                        node->config.mca_params = opal_argv_copy(daemon_cfg.mca_params);
                    }
                    if (NULL != daemon_cfg.env) {
                        node->config.env = opal_argv_copy(daemon_cfg.env);
                    }
                    if (!found_me && ORCM_PROC_IS_DAEMON) {
                        found_me = check_me(&node->config, node->name, vpid, my_ip);
                        if (found_me) {
                            *mynode = node;
                            OBJ_RETAIN(node);
                            node->rack = rack;
                            OBJ_RETAIN(rack);
                            if (ORTE_NODE_STATE_UNDEF != rack->controller.state) {
                                /* define my daemon */
                                ORTE_PROC_MY_DAEMON->jobid = rack->controller.daemon.jobid;
                                ORTE_PROC_MY_DAEMON->vpid = rack->controller.daemon.vpid;
                                if (NULL != scheduler) {
                                    /* define my HNP to be the scheduler, if available */
                                    ORTE_PROC_MY_HNP->jobid = scheduler->controller.daemon.jobid;
                                    ORTE_PROC_MY_HNP->vpid = scheduler->controller.daemon.vpid;
                                } else {
                                    /* its my daemon */
                                    ORTE_PROC_MY_HNP->jobid = ORTE_PROC_MY_DAEMON->jobid;
                                    ORTE_PROC_MY_HNP->vpid = ORTE_PROC_MY_DAEMON->vpid;
                                }
                            } else if (ORTE_NODE_STATE_UNDEF != row->controller.state) {
                                ORTE_PROC_MY_DAEMON->jobid = row->controller.daemon.jobid;
                                ORTE_PROC_MY_DAEMON->vpid = row->controller.daemon.vpid;
                            } else if (NULL != scheduler) {
                                /* fallback to the scheduler */
                                ORTE_PROC_MY_DAEMON->jobid = scheduler->controller.daemon.jobid;
                                ORTE_PROC_MY_DAEMON->vpid = scheduler->controller.daemon.vpid;
                                ORTE_PROC_MY_HNP->jobid = scheduler->controller.daemon.jobid;
                                ORTE_PROC_MY_HNP->vpid = scheduler->controller.daemon.vpid;
                            }
                        }
                    }
                    ++vpid;
                    opal_dss.pack(&rackbuf, &node->daemon, 1, ORTE_NAME);
                }
                bptr = &rackbuf;
                opal_dss.pack(&rowbuf, &bptr, 1, OPAL_BUFFER);
                OBJ_DESTRUCT(&rackbuf);
            }
            bptr = &rowbuf;
            opal_dss.pack(&clusterbuf, &bptr, 1, OPAL_BUFFER);
            OBJ_DESTRUCT(&rowbuf);
        }
        if (30 < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
            opal_dss.dump(0, cluster, ORCM_CLUSTER);
        }
    }

    /* return the number of procs in the system */
    *num_procs = vpid;
    /* provide the cluster definition first */
    bptr = &clusterbuf;
    opal_dss.pack(buf, &bptr, 1, OPAL_BUFFER);
    /* now add the URIs */
    bptr = &uribuf;
    opal_dss.pack(buf, &bptr, 1, OPAL_BUFFER);
    /* cleanup */
    OBJ_DESTRUCT(&uribuf);
    OBJ_DESTRUCT(&clusterbuf);

    return ORTE_SUCCESS;
}

static int parse_config(opal_list_t *result_list,
                         orcm_cfgi_xml_parser_t *xml_in)
{
    orcm_value_t *list_item = NULL;
    orcm_cfgi_xml_parser_t *xml = NULL;
    int rc;

    OPAL_LIST_FOREACH(list_item, result_list, orcm_value_t) {

        if (OPAL_PTR == list_item->value.type) {
            xml = OBJ_NEW(orcm_cfgi_xml_parser_t);

            xml->name = strdup(list_item->value.key);
            if (NULL == xml->name) {
                return ORCM_ERR_OUT_OF_RESOURCE;
            }

            rc = parse_config((opal_list_t *)list_item->value.data.ptr, xml);
            if (ORCM_SUCCESS != rc) {
                return rc;
            }

            opal_list_append(&xml_in->subvals, &xml->super);
        }
        else if (0 == strcmp("value", list_item->value.key)) {
            opal_argv_append_nosize(&xml_in->value, list_item->value.data.string);
        }
    }
    return ORCM_SUCCESS;
}

static int parse_daemons(orcm_cfgi_xml_parser_t *xml,
                         orcm_rack_t *rack)
{
    char *val, **vals;
    int n;
    orcm_node_t *node;
    bool foundlocal = false;

    if (0 == strcmp(xml->name, "port")) {
        if (NULL != xml->value && NULL != xml->value[0] &&
            0 < strlen(xml->value[0])) {
            /* specifies the orcm-port to be used */
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-PORT %s", xml->value[0]);
            daemon_cfg.port = strdup(xml->value[0]);
        }
    } else if (0 == strcmp(xml->name, "cores")) {
        if (NULL != xml->value && NULL != xml->value[0] &&
            0 < strlen(xml->value[0])) {
            /* specifies the cores to be used */
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-CORES %s", xml->value[0]);
            /* all we need do is push this into the corresponding param */
            asprintf(&val, OPAL_MCA_PREFIX"orte_daemon_cores=%s", xml->value[0]);
            opal_argv_append_nosize(&daemon_cfg.mca_params, val);
            free(val);
        }
    } else if (0 == strcmp(xml->name, "mca-params")) {
        if (NULL != xml->value && NULL != xml->value[0] &&
            0 < strlen(xml->value[0])) {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-MCA-PARAMS %s", xml->value[0]);
            vals = opal_argv_split(xml->value[0], ',');
            if (NULL != vals) {
                for (n=0; NULL != vals[n]; n++) {
                    /* add the MCA prefix, if required */
                    if (0 != strncmp(vals[n], OPAL_MCA_PREFIX, strlen(OPAL_MCA_PREFIX))) {
                        asprintf(&val, OPAL_MCA_PREFIX"%s", vals[n]);
                    } else {
                        val = strdup(vals[n]);
                    }
                    opal_argv_append_nosize(&daemon_cfg.mca_params, val);
                    free(val);
                }
                opal_argv_free(vals);
            }
        }
    } else if (0 == strcmp(xml->name, "nodes")) {
        /* this is the node entry - it contains an argv-style list of
         * strings identifying the nodes in this group
         */
        if (NULL != xml->value) {
            for (n=0; NULL != xml->value[n]; n++) {
                if (0 == strcmp(xml->value[n], "localhost")) {
                    /* there can be only ONE such entry in the config */
                    if (foundlocal) {
                        orte_show_help("help-orte-runtime.txt",
                                       "orte_init:startup:internal-failure",
                                       true, "multiple instances of localhost",
                                       ORTE_ERROR_NAME(ORTE_ERR_NOT_SUPPORTED),
                                       ORTE_ERR_NOT_SUPPORTED);
                        return ORTE_ERR_NOT_SUPPORTED;
                    }
                    foundlocal = true;
                    orte_standalone_operation = true;
                    val = orte_process_info.nodename;
                } else if (opal_ifislocal(xml->value[n])) {
                    val = orte_process_info.nodename;
                } else {
                    val = xml->value[n];
                }
                /* add this node */
                node = OBJ_NEW(orcm_node_t);
                node->name = strdup(val);
                node->state = ORTE_NODE_STATE_UNKNOWN;
                OBJ_RETAIN(rack);
                node->rack = rack;
                opal_list_append(&rack->nodes, &node->super);
            }
        }
    } else {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tUNKNOWN TAG: %s", xml->name);
    }
    return ORTE_SUCCESS;
}

static int parse_aggregators(orcm_cfgi_xml_parser_t *xml,
                             orcm_row_t *row)
{
    char *val, **vals;
    int n;
    bool foundlocal = false;

    if (0 == strcmp(xml->name, "port")) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            /* specifies the orcm-port to be used */
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-PORT %s", xml->value[0]);
              agg_cfg.port = strdup(xml->value[0]);
        }
    } else if (0 == strcmp(xml->name, "cores")) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            /* specifies the cores to be used */
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-CORES %s", xml->value[0]);
            /* all we need do is push this into the corresponding param */
            asprintf(&val, OPAL_MCA_PREFIX"orte_daemon_cores=%s", xml->value[0]);
            opal_argv_append_nosize(&agg_cfg.mca_params, val);
            free(val);
        }
    } else if (0 == strcmp(xml->name, "mca-params")) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-MCA-PARAMS %s", xml->value[0]);
            vals = opal_argv_split(xml->value[0], ',');
            if (NULL != vals) {
                for (n=0; NULL != vals[n]; n++) {
                    /* add the MCA prefix, if required */
                    if (0 != strncmp(vals[n], OPAL_MCA_PREFIX, strlen(OPAL_MCA_PREFIX))) {
                        asprintf(&val, OPAL_MCA_PREFIX"%s", vals[n]);
                    } else {
                        val = strdup(vals[n]);
                    }
                    opal_argv_append_nosize(&agg_cfg.mca_params, val);
                    free(val);
                }
                opal_argv_free(vals);
            }
        }
    } else if (0 == strcmp(xml->name, "nodes")) {
        /* save this for later */
        if (NULL != xml->value) {
            for (n=0; NULL != xml->value[n]; n++) {
                if (0 == strcmp(xml->value[n], "localhost")) {
                    /* there can be only ONE such entry in the aggregator config */
                    if (foundlocal) {
                        orte_show_help("help-orte-runtime.txt",
                                       "orte_init:startup:internal-failure",
                                       true, "multiple instances of localhost",
                                       ORTE_ERROR_NAME(ORTE_ERR_NOT_SUPPORTED),
                                       ORTE_ERR_NOT_SUPPORTED);
                        return ORTE_ERR_NOT_SUPPORTED;
                    }
                    foundlocal = true;
                    orte_standalone_operation = true;
                    val = orte_process_info.nodename;
                } else if (opal_ifislocal(xml->value[n])) {
                    val = orte_process_info.nodename;
                } else {
                    val = xml->value[n];
                }
                opal_argv_append_nosize(&aggregators, val);
            }
        }
    } else {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tUNKNOWN TAG: %s", xml->name);
    }
    return ORTE_SUCCESS;
}

static int parse_scheduler(orcm_cfgi_xml_parser_t *xml)
{
    char *val, **vals;
    int n;
    orcm_scheduler_t *scheduler;
    bool foundlocal = false;

    if (0 == strcmp(xml->name, "port")) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            /* specifies the orcm-port to be used */
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-PORT %s", xml->value[0]);
            scd_cfg.port = strdup(xml->value[0]);
        }
    } else if (0 == strcmp(xml->name, "cores")) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            /* specifies the cores to be used */
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-CORES %s", xml->value[0]);
            /* all we need do is push this into the corresponding param */
            asprintf(&val, OPAL_MCA_PREFIX"orte_daemon_cores=%s", xml->value[0]);
            opal_argv_append_nosize(&scd_cfg.mca_params, val);
            free(val);
        }
    } else if (0 == strcmp(xml->name, "mca-params")) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-MCA-PARAMS %s", xml->value[0]);
            vals = opal_argv_split(xml->value[0], ',');
            if (NULL != vals) {
                for (n=0; NULL != vals[n]; n++) {
                    /* add the MCA prefix, if required */
                    if (0 != strncmp(vals[n], OPAL_MCA_PREFIX, strlen(OPAL_MCA_PREFIX))) {
                        asprintf(&val, OPAL_MCA_PREFIX"%s", vals[n]);
                    } else {
                        val = strdup(vals[n]);
                    }
                    opal_argv_append_nosize(&scd_cfg.mca_params, val);
                    free(val);
                }
                opal_argv_free(vals);
            }
        }
    } else if (0 == strcmp(xml->name, "nodes")) {
        /* this is the node entry - it contains an argv-style list of
         * strings identifying the nodes in this group
         */
        if (NULL != xml->value) {
            for (n=0; NULL != xml->value[n]; n++) {
                if (0 == strcmp(xml->value[n], "localhost")) {
                    /* there can be only ONE such entry in the config */
                    if (foundlocal) {
                        orte_show_help("help-orte-runtime.txt",
                                       "orte_init:startup:internal-failure",
                                       true, "multiple instances of localhost",
                                       ORTE_ERROR_NAME(ORTE_ERR_NOT_SUPPORTED),
                                       ORTE_ERR_NOT_SUPPORTED);
                        return ORTE_ERR_NOT_SUPPORTED;
                    }
                    foundlocal = true;
                    orte_standalone_operation = true;
                    val = orte_process_info.nodename;
                } else if (opal_ifislocal(xml->value[n])) {
                    val = orte_process_info.nodename;
                } else {
                    val = xml->value[n];
                }
                /* add this node */
                scheduler = OBJ_NEW(orcm_scheduler_t);
                scheduler->controller.name = strdup(val);
                scheduler->controller.state = ORTE_NODE_STATE_UNKNOWN;
                opal_list_append(orcm_schedulers, &scheduler->super);
            }
        }
    } else {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tUNKNOWN TAG: %s", xml->name);
    }
    return ORTE_SUCCESS;
}

static int cnt=0;

static int parse_configuration(orcm_cfgi_xml_parser_t *xml,
                               orcm_row_t *row)
{
    orcm_cfgi_xml_parser_t *xm;
    int rc;
    orcm_rack_t *rack;

    opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                        "PARSING CONFIGURATION %s", xml->name);

    if (0 == strcmp(xml->name, "configuration")) {
        /* this is the highest level, so just drop down */
        if (!opal_list_is_empty(&xml->subvals)) {
            while (NULL != (xm = (orcm_cfgi_xml_parser_t*)opal_list_remove_first(&xml->subvals))) {
                if (ORTE_SUCCESS != (rc = parse_configuration(xm, row))) {
                    return rc;
                }
            }
        } else {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "Skipping empty configuration");
        }
        return ORTE_SUCCESS;
    }

    if (0 == strcmp(xml->name, "orcm-log")) {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tORCM-LOG %s",
                            (NULL == xml->value) ? "NULL" :
                            ((NULL == xml->value[0]) ? "NULL" : xml->value[0]));
        /* this format specified a common log option for all daemons,
         * so cache it here and we will apply it later
         */
        if (NULL != xml->value && NULL != xml->value[0] &&
            0 < strlen(xml->value[0])) {
            log = strdup(xml->value[0]);
        }
        return ORTE_SUCCESS;
    } else if (0 == strcmp(xml->name, "orcm-daemons")) {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tORCM-DAEMONS");
        /* create a rack for this set of daemons */
        rack = OBJ_NEW(orcm_rack_t);
        asprintf(&rack->name, "A%d", cnt);
        cnt++;
        OBJ_RETAIN(row);
        rack->row = row;
        opal_list_append(&row->racks, &rack->super);
        if (!opal_list_is_empty(&xml->subvals)) {
            while (NULL != (xm = (orcm_cfgi_xml_parser_t*)opal_list_remove_first(&xml->subvals))) {
                if (ORTE_SUCCESS != (rc = parse_daemons(xm, rack))) {
                    return rc;
                }
            }
        } else {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "Skipping empty orcm-daemons");
        }
        return ORTE_SUCCESS;
    } else if (0 == strcmp(xml->name, "orcm-aggregators")) {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tORCM-AGGREGATORS");
        if (!opal_list_is_empty(&xml->subvals)) {
            while (NULL != (xm = (orcm_cfgi_xml_parser_t*)opal_list_remove_first(&xml->subvals))) {
                if (ORTE_SUCCESS != (rc = parse_aggregators(xm, row))) {
                    return rc;
                }
            }
        } else {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "Skipping empty orcm-aggregators");
        }
        return ORTE_SUCCESS;
    } else if (0 == strcmp(xml->name, "orcm-schedulers")) {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tORCM-SCHEDULERS");
        if (!opal_list_is_empty(&xml->subvals)) {
            while (NULL != (xm = (orcm_cfgi_xml_parser_t*)opal_list_remove_first(&xml->subvals))) {
                if (ORTE_SUCCESS != (rc = parse_scheduler(xm))) {
                    return rc;
                }
            }
        } else {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "Skipping empty orcm-schedulers");
        }
        return ORTE_SUCCESS;
    } else {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tUNKNOWN TAG");
    }
    if (!opal_list_is_empty(&xml->subvals)) {
        if (NULL != (xm = (orcm_cfgi_xml_parser_t*)opal_list_remove_first(&xml->subvals))) {
            if (ORTE_SUCCESS != (rc = parse_configuration(xm, row))) {
                return rc;
            }
        }
    } else {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "Skipping empty UNKNOWN TAG");
    }
    return ORTE_SUCCESS;
}

static void setup_environ(char **env)
{
    char **tmp, *t;
    int i;
    opal_output_stream_t lds;

    if (NULL == env) {
        return;
    }

    /* go thru the provided environment and *only* set
     * envars that were not previously set. This allows
     * users to override the config file on the cmd line
     */
    tmp = opal_environ_merge(env, environ);
    if (NULL == tmp) {
        opal_output(orcm_cfgi_base_framework.framework_output,
                    "ERROR: Environment variables merge failed.");
        return;
    }
    /* now cycle thru the result and push MCA params back into our
     * environment. We will overwrite some existing values,
     * but no harm done
     */
    for (i=0; NULL != tmp[i]; i++) {
        if (0 == strncmp(tmp[i], OPAL_MCA_PREFIX, strlen(OPAL_MCA_PREFIX))) {
            t = strdup(tmp[i]);
            if (NULL == t) {
                opal_argv_free(tmp);
                return;
            }
            opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                "PUSHING %s TO ENVIRON", t);
            putenv(t);
        }
    }
    opal_argv_free(tmp);

    if (NULL != log) {
        putenv("OPAL_OUTPUT_REDIRECT=file");
        putenv("OPAL_OUTPUT_SUFFIX=.log");
        opal_output_set_output_file_info(log, orte_process_info.nodename,
                                         NULL, NULL);
        opal_output_reopen(0, NULL);
        OBJ_CONSTRUCT(&lds, opal_output_stream_t);
        lds.lds_want_stdout = true;
        opal_output_reopen(orte_clean_output, &lds);
        OBJ_DESTRUCT(&lds);
    }
}

static bool check_me(orcm_config_t *config, char *node,
                     orte_vpid_t vpid, char *my_ip)
{
    char *uri;

    opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                        "file10:check_me: node %s",
                        (NULL == node) ? "NULL" : node);

    if (NULL == node) {
        opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                            "file10:check_me: node is NULL");
        return false;
    }

    if (opal_net_isaddr(node)) {
        if (0 == strcmp(node, my_ip)) {
            opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                "file10:check_me: node has matching IP");
            ORTE_PROC_MY_NAME->vpid = vpid;
            setup_environ(config->mca_params);
            if (config->aggregator) {
                opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                    "file10:check_me: setting proc type to AGGREGATOR");
                orte_process_info.proc_type = ORCM_AGGREGATOR;
            }
            /* load our port */
            asprintf(&uri, OPAL_MCA_PREFIX"oob_tcp_static_ipv4_ports=%s", config->port);
            putenv(uri);  // cannot free this value
            opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                "file10:check_me: push our port %s", uri);
            return true;
        }
    } else {
        if (0 == strcmp(node, orte_process_info.nodename)) {
            opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                "file10:check_me: names match");
            ORTE_PROC_MY_NAME->vpid = vpid;
            if (config->aggregator) {
                opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                    "file10:check_me: setting proc type to AGGREGATOR");
                orte_process_info.proc_type = ORCM_AGGREGATOR;
            }
            setup_environ(config->mca_params);
            /* load our port */
            asprintf(&uri, OPAL_MCA_PREFIX"oob_tcp_static_ipv4_ports=%s", config->port);
            putenv(uri);  // cannot free this value
            opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                "file10:check_me: push our port %s", uri);
            return true;
        }
    }
    return false;
}

