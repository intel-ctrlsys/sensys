/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
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

#include "opal/dss/dss.h"
#include "opal/mca/db/base/base.h"
#include "opal/mca/event/event.h"
#include "opal/runtime/opal.h"
#include "opal/runtime/opal_cr.h"
#include "opal/mca/hwloc/base/base.h"
#include "opal/mca/pstat/base/base.h"
#include "opal/util/arch.h"
#include "opal/util/if.h"
#include "opal/util/net.h"
#include "opal/util/os_path.h"
#include "opal/util/show_help.h"

#include "orte/mca/ess/base/base.h"
#include "orte/mca/rml/base/base.h"
#include "orte/mca/rml/base/rml_contact.h"
#include "orte/mca/routed/base/base.h"
#include "orte/mca/routed/routed.h"
#include "orte/mca/oob/base/base.h"
#include "orte/mca/dfs/base/base.h"
#include "orte/mca/grpcomm/grpcomm.h"
#include "orte/mca/grpcomm/base/base.h"
#include "orte/mca/iof/base/base.h"
#include "orte/mca/plm/base/base.h"
#include "orte/mca/odls/base/base.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/filem/base/base.h"
#include "orte/util/parse_options.h"
#include "orte/util/proc_info.h"
#include "orte/util/session_dir.h"
#include "orte/util/name_fns.h"
#include "orte/util/nidmap.h"
#include "orte/util/regex.h"
#include "orte/util/show_help.h"
#include "orte/mca/errmgr/base/base.h"
#include "orte/mca/state/base/base.h"
#include "orte/mca/state/state.h"
#include "orte/runtime/orte_cr.h"
#include "orte/runtime/orte_wait.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_quit.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/scd/base/base.h"
#include "orcm/util/utils.h"

#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/cfgi/file20/cfgi_file20.h"

#define ORCM_MAX_LINE_LENGTH  512

/* API functions */

static int file20_init(void);
static void file20_finalize(void);
static int read_config(opal_list_t *config);
static int define_system(opal_list_t *config,
                         orcm_node_t **mynode,
                         orte_vpid_t *num_procs,
                         opal_buffer_t *buf);

/* The module struct */

orcm_cfgi_base_module_t orcm_cfgi_file20_module = {
    file20_init,
    file20_finalize,
    read_config,
    define_system
};

static char *orcm_getline(FILE *fp);

static int file20_init(void)
{
    FILE *fp;
    char *line, *ptr;

    /* if a file is available, open it and read the first
     * line - if it is version 2.x, then that is us
     */
    if (NULL == orcm_cfgi_base.config_file) {
        opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                            "NULL FILE");
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }
    if (NULL == (fp = fopen(orcm_cfgi_base.config_file, "r"))) {
        opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                            "CANT OPEN %s", orcm_cfgi_base.config_file);
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }
    if (NULL == (line = orcm_getline(fp))) {
        opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                            "FAILED TO GET LINE");
        fclose(fp);
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }
    if (NULL == (ptr = strchr(line, '='))) {
        opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                            "NOT FIND =");
        free(line);
        fclose(fp);
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }
    ptr++; // points to the quote
    ptr++; // points to the version number
    if ('2' != *ptr) {
        opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                            "NOT FOUND 2: %s", ptr);
        free(line);
        fclose(fp);
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }
    free(line);
    fclose(fp);

    return ORCM_SUCCESS;
}

static void file20_finalize(void)
{
}

static void parse_config(FILE *fp,
                         char *filename,
                         opal_list_t *config,
                         orcm_cfgi_xml_parser_t *xml_in);
static bool check_me(orcm_config_t *config, char *node,
                     orte_vpid_t vpid, char *my_ip);

static int read_config(opal_list_t *config)
{
    FILE *fp;
    orcm_cfgi_xml_parser_t *xml;

    /* open and parse the config file - this file contains a
     * list of all the nodes in this orcm "group", a groupID
     * that we will use as our jobid, and the static port(s)
     * assigned to orcm.
     */

    fp = fopen(orcm_cfgi_base.config_file, "r");
    if (NULL == fp) {
        opal_show_help("help-orcm-cfgi.txt", "site-file-not-found", true, orcm_cfgi_base.config_file);
        return ORCM_ERR_SILENT;
    }
    parse_config(fp, orcm_cfgi_base.config_file, config, NULL);
    fclose(fp);

    /* print it out, if requested */
    if (30 < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
        OPAL_LIST_FOREACH(xml, config, orcm_cfgi_xml_parser_t) {
            orcm_util_print_xml(xml, NULL);
        }
    }

    return ORCM_SUCCESS;
}

static void setup_environ(char **env);
static int parse_cluster(orcm_cluster_t *cluster,
                         orcm_cfgi_xml_parser_t *x);
static int parse_scheduler(orcm_scheduler_t *scheduler,
                           orcm_cfgi_xml_parser_t *x);

static int define_system(opal_list_t *config,
                         orcm_node_t **mynode,
                         orte_vpid_t *num_procs,
                         opal_buffer_t *buf)
{
    struct hostent *h;
    char *my_ip;
    orte_vpid_t vpid;
    bool found_me;
    int rc;
    orcm_cfgi_xml_parser_t *x, *xx;
    orcm_cluster_t *cluster;
    orcm_scheduler_t *scheduler;
    orcm_row_t *row;
    orcm_rack_t *rack;
    orcm_node_t *node;

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

    OPAL_LIST_FOREACH(xx, config, orcm_cfgi_xml_parser_t) {
        if (0 == strcmp(xx->name, "configuration")) {
            OPAL_LIST_FOREACH(x, &xx->subvals, orcm_cfgi_xml_parser_t) {
                if (0 == strcmp(x->name, "cluster")) {
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
                        return rc;
                    }
                    if (30 < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
                        opal_dss.dump(0, cluster, ORCM_CLUSTER);
                    }
                } else if (0 == strcmp(x->name, "scheduler")) {
                    /* extract the scheduler */
                    scheduler = OBJ_NEW(orcm_scheduler_t);
                    opal_list_append(orcm_schedulers, &scheduler->super);
                    if (ORTE_SUCCESS != (rc = parse_scheduler(scheduler, x))) {
                        return rc;
                    }
                    if (30 < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
                        opal_dss.dump(0, scheduler, ORCM_SCHEDULER);
                    }
                } else {
                    opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                        "\tUNKNOWN TAG %s", x->name);
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
    OPAL_LIST_FOREACH(scheduler, orcm_schedulers, orcm_scheduler_t) {
        scheduler->controller.daemon.jobid = 0;
        scheduler->controller.daemon.vpid = vpid;
        /* scheduler's aren't aggregators, but load their
         * contact info in case needed
         */
        orcm_util_construct_uri(buf, &scheduler->controller);
        if (!found_me) {
            found_me = check_me(&scheduler->controller.config,
                                scheduler->controller.name, vpid, my_ip);
            if (found_me) {
                *mynode = &scheduler->controller;
            }
        }
        ++vpid;
    }

    /* cycle thru the cluster setting up the remaining names */
    OPAL_LIST_FOREACH(cluster, orcm_clusters, orcm_cluster_t) {
        if (ORTE_NODE_STATE_UNDEF != cluster->controller.state) {
            /* the cluster includes a system controller node */
            cluster->controller.daemon.jobid = 0;
            cluster->controller.daemon.vpid = vpid;
            if (cluster->controller.config.aggregator) {
                orcm_util_construct_uri(buf, &cluster->controller);
            }
            if (!found_me) {
                found_me = check_me(&cluster->controller.config,
                                    cluster->controller.name, vpid, my_ip);
                if (found_me) {
                    *mynode = &cluster->controller;
                }
            }
            ++vpid;
        }
        OPAL_LIST_FOREACH(row, &cluster->rows, orcm_row_t) {
            if (ORTE_NODE_STATE_UNDEF != row->controller.state) {
                /* the row includes a controller */
                row->controller.daemon.jobid = 0;
                row->controller.daemon.vpid = vpid;
                if (row->controller.config.aggregator) {
                    orcm_util_construct_uri(buf, &row->controller);
                }
                if (!found_me) {
                    found_me = check_me(&row->controller.config,
                                        row->controller.name, vpid, my_ip);
                    if (found_me) {
                        *mynode = &row->controller;
                    }
                }
                ++vpid;
            }
            OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
                if (ORTE_NODE_STATE_UNDEF != rack->controller.state) {
                    /* the rack includes a controller */
                    rack->controller.daemon.jobid = 0;
                    rack->controller.daemon.vpid = vpid;
                    if (rack->controller.config.aggregator) {
                        orcm_util_construct_uri(buf, &rack->controller);
                    }
                    if (!found_me) {
                        found_me = check_me(&rack->controller.config,
                                            rack->controller.name, vpid, my_ip);
                        if (found_me) {
                            *mynode = &rack->controller;
                        }
                    }
                    ++vpid;
                }
                OPAL_LIST_FOREACH(node, &rack->nodes, orcm_node_t) {
                    node->daemon.jobid = 0;
                    node->daemon.vpid = vpid;
                    if (!found_me) {
                        found_me = check_me(&node->config, node->name, vpid, my_ip);
                        if (found_me) {
                            *mynode = node;
                        }
                    }
                    ++vpid;
                }
            }
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

    return ORTE_SUCCESS;
}

static char *orcm_getline(FILE *fp)
{
    char *ret, *buff, *ptr;
    char input[ORCM_MAX_LINE_LENGTH];
    size_t i;

    ret = fgets(input, ORCM_MAX_LINE_LENGTH, fp);
    if (NULL != ret) {
	   input[strlen(input)-1] = '\0';  /* remove newline */
           /* strip leading spaces */
           ptr = input;
           for (i=0; i < strlen(input)-1; i++) {
               if (' ' != input[i]) {
                   ptr = &input[i];
                   break;
               }
           }
	   buff = strdup(ptr);
	   return buff;
    }
    
    return NULL;
}

static char* extract_tag(char *filename, char *line)
{
    char *start, *end;
    char *tmp, *ret;

    tmp = strdup(line);
    /* find the start of the tag */
    start = strchr(tmp, '<');
    if (NULL == start) {
        fprintf(stderr, "Error parsing tag in configuration file %s: xml format error in %s\n",
                filename, line);
        return NULL;
    }
    start++;
    /* if it is the end character, skip over */
    if ('/' == *start) {
        start++;
    }
    /* find the end of the tag */
    end = strchr(start, '>');
    if (NULL == end) {
        fprintf(stderr, "Error parsing tag in configuration file %s: xml format error in %s\n",
                filename, line);
        return NULL;
    }
    *end = '\0';
    /* pass it back */
    ret = strdup(start);
    free(tmp);
    return ret;
}

static char* extract_value(char *filename, char *line)
{
    char *start, *end;

    /* find the start of the value */
    start = strchr(line, '>');
    if (NULL == start) {
        fprintf(stderr, "Error parsing value in configuration file %s: xml format error in %s\n",
                filename, line);
        return NULL;
    }
    start++;
    /* find the end of the value */
    end = strchr(start, '<');
    if (NULL == end) {
        fprintf(stderr, "Error parsing value in configuration file %s: xml format error in %s\n",
                filename, line);
        return NULL;
    }
    *end = '\0';
    /* pass it back */
    return start;
}

static void parse_config(FILE *fp,
                         char *filename,
                         opal_list_t *config,
                         orcm_cfgi_xml_parser_t *xml_in)
{
    char *line;
    orcm_cfgi_xml_parser_t *xml;
    char *tag;

    opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                        "WORKING %s", (NULL == xml_in) ? "NEW" : xml_in->name);
    xml = NULL;
    while (NULL != (line = orcm_getline(fp))) {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "LINE: %s", line);
        /* skip empty lines */
        if (strlen(line) < 3) {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "ZERO OR SHORT LENGTH LINE");
            free(line);
            continue;
        }
        /* skip all comment lines and xml directives */
        if ('<' != line[0] || '?' == line[1] || '!' == line[1]) {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "COMMENT OR DIRECTIVE");
            free(line);
            continue;
        }
        /* extract the tag */
        if (NULL == (tag = extract_tag(filename, line))) {
            return;
        }
        /* are we inside a section? */
        if (NULL != xml_in) {
            /* is this the matching end tag? */
            if (0 == strcmp(xml_in->name, tag)) {
                /* pop the stack */
                free(tag);
                free(line);
                return;
            }
            /* ignore any descriptions */
            if (0 == strcmp(tag, "description")) {
                free(tag);
                free(line);
                continue;
            }
            /* if it is not a value, then it must be the start
             * of a new section - so parse it
             */
            if (0 != strcmp(tag, "value")) {
                xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
                xml->name = tag;
                opal_list_append(&xml_in->subvals, &xml->super);
                opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                    "START NEW SUBSECTION: %s", tag);
                parse_config(fp, filename, &xml_in->subvals, xml);
                continue;
            }
            /* if it is a value, fill it in */
            free(tag);
            if (NULL == (tag = extract_value(filename, line))) {
                return;
            }
            opal_argv_append_nosize(&xml_in->value, tag);
            /* keep looping until we see the other end of the section */
            parse_config(fp, filename, config, xml_in);
            return;
        }
        /* if we are not inside a section, then start one */
        xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
        xml->name = strdup(tag);
        opal_list_append(config, &xml->super);
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "NOT INSIDE: START NEW SECTION: %s", tag);
        free(tag);
        /* loop around and parse it */
        parse_config(fp, filename, config, xml);
    }
}

static bool check_me(orcm_config_t *config, char *node,
                     orte_vpid_t vpid, char *my_ip)
{
    char *uri;

    if (NULL == node) {
        return false;
    }

    if (opal_net_isaddr(node)) {
        if (0 == strcmp(node, my_ip)) {
            ORTE_PROC_MY_NAME->vpid = vpid;
            setup_environ(config->mca_params);
            if (config->aggregator) {
                orte_process_info.proc_type = ORCM_AGGREGATOR;
            }
            /* load our port */
            asprintf(&uri, "OMPI_MCA_oob_tcp_static_ipv4_ports=%s", config->port);
            putenv(uri);  // cannot free this value
            opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                "push our port %s", uri);
           return true;
        }
    } else {
        if (0 == strcmp(node, orte_process_info.nodename)) {
            ORTE_PROC_MY_NAME->vpid = vpid;
            if (config->aggregator) {
                orte_process_info.proc_type = ORCM_AGGREGATOR;
            }
            setup_environ(config->mca_params);
            /* load our port */
            asprintf(&uri, "OMPI_MCA_oob_tcp_static_ipv4_ports=%s", config->port);
            putenv(uri);  // cannot free this value
            opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                "push our port %s", uri);
            return true;
        }
    }
    return false;
}


static int parse_orcm_config(orcm_config_t *cfg,
                             orcm_cfgi_xml_parser_t *xml)
{
    char *val, **vals;
    int n;

    if (0 == strcmp(xml->name, "port")) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            /* specifies the orcm-port to be used */
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-PORT %s", xml->value[0]);
            cfg->port = strdup(xml->value[0]);
        }
    } else if (0 == strcmp(xml->name, "cores")) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            /* specifies the cores to be used */
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tCORES %s", xml->value[0]);
            /* all we need do is push this into the corresponding param */
            asprintf(&val, "OMPI_MCA_orte_daemon_cores=%s", xml->value[0]);
            opal_argv_append_nosize(&cfg->mca_params, val);
            free(val);
        }
    } else if (0 == strcmp(xml->name, "mca-params")) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-MCA-PARAMS %s", xml->value[0]);
            vals = opal_argv_split(xml->value[0], ',');
            for (n=0; NULL != vals[n]; n++) {
                /* add the MCA prefix, if required */
                if (0 != strncmp(vals[n], "OMPI_MCA_", strlen("OMPI_MCA_"))) {
                    asprintf(&val, "OMPI_MCA_%s", vals[n]);
                } else {
                    val = strdup(vals[n]);
                }
                opal_argv_append_nosize(&cfg->mca_params, val);
                free(val);
            }
            opal_argv_free(vals);
        }
    } else if (0 == strcmp(xml->name, "log")) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tORCM-LOG %s", xml->value[0]);
            opal_setenv("OPAL_OUTPUT_REDIRECT", "file", true, &cfg->env);
            opal_setenv("OPAL_OUTPUT_LOCATION", xml->value[0], true, &cfg->env);
            opal_setenv("OPAL_OUTPUT_SUFFIX", ".log", true, &cfg->env);
        }
    } else if  (0 == strcmp(xml->name, "envar")) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tENVARS %s", xml->value[0]);
            vals = opal_argv_split(xml->value[0], ',');
            for (n=0; NULL != vals[n]; n++) {
                opal_argv_append_nosize(&cfg->env, vals[n]);
            }
            opal_argv_free(vals);
        }
    } else if  (0 == strcmp(xml->name, "aggregator")) {
        if (NULL != xml->value && NULL != xml->value[0]) {
            opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                "\tAGGREGATOR %s", xml->value[0]);
            if (0 == strncmp("y", xml->value[0], 1)) {
                cfg->aggregator = true;
            }
        } else {
            cfg->aggregator = true;
        }
    } else {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tUNKNOWN TAG");
    }
    return ORTE_SUCCESS;
}

static char* pack_charname(char base, char *s)
{
    char *t;
    size_t i;

    t = strdup(s);
    for (i=0; i < strlen(t); i++) {
        if ('@' == t[i]) {
            t[i] = base;
        }
    }
    return t;
 }

static int parse_node(orcm_node_t *node, int idx, orcm_cfgi_xml_parser_t *x)
{
    int rc, digits;
    char *name, *p, *q;
    orcm_rack_t *rack = (orcm_rack_t*)node->rack;

    if (0 == strcmp(x->name, "name")) {
        /* the value contains the node name, or an expression
         * whereby we replace any leading @ with the rack name and any
         * # with the node number (the number of # equals the
         * number of digits to be used)
         */
        if (NULL == x->value || NULL == x->value[0]) {
            /* that's an error */
            ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
            return ORTE_ERR_BAD_PARAM;
        }
        if ('@' == x->value[0][0]) {
            name = strdup(rack->name);
        } else {
            name = NULL;
        }
        if (NULL != (p = strchr(x->value[0], '#'))) {
            q = strrchr(x->value[0], '#');
            digits = (q - p) + 1;
        } else {
            digits = 0;
        }
        if (NULL == name && 0 == digits) {
            node->name = strdup(x->value[0]);
        } else if (NULL == name) {
            asprintf(&node->name, "%0*d", digits, idx);
        } else {
            asprintf(&node->name, "%s.%0*d", rack->name, digits, idx);
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
    char *name, *p, *q;
    int n, rc, nnodes, digits;
    orcm_cfgi_xml_parser_t *xx;
    orcm_node_t *node, *nd;

    if (0 == strcmp(x->name, "controller")) {
        /* the value contains the node name of the controller, or an expression
         * whereby we replace any leading @ with the row name and any
         * # with the rack number (the number of # equals the
         * number of digits to be used)
         */
        if (NULL == x->value || NULL == x->value[0]) {
            /* that's an error */
            ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
            return ORTE_ERR_BAD_PARAM;
        }
        if ('@' == x->value[0][0]) {
            name = strdup(rack->row->name);
        } else {
            name = NULL;
        }
        if (NULL != (p = strchr(x->value[0], '#'))) {
            q = strrchr(x->value[0], '#');
            digits = (q - p) + 1;
        } else {
            digits = 0;
        }
        if (NULL == name && 0 == digits) {
            rack->name = strdup(x->value[0]);
        } else if (NULL == name) {
            asprintf(&rack->name, "%0*d", digits, idx);
        } else {
            asprintf(&rack->name, "%s%0*d", rack->row->name, digits, idx);
        }
        rack->controller.name = strdup(rack->name);
        rack->controller.state = ORTE_NODE_STATE_UNKNOWN;
        /* parse any config that is attached to the rack controller */
        OPAL_LIST_FOREACH(xx, &x->subvals, orcm_cfgi_xml_parser_t) {
            if (ORCM_SUCCESS != (rc = parse_orcm_config(&rack->controller.config, xx))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
        }
    } else if (0 == strcmp(x->name, "node")) {
        /* the value contains either a single name, or a regular expression
         * defining multiple nodes.
         */
        if (NULL == x->value || NULL == x->value[0]) {
            /* that's an error */
            ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
            return ORTE_ERR_BAD_PARAM;
        }
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tNODE NAME %s", x->value[0]);
        /* if the first character is a #, then this is the number of nodes
         * to be defined
         */
        if ('#' == x->value[0][0]) {
            nnodes = strtol(&x->value[0][1], NULL, 10);
            for (n=0; n < nnodes; n++) {
                node = OBJ_NEW(orcm_node_t);
                node->rack = (struct orcm_rack_t*)rack;
                node->state = ORTE_NODE_STATE_UNKNOWN;
                opal_list_append(&rack->nodes, &node->super);
                /* now cycle thru the rest of this config element and apply
                 * those values to this node
                 */
                OPAL_LIST_FOREACH(xx, &x->subvals, orcm_cfgi_xml_parser_t) {
                    if (ORCM_SUCCESS != (rc = parse_node(node, n, xx))) {
                        ORTE_ERROR_LOG(rc);
                        return rc;
                    }
                }
            }
        } else {
            /* see if we already have this node */
            node = NULL;
            OPAL_LIST_FOREACH(nd, &rack->nodes, orcm_node_t) {
                if (0 == strcmp(nd->name, x->value[0])) {
                    node = nd;
                    break;
                }
            }
            if (NULL == node) {
                node = OBJ_NEW(orcm_node_t);
                node->name = strdup(x->value[0]);
                OBJ_RETAIN(rack);
                node->rack = (struct orcm_rack_t*)rack;
                node->state = ORTE_NODE_STATE_UNKNOWN;
                opal_list_append(&rack->nodes, &node->super);
            }
            /* now cycle thru the rest of this config element and apply
             * those values to this node
             */
            OPAL_LIST_FOREACH(xx, &x->subvals, orcm_cfgi_xml_parser_t) {
                if (ORCM_SUCCESS != (rc = parse_node(node, 0, xx))) {
                    ORTE_ERROR_LOG(rc);
                    return rc;
                }
            }
        }
    } else {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tUNKNOWN TAG");
    }
    return ORCM_SUCCESS;
}

static int parse_row(orcm_row_t *row, orcm_cfgi_xml_parser_t *x)
{
    int n, rc, nracks;
    orcm_cfgi_xml_parser_t *xx;
    orcm_rack_t *rack, *r;

    if (0 == strcmp(x->name, "controller")) {
        /* the value contains the node name of the controller, or an expression
         * whereby we replace any leading or trailing # with the row name
         */
        if (NULL == x->value || NULL == x->value[0]) {
            /* that's an error */
            ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
            return ORTE_ERR_BAD_PARAM;
        }
        row->controller.name = pack_charname(row->name[0], x->value[0]);
        row->controller.state = ORTE_NODE_STATE_UNKNOWN;
        /* parse any subvals that are attached to the row controller */
        OPAL_LIST_FOREACH(xx, &x->subvals, orcm_cfgi_xml_parser_t) {
            if (ORCM_SUCCESS != (rc = parse_orcm_config(&row->controller.config, xx))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
        }
    } else if (0 == strcmp(x->name, "rack")) {
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
        /* if the first character is a #, then this is the number of racks
         * to be defined - the racks are named by their controllers
         */
        if ('#' == x->value[0][0]) {
            nracks = strtol(&x->value[0][1], NULL, 10);
            for (n=0; n < nracks; n++) {
                rack = OBJ_NEW(orcm_rack_t);
                rack->row = row;
                OBJ_RETAIN(row);
                opal_list_append(&row->racks, &rack->super);
                /* now cycle thru the rest of this config element and apply
                 * those values to this rack
                 */
                OPAL_LIST_FOREACH(xx, &x->subvals, orcm_cfgi_xml_parser_t) {
                    if (ORCM_SUCCESS != (rc = parse_rack(rack, n, xx))) {
                        ORTE_ERROR_LOG(rc);
                        return rc;
                    }
                }
            }
        } else {
            /* see if we already have this rack */
            rack = NULL;
            OPAL_LIST_FOREACH(r, &row->racks, orcm_rack_t) {
                if (0 == strcmp(r->name, x->value[0])) {
                    rack = r;
                    break;
                }
            }
            if (NULL == rack) {
                rack = OBJ_NEW(orcm_rack_t);
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
                    return rc;
                }
            }
        }
    } else {
        opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                            "\tUNKNOWN TAG");
    }
    return ORCM_SUCCESS;
}

static int parse_cluster(orcm_cluster_t *cluster,
                         orcm_cfgi_xml_parser_t *xx)
{
    orcm_cfgi_xml_parser_t *x, *y;
    int rc, n;
    char **vals, **names;
    orcm_row_t *r, *row;

    OPAL_LIST_FOREACH(x, &xx->subvals, orcm_cfgi_xml_parser_t) {
        if (0 == strcmp(x->name, "controller")) {
            /* the value contains the node name of the controller, or an expression
             * whereby we replace any leading or trailing # with the row name
             */
            if (NULL == x->value || NULL == x->value[0]) {
                /* that's an error */
                ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
                return ORTE_ERR_BAD_PARAM;
            }
            cluster->controller.name = strdup(x->value[0]);
            cluster->controller.state = ORTE_NODE_STATE_UNKNOWN;
            /* parse any subvals that are attached to the cluster controller */
            OPAL_LIST_FOREACH(xx, &x->subvals, orcm_cfgi_xml_parser_t) {
                if (ORCM_SUCCESS != (rc = parse_orcm_config(&cluster->controller.config, xx))) {
                    ORTE_ERROR_LOG(rc);
                    return rc;
                }
            }
        } else if (0 == strcmp(x->name, "row")) {
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
            names = NULL;
            for (n=0; NULL != vals[n]; n++) {
                if (ORTE_SUCCESS != (rc = orte_regex_extract_name_range(vals[n], &names))) {
                    opal_argv_free(vals);
                    return rc;
                }
            }
            if (NULL == names) {
                /* that's an error */
                return ORCM_ERR_BAD_PARAM;
            }
            /* see if we have each row object - it not, create it */
            for (n=0; NULL != names[n]; n++) {
                row = NULL;
                OPAL_LIST_FOREACH(r, &cluster->rows, orcm_row_t) {
                    if (0 == strcmp(r->name, names[n])) {
                        row = r;
                        break;
                    }
                }
                if (NULL == row) {
                    opal_output_verbose(10, orcm_cfgi_base_framework.framework_output,
                                        "\tNEW ROW NAME %s", names[n]);
                    row = OBJ_NEW(orcm_row_t);
                    row->name = strdup(names[n]);
                    opal_list_append(&cluster->rows, &row->super);
                }
                /* now cycle thru the rest of this config element and apply
                 * those values to this row
                 */
                OPAL_LIST_FOREACH(y, &x->subvals, orcm_cfgi_xml_parser_t) {
                    if (ORCM_SUCCESS != (rc = parse_row(row, y))) {
                        ORTE_ERROR_LOG(rc);
                        return rc;
                    }
                }
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
    orcm_cfgi_xml_parser_t *x;
    int rc, n;


    OPAL_LIST_FOREACH(x, &xx->subvals, orcm_cfgi_xml_parser_t) {
        if (0 == strcmp(x->name, "queues")) {
            /* the value tells us either the name of the row, or a regular expression
             * describing the names of the rows
             */
            if (NULL == x->value || NULL == x->value[0]) {
                /* that's an error */
                ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
                return ORTE_ERR_BAD_PARAM;
            }
            /* each value in the argv-array defines a queue, so just
             * pass them back to the scheduler
             */
            if (NULL != x->value) {
                for (n=0; NULL != x->value[n]; n++) {
                    opal_argv_append_nosize(&scheduler->queues, x->value[n]);
                }
            }
        } else if (0 == strcmp(x->name, "node")) {
            /* defines the name of the node where the scheduler is running */
            if (NULL == x->value || NULL == x->value[0]) {
                /* that's an error */
                ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
                return ORTE_ERR_BAD_PARAM;
            }
            scheduler->controller.name = strdup(x->value[0]);
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

    /* get a copy of our environment */
    tmp = opal_argv_copy(environ);

    /* go thru the provided environment and *only* set
     * envars that were not previously set. This allows
     * users to override the config file on the cmd line
     */
    tmp = opal_environ_merge(env, environ);

    /* now cycle thru the result and push MCA params back into our
     * environment. We will overwrite some existing values,
     * but no harm done
     */
    for (i=0; NULL != tmp[i]; i++) {
        if (0 == strncmp(tmp[i], "OMPI_MCA_", strlen("OMPI_MCA_"))) {
            t = strdup(tmp[i]);
            opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                "PUSHING %s TO ENVIRON", t);
            putenv(t);
        }
    }
    opal_argv_free(tmp);
}

