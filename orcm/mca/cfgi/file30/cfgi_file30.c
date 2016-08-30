/*
 * Copyright (c) 2015-2016      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

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
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif

#include <regex.h>

#include "opal/runtime/opal.h"
#include "opal/util/if.h"
#include "opal/util/net.h"
#include "opal/util/opal_environ.h"
#include "opal/util/os_path.h"
#include "opal/util/show_help.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/parse_options.h"
#include "orte/util/proc_info.h"
#include "orte/util/session_dir.h"
#include "orte/util/name_fns.h"
#include "orte/util/nidmap.h"
#include "orte/util/regex.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_wait.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_quit.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/scd/base/base.h"
#include "orcm/util/utils.h"

#include "orcm/mca/parser/parser.h"
#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/cfgi/file30/cfgi_file30.h"
#include "orcm/mca/cfgi/file30/cfgi_file30_helpers.h"

#define ORCM_MAX_LINE_LENGTH  512

typedef enum error_verbosity_level
{
    V_LO     =  2,
    V_HI     = 10,
    V_HIGHER = 30
} error_verbosity_level_t;

/* API functions */
static int file30_init(void);
static void file30_finalize(void);
static int read_config(opal_list_t *config);
static int define_system(opal_list_t *config,
                         orcm_node_t **mynode,
                         orte_vpid_t *num_procs,
                         opal_buffer_t *buf);

/* The module struct */
orcm_cfgi_base_module_t orcm_cfgi_file30_module = {
    file30_init,
    file30_finalize,
    read_config,
    define_system
};

static char *get_controller_name(char **element_name, char *parent_name);

int fileId;

static int return_error(int ret, const char *err_msg)
{
    opal_output(0, err_msg);
    return ret;
}

static int file30_init(void)
{
    if (NULL == orcm_cfgi_base.config_file) {
        return return_error(ORCM_ERR_TAKE_NEXT_OPTION, "NULL FILE");
    }

    fileId = orcm_parser.open(orcm_cfgi_base.config_file);

    if (0 > fileId) {
        orte_show_help("help-orcm-cfgi.txt", "site-file-not-found",
                       true, orcm_cfgi_base.config_file);
        return return_error(ORCM_ERR_TAKE_NEXT_OPTION, "FAILED TO OPEN XML CFGI FILE");
    }

    opal_list_t *config = orcm_parser.retrieve_section(fileId, TXconfig, NULL);

    if(NULL == config){
        return return_error(ORCM_ERR_TAKE_NEXT_OPTION, "MISSING CONFIGURATION TAG");
    }

    orcm_value_t *version = get_child(config, TXversion);

    if(NULL == version) {
        SAFE_RELEASE_NESTED_LIST(config);
        return return_error(ORCM_ERR_TAKE_NEXT_OPTION, "MISSING VERSION TAG");
    }

    if(false == stringMatchRegex(version->value.data.string, version_REGEX)){
        SAFE_RELEASE_NESTED_LIST(config);
        return return_error(ORCM_ERR_TAKE_NEXT_OPTION, "BAD PARSING OF VERSION DATA");
    }

    if( '3' != version->value.data.string[0]){
        opal_output(0, "EXPECTED VERSION 3 NOT FOUND: %s", version->value.data.string);
        SAFE_RELEASE_NESTED_LIST(config);
        return return_error(ORCM_ERR_TAKE_NEXT_OPTION, "VERSION 3 NOT FOUND");
    }

    SAFE_RELEASE_NESTED_LIST(config);
    return ORCM_SUCCESS;
}

static void file30_finalize(void)
{
    int return_code = orcm_parser.close(fileId);
    if(ORCM_SUCCESS != return_code)
        opal_output(0, "FAIL to PROPERLY CLOSE THE CFGI XML FILE");
}

static bool check_me(orcm_config_t *config, char *node,
                     orte_vpid_t vpid, char *my_ip);

static int read_config(opal_list_t *config)
{
    opal_list_t *cfgi;

    orcm_cfgi_xml_parser_t *xml;

    int erri;

    cfgi = orcm_parser.retrieve_document(fileId);
    if (V_HIGHER < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
        print_pugi_opal_list(cfgi);
    }

    if (ORCM_SUCCESS != check_lex_tags_and_field(cfgi)) {
        SAFE_RELEASE_NESTED_LIST(cfgi);
        return ORCM_ERR_SILENT;
    }

    opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                        "XML lexer-level VALIDITY CHECKED and PASSED");

    if(ORCM_SUCCESS != check_duplicate_singleton(cfgi)){
        SAFE_RELEASE_NESTED_LIST(cfgi);
        return ORCM_ERR_SILENT;
    }

    if( ORCM_SUCCESS != cfgi30_check_configure_hosts_ports(cfgi) ){
        SAFE_RELEASE_NESTED_LIST(cfgi);
        return ORCM_ERR_SILENT;
    }

    erri = convert_to_orcm_cfgi_xml_parser_t_list(cfgi, config);

    SAFE_RELEASE_NESTED_LIST(cfgi);

    if(ORCM_SUCCESS != erri){
        return ORCM_ERR_SILENT;
    }

    if (V_HIGHER < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
        OPAL_LIST_FOREACH(xml, config, orcm_cfgi_xml_parser_t) {
            orcm_util_print_xml(xml, NULL);
        }
    }

    return ORCM_SUCCESS;
}

static void setup_environ(char **env);
static int parse_cluster(orcm_cluster_t *cluster, orcm_cfgi_xml_parser_t *x);
static int parse_scheduler(orcm_scheduler_t *scheduler,
                           orcm_cfgi_xml_parser_t *x);

static void get_node_ip(char *node, char **my_ip)
{
    struct hostent *h = gethostbyname(node);
    if (NULL == h) {
        *my_ip = strdup(node);
    } else {
        *my_ip = strdup(inet_ntoa(*(struct in_addr*)h->h_addr_list[0]));
    }
}

static int define_system(opal_list_t *config,    orcm_node_t **mynode,
                         orte_vpid_t *num_procs, opal_buffer_t *buf)
{
    char *my_ip = NULL;
    orte_vpid_t vpid;
    bool found_me;
    int rc;
    orcm_cfgi_xml_parser_t *x, *xx;
    orcm_cluster_t *cluster;
    orcm_row_t *row;
    orcm_rack_t *rack;
    orcm_node_t *node;
    int32_t num;
    opal_buffer_t uribuf, schedbuf, clusterbuf, rowbuf, rackbuf;
    opal_buffer_t *bptr;
    orcm_scheduler_t *scheduler;

    /* set default */
    *mynode = NULL;

    /* convert my hostname to its IP form just in case we need it */
    get_node_ip(orte_process_info.nodename, &my_ip);
    ORCM_ON_NULL_RETURN_ERROR(my_ip, ORCM_ERR_OUT_OF_RESOURCE);

    OPAL_LIST_FOREACH(xx, config, orcm_cfgi_xml_parser_t) {
        if (0 == strcmp(xx->name, TXconfig)) {
            OPAL_LIST_FOREACH(x, &xx->subvals, orcm_cfgi_xml_parser_t) {
                if (0 == strcmp(x->name, FDcluster)) {
                    /* extract the cluster */
                    cluster = OBJ_NEW(orcm_cluster_t);
                    /* the value is the name of this cluster */
                    if (NULL != x->value && NULL != x->value[0]) {
                        opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                            "CLUSTER NAME: %s", x->value[0]);
                        cluster->name = strdup(x->value[0]);
                    }
                    opal_list_append(orcm_clusters, &cluster->super);
                    if (ORTE_SUCCESS != (rc = parse_cluster(cluster, x))) {
                        SAFEFREE(my_ip);
                        return rc;
                    }
                    if (30 < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
                        opal_dss.dump(0, cluster, ORCM_CLUSTER);
                    }
                } else if (0 == strcmp(x->name, TXscheduler)) {
                    /* extract the scheduler */
                    scheduler = OBJ_NEW(orcm_scheduler_t);
                    opal_list_append(orcm_schedulers, &scheduler->super);
                    if (ORTE_SUCCESS != (rc = parse_scheduler(scheduler, x))) {
                        SAFEFREE(my_ip);
                        return rc;
                    }
                    if (30 < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
                        opal_dss.dump(0, scheduler, ORCM_SCHEDULER);
                    }
                } else {
                    opal_output(0,
                                "\tUNKNOWN TAG %s", x->name);
                    SAFEFREE(my_ip);
                    return ORTE_ERR_BAD_PARAM;
                }
            }
        }
    }

    /* track the vpids for the daemons */
    vpid = 0;
    found_me = false;

    /* cycle thru the list of schedulers - they will take the leading positions
     * in the assignment of process names
     */
    OBJ_CONSTRUCT(&schedbuf, opal_buffer_t);
    OBJ_CONSTRUCT(&uribuf, opal_buffer_t);
    OPAL_LIST_FOREACH(scheduler, orcm_schedulers, orcm_scheduler_t) {
        scheduler->controller.daemon.jobid = 0;
        scheduler->controller.daemon.vpid = vpid;
        /* add to the scheduler definitions */
        opal_dss.pack(&schedbuf, &scheduler->controller.daemon, 1, ORTE_NAME);
        /* scheduler's aren't aggregators, but load their
         * contact info in case needed
         */
        orcm_util_construct_uri(&uribuf, &scheduler->controller);
        if (!found_me && ORTE_PROC_IS_SCHEDULER) {
            found_me = check_me(&scheduler->controller.config,
                                scheduler->controller.name, vpid, my_ip);
            if (found_me) {
                *mynode = &scheduler->controller;
            }
        }
        ++vpid;
    }
    /* transfer the scheduler section to the cluster definition */
    OBJ_CONSTRUCT(&clusterbuf, opal_buffer_t);
    bptr = &schedbuf;
    opal_dss.pack(&clusterbuf, &bptr, 1, OPAL_BUFFER);
    OBJ_DESTRUCT(&schedbuf);

    /* since no scheduler is in the system, let's assume
     * for now that orun will act as the HNP and adjust
     * all the vpid's for the daemons
     */
    if (0 == opal_list_get_size(orcm_schedulers)) {
        vpid = 1;
    }

    /* cycle thru the cluster setting up the remaining names */
    OPAL_LIST_FOREACH(cluster, orcm_clusters, orcm_cluster_t) {
        if (ORTE_NODE_STATE_UNDEF != cluster->controller.state) {
            /* the cluster includes a system controller node */
            cluster->controller.daemon.jobid = 0;
            cluster->controller.daemon.vpid = vpid;
            if (cluster->controller.config.aggregator) {
                orcm_util_construct_uri(&uribuf, &cluster->controller);
            }
            if (!found_me && ORTE_PROC_IS_DAEMON) {
                found_me = check_me(&cluster->controller.config,
                                    cluster->controller.name, vpid, my_ip);
                if (found_me) {
                    *mynode = &cluster->controller;
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
        /* pack the cluster controller */
        opal_dss.pack(&clusterbuf, &cluster->controller.daemon, 1, ORTE_NAME);
        /* pack the number of rows */
        num = opal_list_get_size(&cluster->rows);
        opal_dss.pack(&clusterbuf, &num, 1, OPAL_INT32);
        OPAL_LIST_FOREACH_REV(row, &cluster->rows, orcm_row_t) {
            OBJ_CONSTRUCT(&rowbuf, opal_buffer_t);
            /* pack the number of racks */
            num = opal_list_get_size(&row->racks);
            opal_dss.pack(&rowbuf, &num, 1, OPAL_INT32);
            if (ORTE_NODE_STATE_UNDEF != row->controller.state) {
                /* the row includes a controller */
                row->controller.daemon.jobid = 0;
                row->controller.daemon.vpid = vpid;
                if (row->controller.config.aggregator) {
                    orcm_util_construct_uri(&uribuf, &row->controller);
                }
                if (!found_me && ORTE_PROC_IS_DAEMON) {
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
                        if (ORTE_NODE_STATE_UNDEF != cluster->controller.state) {
                           /* define my DAEMON to be the cluster, if available */
                           ORTE_PROC_MY_DAEMON->jobid = cluster->controller.daemon.jobid;
                           ORTE_PROC_MY_DAEMON->vpid = cluster->controller.daemon.vpid;
                        } else {
                           /* otherwise, it is just myself */
                           ORTE_PROC_MY_DAEMON->jobid = ORTE_PROC_MY_NAME->jobid;
                           ORTE_PROC_MY_DAEMON->vpid = ORTE_PROC_MY_NAME->vpid;
                        }

                    }
                }
                ++vpid;
            }
            /* pack the name of the row controller, which will be invalid
             * if we don't have one in the config */
            opal_dss.pack(&rowbuf, &row->controller.daemon, 1, ORTE_NAME);
            OPAL_LIST_FOREACH_REV(rack, &row->racks, orcm_rack_t) {
                OBJ_CONSTRUCT(&rackbuf, opal_buffer_t);
                if (ORTE_NODE_STATE_UNDEF != rack->controller.state) {
                    /* the rack includes a controller */
                    rack->controller.daemon.jobid = 0;
                    rack->controller.daemon.vpid = vpid;
                    if (rack->controller.config.aggregator) {
                        orcm_util_construct_uri(&uribuf, &rack->controller);
                    }
                    if (!found_me && ORTE_PROC_IS_DAEMON) {
                        found_me = check_me(&rack->controller.config,
                                            rack->controller.name, vpid, my_ip);
                        if (found_me) {
                            *mynode = &rack->controller;
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

                            if (ORTE_NODE_STATE_UNDEF != row->controller.state) {
                               ORTE_PROC_MY_DAEMON->jobid = row->controller.daemon.jobid;
                               ORTE_PROC_MY_DAEMON->vpid = row->controller.daemon.vpid;
                            } else if (ORTE_NODE_STATE_UNDEF != cluster->controller.state) {
                               ORTE_PROC_MY_DAEMON->jobid = cluster->controller.daemon.jobid;
                               ORTE_PROC_MY_DAEMON->vpid = cluster->controller.daemon.vpid;
                            } else {
                               /* otherwise, it is just myself */
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
                    node->daemon.jobid = 0;
                    node->daemon.vpid = vpid;
                    if (!found_me && ORTE_PROC_IS_DAEMON) {
                        found_me = check_me(&node->config, node->name, vpid, my_ip);
                        if (found_me) {
                            *mynode = node;
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

                            if (ORTE_NODE_STATE_UNDEF != rack->controller.state) {
                               ORTE_PROC_MY_DAEMON->jobid = rack->controller.daemon.jobid;
                               ORTE_PROC_MY_DAEMON->vpid = rack->controller.daemon.vpid;
                            }  else if (ORTE_NODE_STATE_UNDEF != row->controller.state) {
                               ORTE_PROC_MY_DAEMON->jobid = row->controller.daemon.jobid;
                               ORTE_PROC_MY_DAEMON->vpid = row->controller.daemon.vpid;
                            } else if (ORTE_NODE_STATE_UNDEF != cluster->controller.state) {
                               ORTE_PROC_MY_DAEMON->jobid = cluster->controller.daemon.jobid;
                               ORTE_PROC_MY_DAEMON->vpid = cluster->controller.daemon.vpid;
                            } else {
                               /* otherwise, it is just myself */
                               ORTE_PROC_MY_DAEMON->jobid = ORTE_PROC_MY_NAME->jobid;
                               ORTE_PROC_MY_DAEMON->vpid = ORTE_PROC_MY_NAME->vpid;
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
    }

    if (20 < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
        OPAL_LIST_FOREACH(scheduler, orcm_schedulers, orcm_scheduler_t) {
            opal_dss.dump(0, scheduler, ORCM_SCHEDULER);
        }
        OPAL_LIST_FOREACH(cluster, orcm_clusters, orcm_cluster_t) {
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
    SAFEFREE(my_ip);

    return ORCM_SUCCESS;
}

static bool check_me(orcm_config_t *config, char *node,
                     orte_vpid_t vpid, char *my_ip)
{
    char *uri = NULL;
    bool node_itself = false;
    int rc = ORCM_SUCCESS;

    if( NULL != node && (0 == strcmp(node, orte_process_info.nodename)
        || 0 == strcmp(node, my_ip)) ){

        opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                            "file30:check_me: node '%s' matches", node);

        if( config->aggregator ){
            orte_process_info.proc_type = ORCM_AGGREGATOR;
        }

        ORTE_PROC_MY_NAME->vpid = vpid;
        setup_environ(config->mca_params);
        // load our port
        if( -1 != asprintf(&uri,
                           OPAL_MCA_PREFIX"oob_tcp_static_ipv4_ports=%s",
                           config->port) ){
            putenv(uri);  // cannot free this value
            opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                "push our port %s", uri);

            if( ORCM_SUCCESS == (rc = orcm_set_proc_hostname(node)) ){
                node_itself = true;
            } else {
                ORTE_ERROR_LOG(rc);
            }
        }
    }

    return node_itself;
}

static int parse_orcm_config(orcm_config_t *cfg,
                             orcm_cfgi_xml_parser_t *xml)
{
    char *val, **vals;
    int n;

    if (0 == strcmp(xml->name, TXport)) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            /* specifies the orcm-port to be used */
            opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-PORT %s", xml->value[0]);
            cfg->port = strdup(xml->value[0]);
        }
    } else if (0 == strcmp(xml->name, TXcores)) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            /* specifies the cores to be used */
            opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                "\tCORES %s", xml->value[0]);
            /* all we need do is push this into the corresponding param */
            asprintf(&val, OPAL_MCA_PREFIX"orte_daemon_cores=%s", xml->value[0]);
            if (OPAL_SUCCESS != opal_argv_append_nosize(&cfg->mca_params, val)) {
                free(val);
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            free(val);
        }
    } else if (0 == strcmp(xml->name, TXmca_params)) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-MCA-PARAMS %s", xml->value[0]);
            vals = opal_argv_split(xml->value[0], ',');
            if (NULL != vals) {
                for (n=0; NULL != vals[n]; n++) {
                    /* add the MCA prefix, if required */
                    if (0 != strncmp(vals[n], OPAL_MCA_PREFIX, strlen(OPAL_MCA_PREFIX))) {
                        asprintf(&val, OPAL_MCA_PREFIX"%s", vals[n]);
                    } else {
                        val = strdup(vals[n]);
                        if (NULL == val) {
                            opal_argv_free(vals);
                            return ORCM_ERR_OUT_OF_RESOURCE;
                        }
                    }
                    if (OPAL_SUCCESS != opal_argv_append_nosize(&cfg->mca_params, val)) {
                        free(val);
                        opal_argv_free(vals);
                        return ORCM_ERR_OUT_OF_RESOURCE;
                    }
                    free(val);
                }
                opal_argv_free(vals);
            }
        }
    } else if (0 == strcmp(xml->name, TXlog)) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-LOG %s", xml->value[0]);
            opal_setenv("OPAL_OUTPUT_REDIRECT", "file", true, &cfg->env);
            opal_setenv("OPAL_OUTPUT_LOCATION", xml->value[0], true, &cfg->env);
            opal_setenv("OPAL_OUTPUT_SUFFIX", ".log", true, &cfg->env);
        }
    } else if  (0 == strcmp(xml->name, TXenvar)) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                "\tENVARS %s", xml->value[0]);
            vals = opal_argv_split(xml->value[0], ',');
            if (NULL != vals) {
                for (n=0; NULL != vals[n]; n++) {
                    if (OPAL_SUCCESS != opal_argv_append_nosize(&cfg->env, vals[n])) {
                        opal_argv_free(vals);
                        return ORCM_ERR_OUT_OF_RESOURCE;
                    }
                }
                opal_argv_free(vals);
            }
        }
    } else if  (0 == strcmp(xml->name, TXaggregat)) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                "\tAGGREGATOR %s", xml->value[0]);
            if (0 == strncmp("y", xml->value[0], 1)) {
                cfg->aggregator = true;
            } else {
                cfg->aggregator = false;
            }
        } else {
            cfg->aggregator = true;
        }
    } else {
        opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                            "\tUNKNOWN TAG");
    }
    return ORTE_SUCCESS;
}

static int parse_node(orcm_node_t *node, int idx, orcm_cfgi_xml_parser_t *x)
{
    int rc;
    char *tmp = NULL;

    if (0 == strcmp(x->name, TXname)) {
        /* the value contains the node name, or an expression
         * whereby we replace any leading @ with the rack name and any
         * # with the node number (the number of # equals the
         * number of digits to be used)
         */
        if (NULL == x->value || NULL == x->value[0]) {
            /* that's an error */
            ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
            return ORCM_ERR_BAD_PARAM;
        }

        tmp = get_controller_name(&x->value[0], NULL);
        if (NULL == tmp) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        node->name = strdup(tmp);
        if (NULL == node->name) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }

    } else {
        if (ORCM_SUCCESS != (rc = parse_orcm_config(&node->config, x))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }
    return ORCM_SUCCESS;
}

static int parse_rack(orcm_rack_t *rack, int idx, orcm_cfgi_xml_parser_t *x)
{
    int n, rc;
    orcm_cfgi_xml_parser_t *xx;
    orcm_node_t *node, *nd;
    char *tmp = NULL;
    char **vals = NULL;
    char **names = NULL;


    if (0 == strcmp(x->name, TXcontrol)) {
        /* the value contains the node name of the controller, or an expression
         * whereby we replace any leading @ with the row name and any
         * # with the rack number (the number of # equals the
         * number of digits to be used)
         */
        if (NULL == x->value || NULL == x->value[0]) {
            /* that's an error */
            ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
            return ORCM_ERR_BAD_PARAM;
        }

        tmp = get_controller_name(&x->value[0], rack->name);
        if (NULL == tmp) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        rack->controller.name = strdup(tmp);
        if (NULL == rack->controller.name) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }

        rack->controller.state = ORTE_NODE_STATE_UNKNOWN;
        /* parse any config that is attached to the rack controller */
        OPAL_LIST_FOREACH(xx, &x->subvals, orcm_cfgi_xml_parser_t) {
            if (ORCM_SUCCESS != (rc = parse_orcm_config(&rack->controller.config, xx))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
        }
    } else if (0 == strcmp(x->name, FDnode)) {
        /* the value contains either a single name, or a regular expression
         * defining multiple nodes.
         */
        if (NULL == x->value || NULL == x->value[0]) {
            /* that's an error */
            ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
            return ORCM_ERR_BAD_PARAM;
        }
        opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                            "\tNODE NAME %s", x->value[0]);
        /* define the nodes */
        vals = opal_argv_split(x->value[0], ',');
        if (NULL != vals) {
            for (n=0; NULL != vals[n]; n++) {
                names = NULL;
                if (ORTE_SUCCESS != (rc = orte_regex_extract_node_names(vals[n], &names))) {
                    opal_argv_free(vals);
                    opal_argv_free(names);
                    return rc;
                }
                if (NULL == names) {
                    /* that's an error */
                    opal_argv_free(vals);
                    return ORCM_ERR_BAD_PARAM;
                }
                /* see if we have each racks object - it not, create it */
                int sz_names = opal_argv_count(names);
                int m=0;
                for (m=0; m < sz_names; m++) {
                    if (NULL == names[m]) {
                        continue;
                    }
                    get_controller_name(&names[m], rack->name);
                    node = NULL;
                    OPAL_LIST_FOREACH(nd, &rack->nodes, orcm_node_t) {
                        if (0 == strcmp(nd->name, names[m])) {
                            node = nd;
                            break;
                        }
                    }
                    if (NULL == node) {
                        opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                            "\tNEW NODE NAME %s", names[m]);
                        node = OBJ_NEW(orcm_node_t);
                        node->name = strdup(names[m]);
                        OBJ_RETAIN(rack);
                        node->rack = rack;
                        node->state = ORTE_NODE_STATE_UNKNOWN;
                        opal_list_append(&rack->nodes, &node->super);
                    }
                    /* now cycle thru the rest of this config element and apply
                     * those values to this node
                     */
                    OPAL_LIST_FOREACH(xx, &x->subvals, orcm_cfgi_xml_parser_t) {
                        if (ORCM_SUCCESS != (rc = parse_node(node, 0, xx))) {
                            ORTE_ERROR_LOG(rc);
                            opal_argv_free(names);
                            opal_argv_free(vals);
                            return rc;
                        }
                    }
                }
                opal_argv_free(names);
            }
            opal_argv_free(vals);
        }
    } else {
        opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                            "\tUNKNOWN TAG");
    }
    return ORCM_SUCCESS;
}

static int parse_row(orcm_row_t *row, orcm_cfgi_xml_parser_t *x)
{
    int n, rc;
    orcm_cfgi_xml_parser_t *xx;
    orcm_rack_t *rack, *r;
    char *tmp = NULL;
    char **vals = NULL;
    char **names = NULL;

    if (0 == strcmp(x->name, TXcontrol)) {
        /* the value contains the node name of the controller, or an expression
         * whereby we replace any leading or trailing # with the row name
         */
        if (NULL == x->value || NULL == x->value[0]) {
            /* that's an error */
            ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
            return ORCM_ERR_BAD_PARAM;
        }

        tmp = get_controller_name(&x->value[0], row->name);
        if (NULL == tmp) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        row->controller.name = strdup(tmp);
        if (NULL == row->controller.name) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }

        row->controller.state = ORTE_NODE_STATE_UNKNOWN;

        /* parse any subvals that are attached to the row controller */
        OPAL_LIST_FOREACH(xx, &x->subvals, orcm_cfgi_xml_parser_t) {
            if (ORCM_SUCCESS != (rc = parse_orcm_config(&row->controller.config, xx))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
        }
    } else if (0 == strcmp(x->name, FDrack)) {
        /* the value contains either a single name, or a regular expression
         * defining multiple racks.
         */
        if (NULL == x->value || NULL == x->value[0]) {
            /* that's an error */
            ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
            return ORTE_ERR_BAD_PARAM;
        }
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tRACK NAME %s", x->value[0]);
        /* define the racks */
        vals = opal_argv_split(x->value[0], ',');
        if (NULL != vals) {
            for (n = 0; NULL !=vals[n]; ++n) {
                names = NULL;
                if (ORTE_SUCCESS != (rc = orte_regex_extract_node_names(vals[n], &names))) {
                    opal_argv_free(vals);
                    opal_argv_free(names);
                    return rc;
                }
                if (NULL == names) {
                    opal_argv_free(vals);
                    return ORCM_ERR_BAD_PARAM;
                }
                /*if we have each rack object - if not, create it */
                int sz_names = opal_argv_count(names);
                int m;
                for (m=0; m < sz_names; m++) {
                    if (NULL == names[m]) {
                        continue;
                    }

                    get_controller_name(&names[m], row->name);
                    rack = NULL;
                    OPAL_LIST_FOREACH(r, &row->racks, orcm_rack_t) {
                        if (0 == strcmp(r->name, names[m])) {
                            rack = r;
                            break;
                        }
                    }
                    if (NULL == rack) {
                        opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                            "\tNEW RACK NAME %s", names[m]);
                        rack = OBJ_NEW(orcm_rack_t);
                        rack->name = strdup(names[m]);

                        OBJ_RETAIN(row);
                        rack->row = row;
                        opal_list_append(&row->racks, &rack->super);
                    }
                    /* now cycle thru the rest of this config element and apply
                     * those values to this rack
                     */
                    OPAL_LIST_FOREACH(xx, &x->subvals, orcm_cfgi_xml_parser_t) {
                        if (ORCM_SUCCESS != (rc = parse_rack(rack, 0, xx))) {
                            ORTE_ERROR_LOG(rc);
                            opal_argv_free(vals);
                            opal_argv_free(names);
                            return rc;
                        }
                    }
                }
                opal_argv_free(names);
            }
            opal_argv_free(vals);
        }
    } else {
        opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                            "\tUNKNOWN TAG");
    }
    return ORCM_SUCCESS;
}

static int parse_cluster(orcm_cluster_t *cluster,
                         orcm_cfgi_xml_parser_t *xx)
{
    orcm_cfgi_xml_parser_t *x, *y;
    int rc, n;
    orcm_row_t *r, *row;
    char *tmp = NULL;
    char **vals = NULL;
    char **names = NULL;

    int only_one_cluster_controller=0;

    OPAL_LIST_FOREACH(x, &xx->subvals, orcm_cfgi_xml_parser_t) {
        if ( 0 == strcmp(x->name, TXcontrol)) {
            /* the value contains the node name of the controller, or an expression
             * whereby we replace any leading or trailing # with the row name
             */
            if (NULL == x->value || NULL == x->value[0]) {
                /* that's an error */
                ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                return ORCM_ERR_BAD_PARAM;
            }

            get_controller_name(&x->value[0], cluster->name);

            if (0 == only_one_cluster_controller) {
                tmp = get_controller_name(&x->value[0], NULL);
                if (NULL == tmp) {
                    ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
                    return ORCM_ERR_OUT_OF_RESOURCE;
                }
                cluster->controller.name = strdup(tmp);
                if (NULL == cluster->controller.name) {
                    ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
                    return ORCM_ERR_OUT_OF_RESOURCE;
                }
                cluster->controller.state = ORTE_NODE_STATE_UNKNOWN;
                ++only_one_cluster_controller;
            }
            /* parse any subvals that are attached to the cluster controller */

            orcm_cfgi_xml_parser_t * z = NULL;
            OPAL_LIST_FOREACH(z, &x->subvals, orcm_cfgi_xml_parser_t) {
                if (ORCM_SUCCESS != (rc = parse_orcm_config(&cluster->controller.config, z))) {
                    ORTE_ERROR_LOG(rc);
                    return rc;
                }
            }
        } else if (0 == strcmp(x->name, FDrow)) {
            /* the value tells us either the name of the row, or a regular expression
             * describing the names of the rows
             */
            if (NULL == x->value || NULL == x->value[0]) {
                /* that's an error */
                ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
                return ORTE_ERR_BAD_PARAM;
            }
            /* regular expression defining the names of the rows */
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tROW NAME %s", x->value[0]);
            /* define the rows */
            vals = opal_argv_split(x->value[0], ',');
            if (NULL != vals) {
                for (n=0; NULL != vals[n]; n++) {
                    names = NULL;
                    if (ORTE_SUCCESS != (rc = orte_regex_extract_node_names(vals[n], &names))) {
                        opal_argv_free(vals);
                        opal_argv_free(names);
                        return rc;
                    }
                    if (NULL == names) {
                        /* that's an error */
                        opal_argv_free(vals);
                        return ORCM_ERR_BAD_PARAM;
                    }
                    /* see if we have each row object - if not, create it */
                    int m;
                    for (m=0; NULL != names[m]; m++) {
                        get_controller_name(&names[m], cluster->name);
                        row = NULL;
                        OPAL_LIST_FOREACH(r, &cluster->rows, orcm_row_t) {
                            if (0 == strcmp(r->name, names[m])) {
                                row = r;
                                break;
                            }
                        }
                        if (NULL == row) {
                            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                                "\tNEW ROW NAME %s", names[m]);
                            row = OBJ_NEW(orcm_row_t);
                            OBJ_RETAIN(cluster);
                            row->cluster = cluster;
                            row->name = strdup(names[m]);
                            opal_list_append(&cluster->rows, &row->super);
                        }
                        /* now cycle thru the rest of this config element and apply
                         * those values to this row
                         */
                        OPAL_LIST_FOREACH(y, &x->subvals, orcm_cfgi_xml_parser_t) {
                            if (ORCM_SUCCESS != (rc = parse_row(row, y))) {
                                ORTE_ERROR_LOG(rc);
                                opal_argv_free(vals);
                                opal_argv_free(names);
                                return rc;
                            }
                        }
                    }
                    opal_argv_free(names);
                }
                opal_argv_free(vals);
            }
        } else {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tUNKNOWN TAG");
        }
    }
    return ORCM_SUCCESS;
}

static int parse_scheduler(orcm_scheduler_t *scheduler,
                         orcm_cfgi_xml_parser_t *xx)
{
    char *tmp = NULL;
    orcm_cfgi_xml_parser_t *x;
    int rc, n;

    bool only_one_scheduler = true;

    OPAL_LIST_FOREACH(x, &xx->subvals, orcm_cfgi_xml_parser_t) {
        if (0 == strcmp(x->name, TXqueues)) {
            /* the value tells us either the name of the row, or a regular expression
             * describing the names of the rows
             */
            if (NULL == x->value || NULL == x->value[0]) {
                /* that's an error */
                ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                return ORCM_ERR_BAD_PARAM;
            }
            /* each value in the argv-array defines a queue, so just
             * pass them back to the scheduler
             */
            if (NULL != x->value) {
                for (n=0; NULL != x->value[n]; n++) {
                    if(ORTE_SUCCESS!=opal_argv_append_nosize(&scheduler->queues, x->value[n])){
                        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
                        return ORCM_ERR_OUT_OF_RESOURCE;
                    }
                }
            }
        } else if (0 == strcmp(x->name, FDnode)) {
            /* defines the name of the node where the scheduler is running */
            if (NULL == x->value || NULL == x->value[0]) {
                /* that's an error */
                ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                return ORCM_ERR_BAD_PARAM;
            }
            if (only_one_scheduler) {
                tmp = get_controller_name(&x->value[0], NULL);
                if (NULL == tmp) {
                    ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
                    return ORCM_ERR_OUT_OF_RESOURCE;
                }
                scheduler->controller.name = strdup(tmp);
                if (NULL == scheduler->controller.name) {
                    ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
                    return ORCM_ERR_OUT_OF_RESOURCE;
                }
                only_one_scheduler = false;
            }
        } else {
            if (ORCM_SUCCESS != (rc = parse_orcm_config(&scheduler->controller.config, x))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
        }
    }
    return ORCM_SUCCESS;
}

static void setup_environ(char **env)
{
    char **tmp, *t;
    int i;

    if (NULL == env) {
        return;
    }

    /* go thru the provided environment and *only* set
     * envars that were not previously set. This allows
     * users to override the config file on the cmd line
     */
    tmp = opal_environ_merge(env, environ);
    if (NULL == tmp) {
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
}

static char *get_controller_name(char **element_name, char *parent_name)
{
    char *at_sign = NULL;
    int at_sign_pos = 0;
    char *final_name = NULL;

    if (NULL != *element_name)
    {
        if(0 == strcmp(*element_name, "localhost")) {
            free(*element_name);
            *element_name = strdup(orte_process_info.nodename);
        } else if(NULL != parent_name && NULL != (at_sign = strchr(*element_name,'@'))){
            final_name = (char*) calloc(strlen(*element_name) + strlen(parent_name) + 1, sizeof(char));
            at_sign_pos = (int)(at_sign - *element_name);

            if(NULL != final_name){
                memcpy(final_name,*element_name,(size_t)at_sign_pos);
                final_name[at_sign_pos + 1] = '\x0';
                strcat(final_name, parent_name);
                strcat(final_name, (*element_name + at_sign_pos + 1));
                free(*element_name);
                *element_name = final_name;
            }
        }
    }

    return *element_name;
}
