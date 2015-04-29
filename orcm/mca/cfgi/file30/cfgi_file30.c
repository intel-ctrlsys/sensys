/*
 * Copyright (c) 2015      Intel, Inc.  All rights reserved.
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

#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/cfgi/file30/cfgi_file30.h"

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

static char * get_name_field( unsigned long in_which_row, char ** in_items,
                              long ** in_hierarchy, unsigned long * in_hier_row_length,
                              unsigned long in_sz_hierarchy );
static int check_junctions_have_names( char ** in_items, long ** in_hierarchy,
                                       unsigned long * in_hier_row_length,
                                       unsigned long in_sz_hierarchy);
static int check_parent_has_uniquely_named_children( char ** in_items,
                                                     long ** in_hierarchy,
                                                     unsigned long * in_hier_row_length,
                                                     unsigned long in_sz_hierarchy );

static int lex_xml(char * in_filename, char ** o_text, char *** o_items,
                   unsigned long * o_item_count);
static int check_validity_of_the_inputs(char ** in_items,
                                        unsigned long in_item_count);
static int check_all_port_fields(char ** in_items, unsigned long in_item_count);

static int isa_sectional_item(const char * in_tagtext);
static int find_beginend_configuration(unsigned long in_start_index,
                                       char ** in_items,
                                       unsigned long in_sz_items,
                                       int in_1for_record_0for_mod,
                                       unsigned long * o_begin_config,
                                       unsigned long * o_end_config);
static int structure_lexed_items(unsigned long in_begin_offset,
                                 unsigned long in_end_offset,
                                 char ** in_items,
                                 unsigned long in_item_count,
                                 long *** o_hierachy,
                                 unsigned long ** o_hier_row_length,
                                 unsigned long * o_length_hierarchy);

static int transfer_to_controller(char ** in_items,
                                  unsigned long in_hier_row_length,
                                  long * in_hierarchy,
                                  orcm_cfgi_xml_parser_t * io_parent);
static int transfer_to_scheduler(char ** in_items,
                                 unsigned long in_hier_row_length,
                                 long * in_hierarchy,
                                 orcm_cfgi_xml_parser_t * io_parent);

static int remove_empty_items(char ** io_items, unsigned long * io_sz_items);

static int isa_singleton(const char * in_tagtext);
static int remove_duplicate_singletons(char ** in_items,
                                       long ** io_hierarchy,
                                       unsigned long * io_hier_row_length,
                                       unsigned long * io_sz_hierarchy);

static void print_xml_structure( char ** in_items, unsigned long in_sz_hierarchy,
                                 unsigned long * in_hier_row_length,
                                 long ** in_hierarchy);

static int file30_init(void)
{
    char * text = NULL;  /* Owns the array */
    char ** items = NULL; /* Owns the array but not its c-sring entries */
    unsigned long sz_items=0;

    char * data=NULL;
    unsigned long i=0;

    int version_found=0; /*Set to 1 if found*/

    int erri = ORCM_SUCCESS;

    while (ORCM_SUCCESS == erri){
        if (NULL == orcm_cfgi_base.config_file) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output, "NULL FILE");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        erri = lex_xml(orcm_cfgi_base.config_file,&text,&items,&sz_items);
        if (ORCM_SUCCESS != erri) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "FAILED TO PARSE XML CFGI FILE");
            break;
        }

        /* Now look for the first version data field */
        for (i=0; i!=sz_items; ++i) {
            if ('/' == items[i][0]) {
                continue;
            }
            if ('\t' == items[i][0]) {
                continue;
            }
            if (0 == strcasecmp("version", items[i])) {
                if (i+1 >= sz_items) {
                    opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                        "MISSING VERSION DATA");
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                data=items[i+1];
                if ('\t'!=data[0]) {
                    opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                        "BAD PARSING OF VERSION DATA");
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                if ('3' == data[1]) { /* TODO: Check for whitespace if someone quoted the version*/
                    version_found = 1;
                } else {
                    opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                        "EXPECTED VERSION 3 NOT FOUND: %s",data+1);
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (0 == version_found){
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output, "VERSION 3 NOT FOUND");
            erri = ORCM_ERR_NOT_FOUND;
        }

        break;
    }

    if (NULL != items) {
        free(items);
        items = NULL;
        sz_items = 0;
    }

    if (NULL != text) {
        free(text);
        text = NULL;
    }

    if (ORCM_SUCCESS != erri) {
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }

    return ORCM_SUCCESS;
}

static void file30_finalize(void)
{
}

static int parse_config(char ** in_items,
                        unsigned long in_sz_items,
                        opal_list_t *io_config);
static bool check_me(orcm_config_t *config, char *node,
                     orte_vpid_t vpid, char *my_ip);

static int read_config(opal_list_t *config)
{
    FILE * fp=NULL;

    orcm_cfgi_xml_parser_t *xml; //An iterator, owns nothing.

    char * text = NULL;  /* Owns the array */
    char ** items = NULL; /* Owns the array but not its c-sring entries */
    unsigned long sz_items=0;

    unsigned long i=0;

    int erri = ORCM_SUCCESS;

    /*Check if the file exist*/     /*TODO: Use something more robust than fopen */
    fp = fopen(orcm_cfgi_base.config_file, "r");
    if (NULL == fp) {
        orte_show_help("help-orcm-cfgi.txt", "site-file-not-found", true, orcm_cfgi_base.config_file);
        return ORCM_ERR_SILENT;
    }
    if (0 != fclose(fp)) {
        opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                            "FAIL to PROPERLY CLOSE THE CFGI XML FILE");
        return ORCM_ERR_SILENT;
    } else {
        fp = NULL;
    }

    while (ORCM_SUCCESS == erri) {
        erri = lex_xml(orcm_cfgi_base.config_file, &text, &items, &sz_items);
        if (ORCM_SUCCESS != erri) {
            opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                "FAILED TO XML LEX THE FILE");
            break;
        }

        erri = remove_empty_items(items,&sz_items);
        if (ORCM_SUCCESS != erri) {
            opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                "FAILED TO REMOVE EMPTY FIELDS IN XML CFGI");
            break;
        }

        if (V_HIGHER < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
            for (i=0; i!=sz_items; ++i) {
                /*The orcm_cfgi_base_framework.framework_output of zero used here was taken from orcm_util_print_xml.*/
                opal_output(0, "XML-LEXR:%lu: %s", i, items[i]);
            }
        }

        erri = check_validity_of_the_inputs(items,sz_items);
        if (ORCM_SUCCESS != erri) {
            opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                "XML VALIDITY CHECK FAILED: %d", erri);
            break;
        }
        opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                            "XML lexer-level VALIDITY CHECKED and PASSED");

        erri = parse_config(items,sz_items, config);
        if (ORCM_SUCCESS != erri) {
            opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                "FAILED TO PARSE THE CFGI XML FILE:%d",erri);
            break;
        }

        if (V_HIGHER < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
            OPAL_LIST_FOREACH(xml, config, orcm_cfgi_xml_parser_t) {
                orcm_util_print_xml(xml, NULL);
            }
        }

        break;
    }

    if (NULL != items) {
        free(items);
        items = NULL;
        sz_items = 0;
    }

    if (NULL != text) {
        free(text);
        text = NULL;
    }

    if (ORCM_SUCCESS != erri) {
        return ORCM_ERR_SILENT;
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
                }
            }
            ++vpid;
        }
        /* pack the cluster controller */
        opal_dss.pack(&clusterbuf, &cluster->controller.daemon, 1, ORTE_NAME);
        /* pack the number of rows */
        num = opal_list_get_size(&cluster->rows);
        opal_dss.pack(&clusterbuf, &num, 1, OPAL_INT32);
        OPAL_LIST_FOREACH(row, &cluster->rows, orcm_row_t) {
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
                    }
                }
                ++vpid;
            }
            /* pack the name of the row controller, which will be invalid
             * if we don't have one in the config */
            opal_dss.pack(&rowbuf, &row->controller.daemon, 1, ORTE_NAME);
            OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
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

    return ORCM_SUCCESS;
}

/*  This function finds the beginning index and the end index, in in_items,
    for a configuration of the type requested by in_1for_record_0for_mod.
    The search starts at in_start_index.
    It returns
        = Non-zero upon error
        = Zero AND *o_begin_config == *o_end_config if none are found.
        = Zero AND *o_begin_config < *o_end_config at the first one found.
    The search is sequential, lowest indices looked at first.
 */
static int find_beginend_configuration(unsigned long in_start_index,
                                       char ** in_items,
                                       unsigned long in_sz_items,
                                       int in_1for_record_0for_mod,
                                       unsigned long * o_begin_config,
                                       unsigned long * o_end_config)
{
    const unsigned long nota_index=(unsigned long)-1;
    char * t=NULL;

    unsigned long beg=0, end=0;
    unsigned long i=0;

    *o_begin_config = nota_index;
    *o_end_config   = nota_index;

    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (in_start_index >= in_sz_items) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: Invalid start index for the configuration search");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        beg = nota_index;
        for (i=in_start_index; i!=in_sz_items; ++i) {
            t = in_items[i];
            if (nota_index == beg) {
                if ('c'!=t[0] && 'C'!=t[0]) {
                    continue;
                }
                if (0 != strcasecmp(t,"configuration")) {
                    continue;
                }
                /*configuration found*/
                beg=i;
            } else {
                /*Make sure the found configuration is a RECORD one */
                if ('/'==t[0] || '\t'==t[0]) {
                    /*Skip data field and closing tags*/
                    continue;
                }
                if (0 == strcasecmp(t,"role")) {
                    if ('\t' != in_items[i+1][0]) {
                        opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                            "ERROR: Parser error at tag \"%s\"", t);
                        erri = ORCM_ERR_BAD_PARAM;
                        break;
                    }
                    t = in_items[i+1];
                    if (0 == strcasecmp(t+1,"RECORD")) {
                        if (in_1for_record_0for_mod == 1) {
                            /*We found what we were looking for*/
                            /*So stop searching*/
                            break;
                        } else {
                            /*We are looking for the RECORD configuration.*/
                            /*So restart the search*/
                            beg = nota_index;
                        }
                     } else if (0 == strcasecmp(t+1,"MOD")) {
                        if (0 == in_1for_record_0for_mod) {
                            /*We found what we were looking for*/
                            /*So stop searching*/
                            break;
                        } else {
                            /*We are looking for the MOD configuration.*/
                            /*So restart the search*/
                            beg = nota_index;
                        }
                    } else {
                        opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                            "ERROR: Unkown option found for \"role\" :%s", t);
                        erri= ORCM_ERR_BAD_PARAM;
                        break;
                    }
                } else if (0 == strcasecmp(t,"version")) {
                    continue;
                } else if (0 == strcasecmp(t,"creation")) {
                    continue;
                } else {
                    opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                        "ERROR: Missing \"role\" tag before the \"%s\" tag",t);
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }
        if (nota_index == beg) {
            /*Nothing found */
            *o_begin_config = nota_index;
            *o_end_config   = nota_index;
            break;
        }

        /*===== Find the ending of the RECORD configuration */
        end = nota_index;
        for (i=beg; i!=in_sz_items; ++i) {
            t = in_items[i];
            if ('/' != t[0]) {
                /*Looking for an ending tag only*/
                continue;
            }
            t=in_items[i]+1;
            if ('c' != t[0]) {
                continue;
            }
            if (0 == strcasecmp(t,"configuration")) {
                end = i + 1; /*+1 since we want one past the tag*/
                break;
            }
        }
        if (nota_index == end) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: Missing closing tag for the <configuration> of type RECORD");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        *o_begin_config = beg;
        *o_end_config   = end;

        break;
    } /* while(!erri) */

    return erri;
}

static char * tolower_cstr(char * in)
{
    char * t=in;
    while (NULL!=t && '\0'!=*t) {
        if ('A'>=*t && 'Z'<=*t ) {
            *t -= 'A';
            *t += 'a';
        }
        ++t;
    }
    return in;
}

static void print_xml_structure( char ** in_items, unsigned long in_sz_hierarchy,
                                 unsigned long * in_hier_row_length,
                                 long ** in_hierarchy)
{
    const unsigned int BUFSZ = 1024;

    unsigned long i=0, j=0;

    unsigned long sz_txt = 0, sz=0;
    char * txt = NULL;
    char * t = NULL;
    char buf[BUFSZ];
    char c = '\0';

    if (0 == in_sz_hierarchy || NULL == in_hier_row_length || NULL == in_hierarchy) {
        return;
    }

    for (i=0; i < in_sz_hierarchy; ++i) {
        /* First find out the length of individual lines. */
        sz=0;

        snprintf(buf, BUFSZ, "XML-STRUCT: %lu> ", i);
        sz += strlen(buf);

        for (j=0; j < in_hier_row_length[i]; ++j){

            if (0 > in_hierarchy[i][j]) {
                snprintf(buf, BUFSZ, "%ld\t", -1*in_hierarchy[i][j] );
                sz += strlen(buf);
            } else {
                snprintf(buf, BUFSZ, "%s\t", in_items[ in_hierarchy[i][j] ] );
                sz += strlen(buf);
            }
        }

        if (sz > sz_txt) {
            if (NULL != txt) {
                free(txt);
                txt=NULL;
            }
            sz_txt = 2*sz; /* Times 2 to avoid superfluous mallocs */
            txt = (char*) calloc(sz_txt, sizeof(char) );
            if (NULL == txt){
                /* This should not happen.  Bail out . */
                return;
            }
        }

        /* Then assemble the line and print */
        t = txt;
        snprintf(t, BUFSZ, "XML-STRUCT: %lu> ", i);
        t += strlen(t);

        for (j=0; j < in_hier_row_length[i]; ++j){
            if ( j+1 >= in_hier_row_length[i]) {
                c = ' ';
            } else {
                c = '\t';
            }
            if (0 > in_hierarchy[i][j]) {
                snprintf(t, BUFSZ, "%ld%c", -1*in_hierarchy[i][j], c );
            } else {
                snprintf(t, BUFSZ, "%s%c", in_items[ in_hierarchy[i][j] ], c );
            }
            t += strlen(t);
        }

        opal_output(0, "%s", txt);
    }

    if (NULL != txt) {
        free(txt);
    }
}

static int parse_config(char ** in_items,
                        unsigned long in_sz_items,
                        opal_list_t *io_config)
{
    /*
      io_config is a list of items of type orcm_cfgi_xml_parser_t
    */
    const int NEED_RECORD=1;

    char * t=NULL, *tt=NULL, *txt=NULL;
    unsigned long i=0, j=0;

    unsigned long record_begin=0;
    unsigned long record_end=0;

    unsigned long sz_hierarchy=0;
    unsigned long * hier_row_length=NULL;
    long ** hierarchy=NULL;

    long * hstack=NULL;  /*Stores row offset into hierarchy*/
    orcm_cfgi_xml_parser_t **parent_stack=NULL; /*Array of pointer to xml struct*/
    unsigned long sz_stack=0;
    long u=0, uu=0;

    orcm_cfgi_xml_parser_t *parent=NULL;
    orcm_cfgi_xml_parser_t *xml=NULL;

    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        /*===== Find the RECORD configuration */
        erri = find_beginend_configuration(record_begin, in_items, in_sz_items,
                                           NEED_RECORD, &record_begin, &record_end);
        if (ORCM_SUCCESS != erri) {
            break;
        }
        if (record_begin == record_end) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR:The RECORD <configuration> is missing from the XML CFGI file");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /*===== Parse the found RECORD configuration */
        erri = structure_lexed_items(record_begin, record_end, in_items, in_sz_items,
                                     &hierarchy, &hier_row_length, &sz_hierarchy);
        if (ORCM_SUCCESS != erri) {
            break;
        }
        if (0 == sz_hierarchy) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        erri = remove_duplicate_singletons(in_items, hierarchy, hier_row_length, &sz_hierarchy);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = check_junctions_have_names( in_items, hierarchy, hier_row_length,
                                           sz_hierarchy);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (V_HIGHER < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
            print_xml_structure(in_items, sz_hierarchy, hier_row_length, hierarchy);
        }

        erri = check_parent_has_uniquely_named_children( in_items, hierarchy,
                                                         hier_row_length,
                                                         sz_hierarchy);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*===== Allocate the working stacks */

        /*Here multiply by 2 in order to be generous. */
        sz_stack = 2 * sz_hierarchy;
        hstack = (long*) calloc(sz_stack, sizeof(long));
        if (NULL == hstack){
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        parent_stack = (orcm_cfgi_xml_parser_t **) calloc(sz_stack,sizeof(orcm_cfgi_xml_parser_t *));
        if (NULL == parent_stack) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        /*===== Start the stacks */
        t = in_items[ hierarchy[0][0] ];
        if (0 != strcasecmp(t,"configuration")) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }
        sz_stack=0;

        xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
        if (NULL == xml) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }
        xml->name = strdup(t);
        tolower_cstr(xml->name);
        opal_list_append(io_config, &xml->super);

        for (i=0; i < hier_row_length[0]; ++i) {
            if (0 > hierarchy[0][i]) {
                parent_stack[sz_stack] = xml;
                hstack[sz_stack] = -1 *hierarchy[0][i];
                ++sz_stack;
            } else {
                /*Skip configuration specific items*/
            }
        }
        xml=NULL;

        if (0 == sz_stack) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,"EMPTY configuration hierarchy");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        } else {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output, "XML-XFER: on item 0 = configuration");
        }

        /*===== Stuff the XML into the config */
        while (0 < sz_stack){
            --sz_stack;
            i = hstack[sz_stack];
            parent = parent_stack[sz_stack];

            u = hierarchy[i][0];
            if (0 > u) {
                erri = ORCM_ERR_BAD_PARAM; /*That should never happen.*/
                break;
            }

            t = in_items[u];
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "XML-XFER: on item %lu = %s", i, t);
            if ('c'==t[0] || 'C'==t[0]) {
                if (0 == strcasecmp(t,"controller")) {
                    erri = transfer_to_controller(in_items, hier_row_length[i], hierarchy[i], parent);
                    if (ORCM_SUCCESS != erri) {
                        break;
                    }
                }
            } else if( 'j'==t[0] || 'J'==t[0]) {
                if (0 == strcasecmp(t,"junction")) {
                    xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
                    if (NULL == xml) {
                        erri = ORCM_ERR_OUT_OF_RESOURCE;
                        break;
                    }
                    opal_list_append(&parent->subvals, &xml->super);

                    for (j=1; j < hier_row_length[i]; ++j) {
                        uu = hierarchy[i][j];
                        if (0 > uu) {
                            hstack[sz_stack]       = -1*uu;
                            parent_stack[sz_stack] = xml;
                            ++sz_stack;
                            continue;
                        } else {
                            tt = in_items[uu];
                            if (0 == strcasecmp(tt,"type")) {
                                xml->name = strdup(in_items[uu+1]+1);
                                tolower_cstr(parent->name);
                            } else if (0 == strcasecmp(tt,"name")) {
                                txt = NULL;
                                txt = strdup(in_items[uu+1]+1);
                                tolower_cstr(txt);
                                if (OPAL_SUCCESS != opal_argv_append_nosize(&xml->value, txt)) {
                                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                                    free(txt);
                                    txt = NULL;
                                    break;
                                }
                                free(txt);
                                txt = NULL;
                            } else {
                                /*Skip the other data fields*/
                            }
                        }
                    }
                    if (ORCM_SUCCESS != erri){
                        break;
                    }
                    xml = NULL;
                }
            } else if ('s'==t[0] || 'S'==t[0]) {
                if (0 == strcasecmp(t,"scheduler")) {
                    erri = transfer_to_scheduler(in_items, hier_row_length[i], hierarchy[i], parent);
                    if (ORCM_SUCCESS != erri) {
                        break;
                    }
                }
            } else {
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
        } /*while(sz_stack)*/
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }

    sz_stack=0;
    if (NULL != hstack) {
        free(hstack);
        hstack=NULL;
    }
    if (NULL != parent_stack) {
        free(parent_stack);
        parent_stack=NULL;
    }

    if (NULL != hier_row_length ){
        free(hier_row_length);
        hier_row_length=NULL;
    }
    if (NULL != hierarchy) {
        for(i=0; i < sz_hierarchy; ++i) {
            if (NULL != hierarchy[i]) {
                free(hierarchy[i]);
                hierarchy[i]=NULL;
            }
        }
        free(hierarchy);
        hierarchy=NULL;
        sz_hierarchy=0;
    }

    return erri;
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
            asprintf(&uri, OPAL_MCA_PREFIX"oob_tcp_static_ipv4_ports=%s", config->port);
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
            asprintf(&uri, OPAL_MCA_PREFIX"oob_tcp_static_ipv4_ports=%s", config->port);
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
            asprintf(&val, OPAL_MCA_PREFIX"orte_daemon_cores=%s", xml->value[0]);
            if (OPAL_SUCCESS != opal_argv_append_nosize(&cfg->mca_params, val)) {
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
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
                    if (OPAL_SUCCESS != opal_argv_append_nosize(&cfg->mca_params, val)) {
                        return ORCM_ERR_OUT_OF_RESOURCE;
                    }
                    free(val);
                }
                opal_argv_free(vals);
            }
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
                if (OPAL_SUCCESS != opal_argv_append_nosize(&cfg->env, vals[n])) {
                    return ORCM_ERR_OUT_OF_RESOURCE;
                }
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
            ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
            return ORCM_ERR_BAD_PARAM;
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
            free(name);
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
            ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
            return ORCM_ERR_BAD_PARAM;
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
            free(name);
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
            ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
            return ORCM_ERR_BAD_PARAM;
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
            ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
            return ORCM_ERR_BAD_PARAM;
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
                ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                return ORCM_ERR_BAD_PARAM;
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
            if (NULL != vals) {
                names = NULL;
                for (n=0; NULL != vals[n]; n++) {
                    if (ORTE_SUCCESS != (rc = orte_regex_extract_name_range(vals[n], &names))) {
                        opal_argv_free(vals);
                        opal_argv_free(names);
                        return rc;
                    }
                }
                if (NULL == names) {
                    /* that's an error */
                    opal_argv_free(vals);
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
                            opal_argv_free(vals);
                            opal_argv_free(names);
                            return rc;
                        }
                    }
                }
                opal_argv_free(vals);
                opal_argv_free(names);
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
        } else if (0 == strcmp(x->name, "node")) {
            /* defines the name of the node where the scheduler is running */
            if (NULL == x->value || NULL == x->value[0]) {
                /* that's an error */
                ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                return ORCM_ERR_BAD_PARAM;
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
        if (0 == strncmp(tmp[i], OPAL_MCA_PREFIX, strlen(OPAL_MCA_PREFIX))) {
            t = strdup(tmp[i]);
            opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                "PUSHING %s TO ENVIRON", t);
            putenv(t);
        }
    }
    opal_argv_free(tmp);
}


static int is_whitespace(char c){
    if (' '==c || '\t'==c || '\n'==c || '\r'==c) {
        return 1;
    }
    return 0;
}

static int is_forbidden(char c){
    int x=c;
    if (31 >= x || 127 <= x) {
        return 1;
    }
    return 0;
}

    /* This function takes the XML text found in in_filename and returns
     * a list of items, pointed to by o_items.
     *
     * o_text : =Owned by the call function
     *          =A raw text buffer where all the info is stored.
     * o_items : =Owned by the call function
     *           =An array of (char*), i.e. c-strings, where each entry is
     *            an item in the XML file.
     *            These c-string item are in o_text, and are not owned by o_items.
     * o_item_count : The length o_items
     * NOTE:
     *  =All data field are preprended by '\t'.
     *  =All closing marker are prepended with '/'.
     */
static int lex_xml( char * in_filename, char ** o_text,
                    char *** o_items, unsigned long * o_item_count)
{
    int erri=ORCM_SUCCESS;

    FILE * fin =NULL;
    unsigned long fsize; /*The number of bytes in the in_filename file*/
    const unsigned long min_file_size = 32; /*TODO: update this number*/
    char * tx=NULL; /*Contains all the text before transfer */

    char * p=NULL, *q=NULL; /*Utility pointers */

    const int some_added_extra=32;
    unsigned long i=0,j=0,k=0;
    int ic=0;
    char c=0;
    unsigned long line_count=0;
    unsigned long text_length=0;

    char ** tem=NULL; /*The working array of items */
    unsigned long sz_tem=0; /*length of tem */

    int came_from_data=-1; /* -1 -> Undecided
                            *  0 -> False
                            *  1 -> True
                            */

    *o_text = NULL;
    *o_items = NULL;
    *o_item_count = 0;

    while (ORCM_SUCCESS == erri){
        /*----- Get the image of the file */
        fin = fopen(in_filename, "r");
        if (NULL == fin) {
            erri = ORCM_ERR_NOT_FOUND;
            break;
        }

        fseek(fin, 0L, SEEK_END);
        fsize = ftell(fin);
        fseek(fin, 0L, SEEK_SET);

        if (fsize < min_file_size) {
            erri = ORCM_ERROR;
            break;
        }

        tx = (char*) calloc(fsize + some_added_extra, sizeof(char));
        if (NULL == tx) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        for (i=0; i < fsize; ++i) {
            ic = fgetc(fin);
            if (EOF == ic) {
                /*It used to be--> erri=__LINE__; */
                /*Going From Window OS to Linux and back does funny things with
                 *fsize and where one will encounter the EOF.  The EOF may
                 *show up earlier.
                 *IF EOF is found early, fsize will be corrected->See @1 marker.
                 *TODO: FInd out why the EOF is not where fsize says it is.
                 */
                opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                    "WARNING: unexpected end-of-file encountered\n");
                break;
            }
            if (ic > 255) {
                erri = ORCM_ERROR;
                break;
            }
            c=ic;
            tx[i] = c;
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        fsize = i;        /* @1 Correcting fsize if needed. See comment above.*/
        tx[fsize] = '\0';

        /*-----Chop things up
         *
         * 2 types of XML tags:
         *      ctDIRECTIVE -> XML specifics ->To be thrown away
         *      ctMARKER    -> tags we use -> To be kept
         * We need to keep the ctFIELD our ctMARKER defines.
         * All whitespaces are to be thrwon away, except
         * those within the double-quoted in ctFIELD.
         *
         *=The text will use '\n' as token delimiter.  In the final
         * text, the delimiter will be '\0'.
         *=All data field will be preprended by '\t'.
         *=After the final delimiter a '\0' is appended but not counted
         * in the text length.
         *=All closing marker will be prepended with '/'.
         */

        line_count = 1;
        k = (unsigned long)(-1);
        for (i=0; i < fsize; ++i) {
            c = tx[i];
            if (is_whitespace(c)) {
                if ('\n' ==c ) {
                    ++line_count;
                }
                continue;
            }
            if ('<' == c) {
                if (1 == came_from_data) {
                    tx[++k ]= '\n';
                    came_from_data = 0;
                }
                p = strchr(tx+i, '>');
                if (NULL == p) {
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                if ('!' == tx[i+1] || '?' == tx[i+1]) {
                    /*In a directive */
                    /* A directive will end with one of the following:
                            ?>
                        or  -->
                     */
                    if ('?'==*(p-1) || ('-'==*(p-1) && '-'==*(p-2))) {
                        i += (p-(tx+i)); /*Ratchet up to the ending '>' */
                        if (i >= fsize) {
                            erri = ORCM_ERR_BAD_PARAM;
                            break;
                        }
                    } else {
                        /*Keep looking*/
                        while (!('?'==*(p-1) || ('-'==*(p-1) && '-'==*(p-2)))) {
                            ++p;
                            p = strchr(p, '>');
                        }
                        i += (p-(tx+i)); /*Ratchet up to the ending '>' */
                        if (i >= fsize) {
                            erri = ORCM_ERR_BAD_PARAM;
                            break;
                        }
                    }
                    continue;
                } else {
                    /*In a marker*/
                    for (q=tx+i; q< p; ++q) {
                        if ('\t' == *q) {
                            continue;
                        }
                        if (is_forbidden(*q)) {
                            erri = ORCM_ERR_BAD_PARAM;
                            break;
                        }
                    }
                    if (ORCM_SUCCESS != erri) {
                        break;
                    }

                    for(q=tx+i+1; q < p; ++q) {
                        ++k;
                        tx[k] = *q;
                    }
                    ++k;
                    tx[k] = '\n';
                    i += (p-(tx+i)); /*Ratchet up to the ending '>' */
                    continue;
                }
            } else if ('"' == c) {
                /*in a string*/
                p = strchr(tx+i+1, '"');
                if (NULL == p) {
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                for (q=tx+i; q < p; ++q) {
                    if ('\t' == *q) {
                        continue;
                    }
                    if (is_forbidden(*q)) {
                        erri = ORCM_ERR_BAD_PARAM;
                        break;
                    }
                }
                if (ORCM_SUCCESS != erri) {
                    break;
                }

                tx[++k] = '\t'; /*Prepending*/

                for (q=tx+i+1; q < p; ++q) {
                    if ('\t' == *q) {
                        tx[++k] = ' ';
                    } else {
                        tx[++k] = *q;
                    }
                }
                ++k;
                tx[k] = '\n';
                i += (p-(tx+i)); /*Ratchet up to the ending '"' */
                continue;
            } else {
                /*This should only be un-quoted field item */
                /*Yep, putting whitespace in a field item will mangled the text*/
                if (1 != came_from_data) {
                    tx[++k] = '\t';
                }
                ++k;
                tx[k] = c;
                came_from_data = 1;
            }

        } /*for(i=0; i<fsize; ++i) */
        if (ORCM_SUCCESS != erri) {
            break;
        }

        ++k;
        tx[k] = '\0';
        text_length = k;
        k = 0;

        /*Count the number of items found */
        k=0;
        for (i=0; i < text_length; ++i) {
            if ('\n' == tx[i]) {
                ++k;
            }
        }
        if (0 == k) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }
        sz_tem = k;

        /*Set up the item array */
        tem = (char **) calloc(sz_tem, sizeof(char*));
        if (NULL == tem) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        j=0;
        k=0;
        for (i=0; i<text_length; ++i) {
            if ('\n' == tx[i]) {
                tx[i ]= '\0';
                tem[j] = &tx[k];
                ++j;
                k=i+1;
            }
        }
        if (j != sz_tem) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /*Final cleaning up */
        *o_text = tx;
        tx = NULL;

        *o_item_count = sz_tem;
        sz_tem = 0;

        *o_items = tem;
        tem = NULL;

        break;
    }

    if (NULL != fin){
        fclose(fin);
        fin=NULL;
    }
    return erri;
}

/*Returns 1 if the tag given is one of the one used.
 *Returns 0 otherwise.
 */
static int isa_known_tag(const char * in_tagtext)
{
    const char * t = in_tagtext;
    /*First a quick check than a fuller one*/
    if (NULL==t) {
        return 0;
    }
    switch (t[0]) {
    case 'a':
    case 'A':
        if(0 == strcasecmp(t,"aggregator")) {
            return 1;
        }
        break;
    case 'c':
    case 'C':
        if (0 == strcasecmp(t,"configuration") ||
            0 == strcasecmp(t,"controller") ||
            0 == strcasecmp(t,"count") ||
            0 == strcasecmp(t,"cores") ||
            0 == strcasecmp(t,"creation")) {
            return 1;
        }
        break;
    case 'e':
    case 'E':
        if (0 == strcasecmp(t,"envar")) {
            return 1;
        }
        break;
    case 'h':
    case 'H':
        if (0 == strcasecmp(t,"host")) {
            return 1;
        }
        break;
    case 'j':
    case 'J':
        if (0 == strcasecmp(t,"junction")) {
            return 1;
        }
        break;
    case 'l':
    case 'L':
        if (0 == strcasecmp(t,"log")) {
            return 1;
        }
        break;
    case 'm':
    case 'M':
        if (0 == strcasecmp(t,"mca-params")) {
            return 1;
        }
        break;
    case 'n':
    case 'N':
        if (0 == strcasecmp(t,"name")) {
            return 1;
        }
        break;
    case 'p':
    case 'P':
        if (0 == strcasecmp(t,"port")) {
            return 1;
        }
        break;
    case 'q':
    case 'Q':
        if (0 == strcasecmp(t,"queues")) {
            return 1;
        }
        break;
    case 'r':
    case 'R':
        if (0 == strcasecmp(t,"role")) {
            return 1;
        }
        break;
    case 's':
    case 'S':
        if (0 == strcasecmp(t,"scheduler") ||
            0 == strcasecmp(t,"shost")) {
            return 1;
        }
        break;
    case 't':
    case 'T':
        if (0 == strcasecmp(t,"type")) {
            return 1;
        }
        break;
    case 'v':
    case 'V':
        if (0 == strcasecmp(t,"version")) {
            return 1;
        }
        break;
    default:
        break;
    }
    return 0;
}

/*Returns 1 for sectional items
 *Returns 0 otherwise.
 */
static int isa_sectional_item(const char * in_tagtext)
{
    const char * t = in_tagtext;
    if (NULL==t) {
        return 0;
    }
    /*First a quick check than a fuller one*/
    switch (t[0]) {
    case 'c':
    case 'C':
        if (0 == strcasecmp(t, "controller") ||
            0 == strcasecmp(t,"configuration")) {
            return 1;
        }
        break;
    case 'j':
    case 'J':
        if (0 == strcasecmp(t, "junction")) {
            return 1;
        }
        break;
    case 's':
    case 'S':
        if (0 == strcasecmp(t,"scheduler")) {
            return 1;
        }
        break;
    default:
        break;
    }

    return 0;
}

/*A singleton item is one where only one instance of that item can be present
  in the current context.
  Returns 1 for singleton item
 *Returns 0 otherwise.
 */
static int isa_singleton(const char * in_tagtext)
{
    const char * t = in_tagtext;
    /*First a quick check than a fuller one*/
    if (NULL == t) {
        return 0;
    }
    switch (t[0]) {
    case 'a':
    case 'A':
        if (0 == strcasecmp(t,"aggregator")) {
            return 1;
        }
    case 'c':
    case 'C':
        if (0 == strcasecmp(t,"controller") ||
            0 == strcasecmp(t,"count") ||
            0 == strcasecmp(t,"cores") ||
            0 == strcasecmp(t,"creation")) {
            return 1;
        }
        break;
    case 'e':
    case 'E':
        if (0 == strcasecmp(t,"envar")) {
            return 1;
        }
        break;
    case 'h':
    case 'H':
        if (0 == strcasecmp(t,"host")) {
            return 1;
        }
    case 'j':
    case 'J':
        break;
    case 'l':
    case 'L':
        if (0 == strcasecmp(t,"log")) {
            return 1;
        }
        break;
    case 'm':
    case 'M':
        break;
    case 'n':
    case 'N':
        if (0 == strcasecmp(t,"name")) {
            return 1;
        }
        break;
    case 'p':
    case 'P':
        if (0 == strcasecmp(t,"port")) {
            return 1;
        }
        break;
    case 'q':
    case 'Q':
        break;
    case 'r':
    case 'R':
        if (0 == strcasecmp(t,"role")) {
            return 1;
        }
        break;
    case 's':
    case 'S':
        if (0 == strcasecmp(t,"scheduler") ||
            0 == strcasecmp(t,"shost")) {
            return 1;
        }
        break;
    case 't':
    case 'T':
        if( ! strcasecmp(t,"type")) return 1;
        break;
    case 'v':
    case 'V':
        if (0 == strcasecmp(t,"version")) {
            return 1;
        }
        break;
    default:
        break;
    }
    return 0;
}

static int check_validity_of_the_inputs(char ** in_items, unsigned long in_item_count)
{
    unsigned long i=0, k=0;

    char * t; /*text pointer*/
    unsigned long openings=0;
    unsigned long closings=0;

    unsigned long record_count=0;

    unsigned long t_cluster=0;
    unsigned long t_rack=0;
    unsigned long t_row=0;
    unsigned long t_inter=0;
    unsigned long t_node=0;

    int err2=0;
    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (0 == in_item_count) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: No XML items to examine.");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /* Make sure all tags are real ones */
        err2=0;
        for (i=0; i < in_item_count; ++i) {
            t = in_items[i];
            if (NULL == t) {
                opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                    "ERROR: NULL tag found.");
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
            if ('\t' == t[0]) {
                continue;
            }
            if ('/' == t[0]) {
                k=1;
            } else {
                k=0;
            }
            if (!isa_known_tag(t+k)) {
                opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                    "ERROR: Unknown XML tag->%s", t);
                err2 = ORCM_ERR_BAD_PARAM;
            }
        }
        if (ORCM_SUCCESS != err2) {
            erri = err2;
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*Make sure the number of opening tag matches the number of closing tags*/
        /*For non-sectional tags, the closing tag should be close to the
         *opening tag, with a data field in between them.
         */
        err2=0;
        for (i=0; i < in_item_count; ++i) {
            t = in_items[i];
            if ('/' == t[0]) {
                ++closings;
            } else {
                if ('\t' != t[0]) {
                    ++openings;
                }
            }

            if (isa_sectional_item(t)) {
                continue;
            }
            if ('\t' == t[0]) {
                /*Skip data fields.  See below.*/
                continue;
            }

            if ('/' == t[0]) {
                /* We'll focus on going from the opening tag to the closing tag.
                 *So skip the closing tag as they will be found during the search.
                 */
                continue;
            }

            /* We are on a non-sectional opening tag.  The following entry should
             * be a data field, and then, after that, it should be a closing tag.
             */
            k=i+1;
            if (k >= in_item_count || '\t' != in_items[k][0]) {
                opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                    "ERROR: Missing data field for XML tag ->%s", t);
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }

            k=i+2;
            if (k >= in_item_count || '/' != in_items[k][0]) {
                opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                    "ERROR: Missing closing tag for XML tag ->%s", t);
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
        }
        if (ORCM_SUCCESS != err2) {
            erri = err2;
        }
        if (ORCM_SUCCESS == erri) {
            if (openings != closings) {
                opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                    "ERROR: The number of opening and closing XML tags do not match: open=%lu close=%lu", openings,closings);
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
        }
        if (ORCM_SUCCESS != erri){
            break;
        }

        /* Make sure there is only a single RECORD configuration */
        record_count=0;
        for (i=0; i < in_item_count; ++i) {
            t = in_items[i];
            /* Looking for the "role" tag */
            if ('r'!=t[0] && 'R'!=t[0]) {
                continue;
            }
            if (0 != strcasecmp(t,"role")) {
                continue;
            }

            k=i+1;
            /* k overflow and presence of data field should have
               been checked above. */
            t=in_items[k];
            ++t; /*Jump over the '\t' */
            if (0 == strcasecmp(t,"RECORD")){
                ++record_count;
            } else if (0 == strcasecmp(t,"MOD")) {
                /*All good. That is what one would expect. */
            } else {
                opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                    "ERROR: Unknown data content for the XML tag ->role");
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (1 != record_count) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: Incorrect count of XML tag configuration of \"RECORD\" role:%lu",record_count);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /*Making sure that only the allowed junction type are present */
        /*Make sure that only one cluster type is present.*/
        /*Make sure that at least one node is present */
        t_cluster = 0;
        t_rack = 0;
        t_row = 0;
        t_inter = 0;
        t_node = 0;
        for (i=0; i < in_item_count; ++i) {
            t=in_items[i];

            if ('\t' == t[0] || '/' == t[0]) {
                /*Skip ending tags and data fields*/
                continue;
            }

            if ('t' != t[0] && 'T' != t[0]) {
                /*Not the right tag*/
                continue;
            }
            if (0 != strcasecmp(t,"type")) {
                continue;
            }

            k=i+1;
            /* k overflow and presence of data field should have
               been checked above. */
            t=in_items[k];
            ++t; /*Jump over the '\t' */
            if (0 == strcasecmp(t,"cluster")) {
                ++t_cluster;
            } else if (0 == strcasecmp(t, "rack")) {
                ++t_rack;
            } else if (0 == strcasecmp(t, "row")) {
                ++t_row;
            } else if (0 == strcasecmp(t, "node")) {
                ++t_node;
            } else if (0 == strcasecmp(t, "inter")) {
                ++t_inter;
            }
        }

        if (0 != t_inter) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: Junction of type \"inter\" have yet to be implemented.");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if (0 == t_node) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: At least one junction of type \"node\" must be defined.");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /*Check if the aggregator field is really yes or no*/
        for (i=0; i < in_item_count; ++i) {
            t = in_items[i];

            if ('\t' == t[0] || '/' == t[0]) {
                /*Skip ending tags and data fields*/
                continue;
            }

            if ('a' != t[0] && 'A' != t[0]) {
                /*Not the right tag*/
                continue;
            }
            if (0 != strcasecmp(t,"aggregator")) {
                continue;
            }

            t = in_items[i+1];
            t += 1; /*Jump over the tab*/

            if ('y' == t[0] || 'Y' == t[0]) {
                /*All good*/
                break;
            }
            if ('n' == t[0] || 'N' == t[0]) {
                /*All good*/
                break;
            }
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: \"aggregator\" only allow the values: yes, no");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        erri = check_all_port_fields(in_items, in_item_count);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }
    return erri;
}

static int check_all_port_fields(char ** in_items, unsigned long in_item_count)
{
    /*Check that port field content is a positive bound integer */
    unsigned long i=0;
    char *t=NULL;
    char * endptr=NULL;
    const int base = 10;
    long number=0;

    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri){
        for (i=0; i < in_item_count; ++i){
            t = in_items[i];

            if ('\t' == t[0] || '/' == t[0]) {
                /*Skip ending tags and data fields*/
                continue;
            }
            if ('p' != t[0] && 'P' != t[0]) {
                /*Not the right tag*/
                continue;
            }
            if (0 != strcasecmp(t,"port")) {
                continue;
            }

            t=in_items[i+1];
            t+=1; /*Jump over the tab*/

            endptr=NULL;
            number = strtol(t, &endptr, base);

            if ('\0' != *endptr) {
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                     "ERROR: The value of a node item is not a valid integer");
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                     "Error on lexer element %lu", i+1);
                erri = ORCM_ERR_BAD_PARAM;
                /*Do not bail out right away.  Survey all nodes.
                  That gives a chance to see all mistakes. */
                /*break;*/
            } else {
                if (0 > number || USHRT_MAX < number) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                         "ERROR: The value of a node item is not in an acceptable range (0<=n<=SHRT_MAX): %ld",
                                         number);
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                         "Error on lexer element %lu", i+1);
                    erri = ORCM_ERR_BAD_PARAM;
                    /*Do not bail out right away.  Survey all nodes.
                      That gives a chance to see all mistakes. */
                    /*break;*/
                }
            }
        }
        break;
    }
    return erri;
}

static int structure_lexed_items(unsigned long in_begin_offset,
                                 unsigned long in_end_offset,
                                 char ** in_items,
                                 unsigned long in_item_count,
                                 long *** o_hierachy,
                                 unsigned long ** o_hier_row_length,
                                 unsigned long * o_length_hierarchy)
{
    /* in_begin_offset : The start of the offset in in_items where to begin
     *                   the search.
     *                   ==>IMPORTANT: It has to be on a sectional item
     * in_end_offset :  Where to stop the search.
     *                   ==>IMPORTANT: It has include the ending tag of the
     *                                 sectional item refer to by in_begin_offset.
     * in_items: Array of string as produced by lex_xml
     *              ==>IMPORTANT: No empty non-sectional items are allowed.
     * in_item_count : Length of in_items array.
     *
     * o_length_hierarchy --> The length of the array o_hierarchy.
     *                    --> The length of the array o_hier_row_length
     * o_hier_row_length --> An array of lenth one for each row in o_hierarchy.
     * o_hierachy --> an array of arrays of indices.
     * A row in o_hierarchy contains a sequence of signed integers:
     *          A positive integer is an offset into in_items
     *          A negative integer is an offset into o_hierarchy
     *      The first, at offset=0, positive integer points to where that
     *      sectional item starts in in_items.
     *      All other positive integers points to non-sectional entries
     *      in in_items, for that sectional item.
     *          Only beginning tags are pointed to; ending tags and data fields
     *          are not included.  Those are assumed to be there.
     *
     *      All negative integers points to a sectional entry.
     *      So negative integers are the child sections of the current row.
     *      The positive integers are the properties local to the current row.
     *
     */

    /*The algorithm is essentialy a hierarchical search without using recursion
     * or a stack.
     * A little slower maybe but it is real easy to debug.
     */
    const unsigned long nota_index=(unsigned long)(-1);
    int erri=ORCM_SUCCESS;

    unsigned long i=0,k=0;
    unsigned long ending_sectional_tags=0;

    unsigned long sz_section=0;
    unsigned long * section_starts=NULL;
    unsigned long * section_ends=NULL;
    unsigned long * section_parents=NULL;
    unsigned long * section_counts=NULL;

    char * t=NULL;

    *o_length_hierarchy=0;
    *o_hierachy=NULL;

    while (ORCM_SUCCESS == erri){
        if (in_item_count==0) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }
        if (in_begin_offset >= in_end_offset ||
            in_begin_offset >= in_item_count ||
            in_end_offset > in_item_count) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if (!isa_sectional_item(in_items[in_begin_offset])) {
            /*We are not starting on a sectional element*/
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /*First count the number of sections we have */
        k=0;
        ending_sectional_tags=0;
        for (i=in_begin_offset; i < in_end_offset; ++i) {
            t=in_items[i];
            if ('\t' == t[0]) {
                continue;
            }
            if ('/' == t[0]) {
                if (isa_sectional_item(t+1)) {
                    ++ending_sectional_tags;
                }
            }
            if (!isa_sectional_item(t)) {
                continue;
            }
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "Section =%s",t);
            if ('/'==t[0]) {
                ++ending_sectional_tags;
            } else {
                ++k;
            }
        }
        opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                            "Section count=%lu  %lu\n",k, ending_sectional_tags);

        if (k != ending_sectional_tags || 0 == k) {
            /*This crude check to see if we have the ending tag of the starting section*/
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR:Provided sub-section of XML lexer invalid");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        *o_length_hierarchy=k;
        k=0;
        *o_hierachy = (long **) calloc(*o_length_hierarchy, sizeof(long *));
        if (NULL == *o_hierachy) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        sz_section=0;
        section_starts = (unsigned long *) calloc(*o_length_hierarchy, sizeof(unsigned long));
        if (NULL == section_starts) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        section_ends = (unsigned long *) calloc(*o_length_hierarchy, sizeof(unsigned long));
        if (NULL == section_ends) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        section_parents = (unsigned long *) calloc(*o_length_hierarchy, sizeof(unsigned long));
        if (NULL == section_parents) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        section_counts = (unsigned long *) calloc(*o_length_hierarchy, sizeof(unsigned long));
        if (NULL == section_counts) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        /*Find the beginning and the end of each section, and their parent section */

        /*NOTE: Section_parents does not store indices in in_items.
         *      It stores indices pointing in section_starts.
         */

        /*NOTE: The observation here is that an offset of zero in either
         *      section_ends or section_parents is not possible.
         *      In the section_parents, only sections being reporting to
         *      the base section can be zero.
         *      The entry zero of section_parents cannot be defined as
         *      the base section has no parents.
         */

        /*TODO: Optimize the search for the first empty ending.
         *      As it is, we are looking at a worst case O(N^2) parsing.
         *      Using sections_ends as a stack would speed things up.
         */

        /*NOTE: This algorithm is done twice. Once now and once afterward.*/
        section_parents[0]=nota_index;

        sz_section=-1;
        for (i=in_begin_offset; i < in_end_offset; ++i) {
            t=in_items[i];
            if ('\t' == t[0]) { /*Skip data field*/
                continue;
            }
            if ('/' == t[0]) { /*An ending*/
                if (!isa_sectional_item(t+1)) {
                    continue;
                }
            } else if (!isa_sectional_item(t)) {
                /*Not a sectional tag, not an ending tag and not a data field*/
                if ((unsigned long)(-1) == sz_section) {
                    /*I did not expect that here. */
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                for (k=sz_section; k >= 0; --k) {
                    if (0 == section_ends[k]) {
                        /*Found the first empty ending*/
                        break;
                    }
                }
                if (0==k && 0 != section_ends[0]) {
                    /* No empty ending found ???*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                ++section_counts[k];
                continue;
            }

            if ('/' == t[0]) { /*An ending tag of a section */
                for (k=sz_section; k >= 0; --k) {
                    if (0 == section_ends[k]) {
                        /*Found the first empty ending*/
                        break;
                    }
                }
                if (0 == k && 0 != section_ends[0]) {
                    /* No empty ending found ???*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                section_ends[k]=i;
            } else { /*A beginning tag of a section */
                ++sz_section;
                section_starts[sz_section]=i;
                section_ends[sz_section]=0;
                if (0 != sz_section) {
                    /*Find first empty ending */
                    for (k=sz_section-1;  k>= 0; --k) {
                        if (0 == section_ends[k]) {
                            /*Found the first available parent*/
                            section_parents[sz_section] = k;
                            break;
                        }
                    }
                    if (0 == k && 0 != section_ends[0]) {
                        /* No parent were found ???*/
                        erri = ORCM_ERR_BAD_PARAM;
                        break;
                    }
                }
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (sz_section+1 != *o_length_hierarchy) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "DEBUG:: sz_section=%lu    o_length_hierarchy=%lu\n",
                                sz_section, *o_length_hierarchy);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /*Add to the count the number of childrens*/
        *o_hier_row_length = (unsigned long *) calloc(*o_length_hierarchy, sizeof(unsigned long));
        if (NULL == *o_hier_row_length) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }
        memcpy(*o_hier_row_length,section_counts,(*o_length_hierarchy)*sizeof(unsigned long));

        /*Add the sub-sectional items */
        for (k=0; k < *o_length_hierarchy; ++k) {
            if (nota_index == section_parents[k]) {
                continue;
            }
            i = section_parents[k];
            ++(*o_hier_row_length)[i];
        }
        /*Add the space for section_starts */
        for (k=0; k < *o_length_hierarchy; ++k) {
            ++(*o_hier_row_length)[k];
        }

        /*Allocate the memory for the entries in o_hierachy*/
        for (k=0; k < *o_length_hierarchy; ++k) {
            i = (*o_hier_row_length)[k];
            (*o_hierachy)[k] = (long *) calloc(i, sizeof(long));
            if (NULL == (*o_hierachy)[k]) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }
        }
        if (ORCM_SUCCESS != erri){
            break;
        }


        /*Fill each row of o_hierarchy up */
        memset(section_ends, 0, (*o_length_hierarchy)*sizeof(unsigned long));
        memset(section_counts, 0, (*o_length_hierarchy)*sizeof(unsigned long));

        /*Add the section_starts to the hierarchy */
        for (k=0; k < *o_length_hierarchy; ++k) {
            (*o_hierachy)[k][0] = section_starts[k];
            ++section_counts[k];
        }

        /*Do the same as above but only to get the non-sectional items*/
        /*No need to recalculate the section_starts.  Keep them for later*/
        /*No need to recalculate the section_parents.  Keep them for later*/
        /*section_counts & sections_ends are re-built*/

        sz_section=-1;
        for (i=in_begin_offset; i < in_end_offset; ++i) {
            t = in_items[i];
            if ('\t' == t[0]) { /*Skip data field*/
                continue;
            }
            if ('/' == t[0]) { /*An ending*/
                if (!isa_sectional_item(t+1)) {
                    continue;
                }
            }else if (!isa_sectional_item(t)) {
                /*Not a sectional tag, not an ending tag and not a data field*/
                if ((unsigned long)(-1) == sz_section) {
                    /*I did not expect that here. */
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                for (k=sz_section; k >= 0; --k) {
                    if (0 == section_ends[k]) {
                        /*Found the first empty ending*/
                        break;
                    }
                }
                if (0 == k && 0 != section_ends[0]) {
                    /* No empty ending found ???*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                (*o_hierachy)[k][section_counts[k]] = i;
                ++section_counts[k];
                continue;
            }

            if ('/'==t[0]) { /*An ending tag of a section */
                for (k=sz_section; k >= 0; --k) {
                    if (0 == section_ends[k]) {
                        /*Found the first empty ending*/
                        break;
                    }
                }
                if (0 == k && 0!=section_ends[0]) {
                    /* No empty ending found ???*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                section_ends[k]=i;
            } else { /*A beginning tag of a section */
                ++sz_section;
                section_ends[sz_section]=0;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*Add the missing childrens */
        /*This will consume section_counts */
        for (k=0; k < *o_length_hierarchy; ++k) {
            if (nota_index == section_parents[k]){
                continue;
            }
            i = section_parents[k];
            (*o_hierachy)[i][section_counts[i]] = (-1L)*k;
            ++section_counts[i];
        }

        break;
    }/* while(!erri) */

    if (NULL != section_starts) {
        free(section_starts);
        section_starts=NULL;
        sz_section=0;
    }
    if (NULL != section_ends) {
        free(section_ends);
        section_ends=NULL;
        sz_section=0;
    }
    if (NULL != section_parents) {
        free(section_parents);
        section_parents=NULL;
        sz_section=0;
    }
    if (NULL != section_counts) {
        free(section_counts);
        section_counts=NULL;
        sz_section=0;
    }

    return erri;
}

static int transfer_to_controller(char ** in_items,
                                  unsigned long in_hier_row_length,
                                  long * in_hierarchy,
                                  orcm_cfgi_xml_parser_t * io_parent)
{
    orcm_cfgi_xml_parser_t * xml=NULL;
    unsigned long j=0;
    long uu=0;
    unsigned long text_length=0;
    char comma = ',';

    char *tt=NULL, *txt=NULL;

    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri){
        xml = NULL;
        xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
        if (NULL == xml) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        opal_list_append(&io_parent->subvals, &xml->super);

        xml->name = strdup("controller");

        /* Adjust the parent */
        io_parent = xml;
        xml=NULL;

        /*Make sure the controller does not have some sub-level */
        for (j=1; j < in_hier_row_length; ++j) {
            uu=in_hierarchy[j];
            if (0 > uu) {
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
        }
        if (ORCM_SUCCESS != erri){
            break;
        }

        /*First resolve the host */
        for (j=1; j < in_hier_row_length; ++j) {
            uu = in_hierarchy[j];
            tt = in_items[uu];
            if ('h' != tt[0] && 'H' != tt[0]) {
                continue;
            }
            if (0 == strcasecmp(tt,"host")) {
                if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&io_parent->value, in_items[uu+1]+1))) {
                    break;
                }
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*Then handle the <mca_params> */
        /*This code is identical to the one used in transfer_to_scheduler*/
        text_length = 0;
        for (j=1; j < in_hier_row_length; ++j) {
            uu = in_hierarchy[j];
            if (0 > uu){
                continue;
            } else {
                tt = in_items[uu];
                if ('m' != tt[0] && 'M' != tt[0]) {
                    continue;
                }
                if (0 == strcasecmp(tt,"mca-params")) {
                    /*Add 1 for either the comma or the ending '\0'*/
                    text_length += strlen(in_items[uu+1]+1) +1;
                }
            }
        }

        if (0 < text_length){
            txt = (char*) calloc(text_length, sizeof(char));
            if (NULL == txt) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }

            comma = ',';
            for (j=1; j < in_hier_row_length; ++j) {
                if (j+1 == in_hier_row_length) {
                    comma = '\0';
                }
                uu=in_hierarchy[j];
                if (0 > uu){
                    continue;
                } else {
                    tt=in_items[uu];
                    if ('m' != tt[0] && 'M' != tt[0]) {
                        continue;
                    }
                    if (0 == strcasecmp(tt,"mca-params")) {
                        text_length = strlen(txt);
                        sprintf(txt+text_length, "%s%c", in_items[uu+1]+1, comma);
                    }
                }
            }

            xml = NULL;
            xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
            if (NULL == xml) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }
            opal_list_append(&io_parent->subvals, &xml->super);

            xml->name = strdup("mca-params");
            if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&xml->value, txt))) {
                free(txt);
                break;
            }
            free(txt);

        } /*if(text_length)*/

        /*Then the other fields */
        for (j=1; j < in_hier_row_length; ++j) {
            uu = in_hierarchy[j];
            if (0 > uu) {
                continue;
            }
            tt = in_items[uu];
            xml = NULL;

            if (0 == strcasecmp(tt,"host")) {
                /*Dealt with before*/
                continue;
            } else if (0 == strcasecmp(tt,"port") ||
                       0 == strcasecmp(tt,"cores") ||
                       0 == strcasecmp(tt,"log") ||
                       0 == strcasecmp(tt,"envar") ||
                       0 == strcasecmp(tt,"aggregator")) {
                xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
                if (NULL == xml) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                opal_list_append(&io_parent->subvals, &xml->super);

                xml->name = strdup(tt);
                tolower_cstr(xml->name);

                if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&xml->value, in_items[uu+1]+1))) {
                    break;
                }

            } else if (0 == strcasecmp(tt,"mca-params")) {
                /*Dealt with before */
                continue;
            } else {
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
        } /*for(j=1; j!=hier_row_length[i]; ++j)*/
        if (ORCM_SUCCESS != erri) {
            break;
        }
        break;
    }
    return erri;
}
static int transfer_to_scheduler( char ** in_items,
                                  unsigned long in_hier_row_length,
                                  long * in_hierarchy,
                                  orcm_cfgi_xml_parser_t * io_parent)
{
    orcm_cfgi_xml_parser_t * xml=NULL;
    unsigned long j=0;
    long uu=0;
    unsigned long queues_count=0;

    char *tt=NULL, *txt=NULL;

    unsigned long text_length=0;
    char comma = ',';

    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri){
        xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
        if (NULL == xml) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        opal_list_append(&io_parent->subvals, &xml->super);

        xml->name = strdup("scheduler");

        /* Adjust the parent */
        io_parent = xml;
        xml=NULL;

        /*Make sure the scheduler does not have some sub-level */
        for (j=1; j < in_hier_row_length; ++j) {
            uu=in_hierarchy[j];
            if (0 > uu) {
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
        }
        if (ORCM_SUCCESS != erri){
            break;
        }

        /*Then handle the <queues> */
        queues_count = 0;
        for (j=1; j < in_hier_row_length; ++j) {
            uu = in_hierarchy[j];
            if (0 > uu) {
                continue;
            } else {
                tt = in_items[uu];
                if ('q' != tt[0] && 'Q' != tt[0]){
                    continue;
                }
                if (0 == strcasecmp(tt,"queues") ){
                    ++queues_count;
                }
            }
        }

        if (0 < queues_count) {
            xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
            if (NULL == xml) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }
            opal_list_append(&io_parent->subvals, &xml->super);

            xml->name = strdup("queues");

            for (j=1; j < in_hier_row_length; ++j) {
                uu = in_hierarchy[j];
                if (0 > uu) {
                    continue;
                } else {
                    tt = in_items[uu];
                    if ('q' != tt[0] && 'Q' != tt[0]) {
                        continue;
                    }
                    if (0 == strcasecmp(tt,"queues")) {
                        if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&xml->value, in_items[uu+1]+1))) {
                            break;
                        }
                    }
                }
            }
            if (ORCM_SUCCESS != erri) {
                break;
            }
        } /*if(queues_count)*/

        /*Then handle the <mca_params> */
        /*This code is identical to the one used in transfer_to_controller*/
        text_length=0;
        for (j=1; j < in_hier_row_length; ++j) {
            uu=in_hierarchy[j];
            if (0 > uu){
                continue;
            } else {
                tt = in_items[uu];
                if ('m' != tt[0] && 'M' != tt[0]){
                    continue;
                }
                if (0 == strcasecmp(tt,"mca-params")) {
                    /*Add 1 for either the comma or the ending '\0'*/
                    text_length += strlen(in_items[uu+1]+1) + 1;
                }
            }
        }

        if (0 < text_length){
            txt = (char*) calloc(text_length, sizeof(char));
            if (NULL == txt) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }

            comma=',';
            for (j=1; j < in_hier_row_length; ++j) {
                if (j+1 == in_hier_row_length) {
                    comma='\0';
                }
                uu=in_hierarchy[j];
                if (0 > uu) {
                    continue;
                } else {
                    tt=in_items[uu];
                    if ('m' != tt[0] && 'M' != tt[0]) {
                        continue;
                    }
                    if (0 == strcasecmp(tt,"mca-params")) {
                        text_length = strlen(txt);
                        sprintf(txt+text_length, "%s%c" , in_items[uu+1]+1, comma);
                    }
                }
            }

            xml = NULL;
            xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
            if (NULL == xml) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }
            opal_list_append(&io_parent->subvals, &xml->super);

            xml->name = strdup("mca-params");
            if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&xml->value, txt))) {
                break;
            }
            txt=NULL;
        } /*if(text_length)*/

        /*The handle the shost*/
        for (j=1; j < in_hier_row_length; ++j) {
            uu=in_hierarchy[j];
            if (0 > uu) {
                continue;
            } else {
                tt=in_items[uu];
                if ('s' != tt[0] && 'S' != tt[0]) {
                    continue;
                }
                if (0 == strcasecmp(tt,"shost")) {
                    xml = NULL;
                    xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
                    if (NULL == xml) {
                        erri = ORCM_ERR_OUT_OF_RESOURCE;
                        break;
                    }
                    opal_list_append(&io_parent->subvals, &xml->super);

                    xml->name = strdup("node");

                    txt=NULL;
                    txt = strdup(in_items[uu+1]+1);
                    if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&xml->value, txt))) {
                        break;
                    }
                    txt=NULL;
                }
            }
        }

        /*Then the ports */
        for (j=1; j < in_hier_row_length; ++j) {
            uu=in_hierarchy[j];
            if (0 > uu) {
                continue;
            }
            tt=in_items[uu];
            if ('p' != tt[0] && 'P' != tt[0]) {
                continue;
            }

            if (0 == strcasecmp(tt,"port")) {
                xml = NULL;
                xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
                if (NULL == xml){
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                opal_list_append(&io_parent->subvals, &xml->super);

                xml->name = strdup(tt);
                tolower_cstr(xml->name);

                txt=NULL;
                txt = strdup(in_items[uu+1]+1);
                if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&xml->value, txt))) {
                    break;
                }
                txt=NULL;

            }
        } /*for(j=1; j!=hier_row_length[i]; ++j)*/
        if (ORCM_SUCCESS != erri) {
            break;
        }
        break;
    }
    return erri;
}

static int remove_empty_items(char ** io_items, unsigned long * io_sz_items)
{
    unsigned long i=0,k=0;
    char * t=NULL;
    char * tt=NULL;

    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (!io_sz_items || !io_items) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }
        if (0 == *io_sz_items) {
            break;
        }

        for (i=0; i < *io_sz_items; ++i) {
            t = io_items[i];
            if (NULL == t){
                continue;
            }
            if ('\t' == t[0] || '/' == t[0]) {
                continue;
            }

            if (i+1 == *io_sz_items) {
                continue;
            }

            tt = io_items[i+1];
            if ('\t' == tt[0]) {
                continue;
            }

            ++tt; /*Jump over the foward slash*/
            if (0 == strcasecmp(t,tt)) {
                /*Empty field found*/
                io_items[i] = NULL;
                io_items[i+1] = NULL;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*Remove nulled entries */
        k=-1;
        for (i=0; i < *io_sz_items; ++i) {
            t=io_items[i];
            if (NULL != t) {
                ++k;
                io_items[k]=io_items[i];
            }
        }
        *io_sz_items = k+1;

        break;
    }
    return erri;
}

static int remove_duplicate_singletons(char ** in_items,
                                       long ** io_hierarchy,
                                       unsigned long * io_hier_row_length,
                                       unsigned long * io_sz_hierarchy)
{
    unsigned long i=0, j=0, k=0;
    long u=0, v=0;
    char * t=NULL, *tt=NULL, *context=NULL;

    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        for (i=0; i < *io_sz_hierarchy; ++i) {
            context = in_items[ io_hierarchy[i][0] ];
            for (j=0; j < io_hier_row_length[i]; ++j) {
                u = io_hierarchy[i][j];
                if (0 > u) {
                    /*=====  Sectional items */
                    t=in_items[io_hierarchy[-u][0]];
                    if (!isa_singleton(t)) {
                        continue;
                    }
                    for (k=j+1; k < io_hier_row_length[i]; ++k) {
                        v=io_hierarchy[i][k];
                        if (0 <= v) {
                            erri = ORCM_ERR_BAD_PARAM;
                            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                                "ERROR: XML parser inconsistency found: %d", __LINE__);
                            break;
                        }
                        tt=in_items[ io_hierarchy[-v][0] ];
                        if (0 == strcasecmp(t,tt)) {
                            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                                "ERROR: More than one instance of the command \"%s\" was found in \"%s\"",tt,context);
                            erri = ORCM_ERR_BAD_PARAM;
                            break;
                        }
                    }
                    if (ORCM_SUCCESS != erri) {
                        break;
                    }

                } else {
                    /*===== non-Sectional items */
                    t=in_items[u];
                    if (!isa_singleton(t)) {
                        continue;
                    }
                    for (k=j+1; k < io_hier_row_length[i]; ++k) {
                        v=io_hierarchy[i][k];
                        if (0 > v) {
                            /*No need to check sectional items */
                            break;
                        }
                        tt=in_items[v];
                        if (0 == strcasecmp(t,tt)) {
                            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                                "ERROR: More than one instance of the command \"%s\" was found in \"%s\"",tt,context);
                            erri = ORCM_ERR_BAD_PARAM;
                            break;
                        }
                    }
                    if (ORCM_SUCCESS != erri) {
                        break;
                    }
                }
            }
            if (ORCM_SUCCESS != erri) {
                break;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }
    return erri;
}

static char * get_name_field( unsigned long in_which_row, char ** in_items,
                              long ** in_hierarchy, unsigned long * in_hier_row_length,
                              unsigned long in_sz_hierarchy )
{
    char * name_found = NULL;

    unsigned long i = 0;
    long u = 0;
    char *t = NULL;

    if (in_which_row >= in_sz_hierarchy){
        /*No way we'll find a name*/
        return name_found;
    }

    for (i=0; i < in_hier_row_length[in_which_row]; ++i) {
        u = in_hierarchy[in_which_row][i];
        if (0>u) {
            continue;
        }
        t = in_items[u];

        if ( 0 == strcasecmp(t,"name") ) {
            t = in_items[u+1];
            ++t; /*Jump over the tab marker*/
            name_found = t;
            break;
        }
    }

    return name_found;
}

static int check_junctions_have_names( char ** in_items, long ** in_hierarchy,
                                       unsigned long * in_hier_row_length,
                                       unsigned long in_sz_hierarchy)
{
    unsigned long i = 0;
    char * t = NULL;
    char *name_found = NULL;
    long u = 0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        for (i = 0; i < in_sz_hierarchy; ++i) {
            u = in_hierarchy[i][0];
            if (0>u) {
                continue;
            }
            t = in_items[ u ];

            if ( 0 != strcasecmp(t,"junction") ) {
                continue;
            }

            name_found = get_name_field( i, in_items, in_hierarchy,
                                         in_hier_row_length, in_sz_hierarchy);

            if (NULL == name_found) {
                opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                    "ERROR: A junction (Lexer item %ld) is missing its name", u);
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }
    return erri;
}

static int check_parent_has_uniquely_named_children( char ** in_items,
                                                     long ** in_hierarchy,
                                                     unsigned long * in_hier_row_length,
                                                     unsigned long in_sz_hierarchy)
{
    unsigned long i = 0, j = 0, k = 0;
    long u = 0, v = 0, context = 0;
    char *t = NULL, *tt = NULL;
    char *cA_name = NULL, *cB_name = NULL;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        /*First find a parent junction*/
        for (i=0; i < in_sz_hierarchy; ++i) {
            context= in_hierarchy[i][0];
            if ( 0 <= context ) {
                continue;
            }
            t = in_items[ -context ];

            if (0 != strcasecmp(t,"junction")) {
                continue;
            }

            /*Now look for a child junction */
            for (j=0; j < in_hier_row_length[i]; ++j) {
                u = in_hierarchy[i][j];
                if (0<=u) {
                    continue;
                }

                tt = in_items[ in_hierarchy[-u][0] ];
                if ( 0 != strcasecmp(tt,"junction")) {
                    continue;
                }

                cA_name = get_name_field( -u, in_items, in_hierarchy,
                                          in_hier_row_length, in_sz_hierarchy );
                if ( NULL == cA_name) {
                    /*Child without a name --> That should have been checked*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                for (k=j+1; k<in_hier_row_length[i]; ++k){
                    /*Look at the other children, if any */
                    v = in_hierarchy[i][k];
                    if (0<=v) {
                        continue;
                    }

                    tt = in_items[ in_hierarchy[-v][0] ];
                    if ( 0 != strcasecmp(tt,"junction")) {
                        continue;
                    }
                    cB_name = get_name_field( -v, in_items, in_hierarchy,
                                              in_hier_row_length, in_sz_hierarchy );
                    if ( NULL == cB_name) {
                        /*Child without a name --> That should have been checked*/
                        erri = ORCM_ERR_BAD_PARAM;
                        break;
                    }
                    if ( 0 == strcmp(cA_name, cB_name) ) {
                        opal_output_verbose( V_LO, orcm_cfgi_base_framework. framework_output,
                                            "ERROR: Junction %lu has two children with the same name: %s", i, cB_name);
                        erri = ORCM_ERR_BAD_PARAM;
                        break;
                    }
                }
                if (ORCM_SUCCESS != erri) {
                    break;
                }
            }/*for(j=0; j!=io_hier_row_length[i]; ++j)*/

            /* Do not bail out right away in order to try flagging as many errors
             * as possible.
             *
             * if (ORCM_SUCCESS != erri) {
             *     break;
             * }
             */
        }/*for(i=0; i!=*io_sz_hierarchy; ++i)*/
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }/*while(!erri)*/
    return erri;
}

