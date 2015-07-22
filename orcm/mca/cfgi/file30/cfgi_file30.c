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

/* These defines all the XML tags that are recognized. */
#define TXaggregat      "aggregator"
#define TXconfig        "configuration"
#define TXcontrol       "controller"
#define TXcount         "count"
#define TXcores         "cores"
#define TXcreation      "creation"
#define TXenvar         "envar"
#define TXhost          "host"
#define TXjunction      "junction"
#define TXlog           "log"
#define TXmca_params    "mca-params"
#define TXname          "name"
#define TXport          "port"
#define TXqueues        "queues"
#define TXrole          "role"
#define TXscheduler     "scheduler"
#define TXshost         "shost"
#define TXtype          "type"
#define TXversion       "version"

/*These define field value associated with their respective XML tag.*/
#define FDcluster       "cluster"
#define FDrow           "row"
#define FDrack          "rack"
#define FDnode          "node"
#define FDinter         "inter"
#define FDrecord        "record"
#define FDmod           "mod"

typedef enum xml_tree_constants_t
{
    xtBLOCK_SIZE = 32UL, /* The size of an allocation block
                          * See append_spot's in_block_size,
                          * and xml_tree_alloc_* functions.
                          */
    xtNOTA_CHILD = -1 /* Used by get_first_child() */
} xml_tree_constants_t;

typedef struct xml_tree_t
{
    /* Variable definitions:
     * sz_items : Length of items array.
     * items: Array of string as produced by lex_xml
     *        To allocate use the private function: xml_tree_alloc_items
     *        ==>IMPORTANT: No empty non-sectional items are allowed.
     *
     * sz_hierarchy --> The length of the array hierarchy.
     *              --> The length of the array o_hier_row_length
     * hier_row_lengths --> An array of length, one for each row in hierarchy.
     *                      To allocate use the private function:
     *                          xml_tree_alloc_hier_row_lengths
     * hierachy --> an array of arrays of indices.
     *              To allocate use the private function:
     *                  xml_tree_alloc_hierarchy
     * A row in hierarchy contains a sequence of signed integers:
     *          A positive integer is an offset into items
     *          A negative integer is an offset into hierarchy
     *      The first, at offset=0, positive integer points to where that
     *      sectional item starts in items.
     *      All other positive integers points to non-sectional entries
     *      in items, for that sectional item.
     *          Only beginning tags are pointed to; ending tags and data fields
     *          are not included.  Those are assumed to be there.
     *
     *      All negative integers points to a sectional entry.
     *      So negative integers are the child sections of the current row.
     *      The positive integers are the properties local to the current row.
     *
     *Typical USAGE:
     *  char input_xml_filename[]= "abc.xml"
     *  int erri = ORCM_SUCCESS;
     *  xml_tree_t xmltree;
     *  erri = xml_tree_create(&xmltree);
     *  if (ORCM_SUCCESS != erri) --> error
     *  erri = lex_xml(input_xml_filename, &xmltree);
     *  if (ORCM_SUCCESS != erri) --> error
     *  erri = structure_lexed_items(0, xmltree.sz_items, xmltree);
     *  if (ORCM_SUCCESS != erri) --> error
     *  ... proceed with the obtained XML syntax tree xmltree.
     *  xml_tree_destroy(&xmltree);
     */
    unsigned long sz_items; /* The length of items                           */
    char ** items; /*An array of c-strings             cstrings & array owned*/
    unsigned long sz_hierarchy;
    unsigned long * hier_row_lengths;                /* array owned */
    long ** hierarchy;                               /* all arrays owned */
} xml_tree_t;

static void xml_tree_destroy(xml_tree_t * io_tree);
static int xml_tree_create(xml_tree_t * io_tree);

/*These private functions standardized the memory allocation.
 *That way each array can be easily extended with the function
 *    xml_tree_grow(...)
 */
static int xml_tree_alloc_items(xml_tree_t * io_tree);
static int xml_tree_alloc_hierarchy(xml_tree_t * io_tree);
static int xml_tree_alloc_hier_row_lengths(xml_tree_t * io_tree);
static int xml_tree_grow(unsigned long in_data_size,unsigned long in_block_size,
                         unsigned long in_sz_array, void * *io_array);

/* get_field(...) gets the field string associated with a given xml tag.
 * It returns the c-string without transfering ownership.
 * If the xml tag is not found, NULL is returned.
 */
static const char * get_field( const char * in_xml_tag, unsigned long in_which_row,
                               const xml_tree_t * in_tree );

/* get_field_from_tag(...) returns NULL if the given tag offset does not have
 * an associated tag; otherwise, it returns the cstring of the field associated
 * with the tag pointed to in in_tree.items by in_tag_offset_in_items.
 */
static const char * get_field_from_tag(long in_tag_offset_in_items,
                                       const xml_tree_t * in_tree);

/*in_item_offset is contained within xml_tree_t::hierarchy */
static bool is_field(long in_hierarchy_index);
static bool is_child(long in_hierarchy_index);
static long child_to_offset(long in_hierarchy_index);

/* get_first_child returns the row offset into in_tree->hierarchy
 * associated with the child's in_xml_tag.
 * xtNOTA_CHILD is returned if nothing is found.
 */
static long get_first_child(const char * in_child_xml_tag,
                            unsigned long in_parent_row,
                            const xml_tree_t * in_tree );

static void xml_tree_print_lexer(const xml_tree_t * in_tree);
static void xml_tree_print_structure(const xml_tree_t * in_xtree);

static int check_junctions_have_names(xml_tree_t * in_tree);
static int check_parent_has_uniquely_named_children( xml_tree_t * in_tree );
static int check_host_field_has_matching_port( xml_tree_t * in_tree );

static int check_unique_scheduler( xml_tree_t * in_tree );
static int check_no_sublevel_on_scheduler( xml_tree_t * in_tree );

static int check_unique_host_port_pair( xml_tree_t * in_tree );

static int check_unique_cluster_on_top( xml_tree_t * in_tree );
static int check_no_sublevel_on_controller( xml_tree_t * in_tree );

static bool is_regex(const char * in_cstring);
static int check_controller_on_node_junction( xml_tree_t * in_tree );
/*Check that row are before rack */
/*Note that this check only makes sense in a 4 tiers hierarchy */
/*So check that we have a 4-tiers hierarchy at the same time. */
/*This also check if node junction are the only leaf junctions. */
static int check_hierarchy_integrity( xml_tree_t * in_tree );

static int lex_xml(char * in_filename, xml_tree_t * io_xtree);

static int check_lex_tags_valid(xml_tree_t * in_xtree);
static int check_lex_open_close_tags_matching(xml_tree_t * in_xtree);
static int check_lex_single_record(xml_tree_t * in_xtree);
static int check_lex_allowed_junction_type(xml_tree_t * in_xtree);
static int check_lex_all_port_fields(xml_tree_t * in_xtree);
static int check_lex_aggregator_yes_no(xml_tree_t * in_xtree);

static int is_sectional_item(const char * in_tagtext);
static int find_beginend_configuration(unsigned long in_start_index,
                                       char ** in_items,
                                       unsigned long in_sz_items,
                                       int in_1for_record_0for_mod,
                                       unsigned long * o_begin_config,
                                       unsigned long * o_end_config);
static int structure_lexed_items(unsigned long in_begin_offset,
                                 unsigned long in_end_offset,
                                 xml_tree_t * io_xtree);

static int augment_hierarchy( xml_tree_t * io_xtree );

static void set_parent_isa_node(bool * io_parent_isa_node, const xml_tree_t * in_xtree);

static int transfer_mca_params(unsigned long in_row_in_hier,
                               const xml_tree_t * in_xtree,
                               orcm_cfgi_xml_parser_t * io_parent);
static int transfer_other_items(unsigned long in_row_in_hier,
                                const xml_tree_t * in_xtree,
                                orcm_cfgi_xml_parser_t * io_parent);

static int transfer_to_controller(unsigned long in_ctrl_row,
                                  const xml_tree_t * in_xtree,
                                  orcm_cfgi_xml_parser_t * io_parent);

static int transfer_queues_to_scheduler(unsigned long in_sched_row,
                                        const xml_tree_t * in_xtree,
                                        orcm_cfgi_xml_parser_t * io_parent);

static int transfer_to_scheduler(unsigned long in_sched_row,
                                 const xml_tree_t * in_xtree,
                                 orcm_cfgi_xml_parser_t * io_parent);

static int transfer_to_node(unsigned long in_node_row,
                            const xml_tree_t * in_xtree,
                            orcm_cfgi_xml_parser_t * io_parent);

static int remove_empty_items(xml_tree_t * io_xtree);

static int is_singleton(const char * in_tagtext);
static int check_duplicate_singletons(xml_tree_t * in_xtree);

void replace_ampersand(char** io_name_to_modify, char * in_parent_name);

static int file30_init(void)
{
    xml_tree_t xml_syntax;

    char * data=NULL;
    unsigned long i=0;

    int version_found=0; /*Set to 1 if found*/

    int erri = ORCM_SUCCESS;

    while (ORCM_SUCCESS == erri){
        erri = xml_tree_create(&xml_syntax);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (NULL == orcm_cfgi_base.config_file) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output, "NULL FILE");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        erri = lex_xml(orcm_cfgi_base.config_file, &xml_syntax);

        if (ORCM_SUCCESS != erri) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "FAILED TO PARSE XML CFGI FILE");
            break;
        }

        /* Now look for the first version data field */
        for (i=0; i!=xml_syntax.sz_items; ++i) {
            if ('/' == xml_syntax.items[i][0]) {
                continue;
            }
            if ('\t' == xml_syntax.items[i][0]) {
                continue;
            }
            if (0 == strcasecmp(TXversion, xml_syntax.items[i])) {
                if (i+1 >= xml_syntax.sz_items) {
                    opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                        "MISSING VERSION DATA");
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                data=xml_syntax.items[i+1];
                if ('\t' != data[0]) {
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

    xml_tree_destroy(&xml_syntax);

    if (ORCM_SUCCESS != erri) {
        return ORCM_ERR_TAKE_NEXT_OPTION;
    }

    return ORCM_SUCCESS;
}

static void file30_finalize(void)
{
}

static int parse_config(xml_tree_t * io_xtree, opal_list_t *io_config);
static bool check_me(orcm_config_t *config, char *node,
                     orte_vpid_t vpid, char *my_ip);

static int read_config(opal_list_t *config)
{
    FILE * fp=NULL;

    orcm_cfgi_xml_parser_t *xml; //An iterator, owns nothing.

    xml_tree_t xml_syntax;

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
        erri = xml_tree_create(&xml_syntax);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = lex_xml(orcm_cfgi_base.config_file, &xml_syntax);
        if (ORCM_SUCCESS != erri) {
            opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                "FAILED TO XML LEX THE FILE");
            break;
        }

        erri = remove_empty_items(&xml_syntax);
        if (ORCM_SUCCESS != erri) {
            opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                                "FAILED TO REMOVE EMPTY FIELDS IN XML CFGI");
            break;
        }

        if (V_HIGHER < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
            xml_tree_print_lexer(&xml_syntax);
        }
        erri = check_lex_tags_valid(&xml_syntax);
        if (ORCM_SUCCESS != erri) {
            break;
        }
        erri = check_lex_open_close_tags_matching(&xml_syntax);
        if ( ORCM_SUCCESS != erri ) {
            break;
        }
        erri = check_lex_single_record(&xml_syntax);
        if ( ORCM_SUCCESS != erri ) {
            break;
        }
        erri = check_lex_allowed_junction_type(&xml_syntax);
        if ( ORCM_SUCCESS != erri ) {
            break;
        }
        erri = check_lex_aggregator_yes_no(&xml_syntax);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = check_lex_all_port_fields(&xml_syntax);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        opal_output_verbose(V_HI, orcm_cfgi_base_framework.framework_output,
                            "XML lexer-level VALIDITY CHECKED and PASSED");

        erri = parse_config(&xml_syntax, config);
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

    xml_tree_destroy(&xml_syntax);

    if (ORCM_SUCCESS != erri) {
        return ORCM_ERR_SILENT;
    }

    return ORCM_SUCCESS;
}

static void setup_environ(char **env);
static int parse_cluster(orcm_cluster_t *cluster, orcm_cfgi_xml_parser_t *x);
static int parse_scheduler(orcm_scheduler_t *scheduler,
                           orcm_cfgi_xml_parser_t *x);

static int define_system(opal_list_t *config,    orcm_node_t **mynode,
                         orte_vpid_t *num_procs, opal_buffer_t *buf)
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
                if ('c' != t[0] && 'C' != t[0]) {
                    continue;
                }
                if (0 != strcasecmp(t,TXconfig)) {
                    continue;
                }
                /*configuration found*/
                beg=i;
            } else {
                /*Make sure the found configuration is a RECORD one */
                if ('/' == t[0] || '\t' == t[0]) {
                    /*Skip data field and closing tags*/
                    continue;
                }
                if (0 == strcasecmp(t,TXrole)) {
                    if ('\t' != in_items[i+1][0]) {
                        opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                            "ERROR: Parser error at tag \"%s\"", t);
                        erri = ORCM_ERR_BAD_PARAM;
                        break;
                    }
                    t = in_items[i+1];
                    if (0 == strcasecmp(t+1,FDrecord)) {
                        if (in_1for_record_0for_mod == 1) {
                            /*We found what we were looking for*/
                            /*So stop searching*/
                            break;
                        } else {
                            /*We are looking for the RECORD configuration.*/
                            /*So restart the search*/
                            beg = nota_index;
                        }
                     } else if (0 == strcasecmp(t+1,FDmod)) {
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
                } else if (0 == strcasecmp(t,TXversion)) {
                    continue;
                } else if (0 == strcasecmp(t,TXcreation)) {
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
            if (0 == strcasecmp(t,TXconfig)) {
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
    while (NULL != t && '\0' != *t) {
        if ('A' >= *t && 'Z' <= *t ) {
            *t -= 'A';
            *t += 'a';
        }
        ++t;
    }
    return in;
}

static void xml_tree_print_structure( const xml_tree_t * in_xtree )
{
    const unsigned int BUFSZ = 1024;

    unsigned long i=0, j=0;

    unsigned long sz_txt = 0, sz=0;
    char * txt = NULL;
    char * t = NULL;
    char buf[BUFSZ];
    char c = '\0';

    if (0 == in_xtree->sz_hierarchy ||
        0 == in_xtree->sz_items ||
        NULL == in_xtree->hier_row_lengths ||
        NULL == in_xtree->hierarchy) {
        return;
    }

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        for (i=0; i < in_xtree->sz_hierarchy; ++i) {
            /* First find out the length of individual lines. */
            sz=0;

            snprintf(buf, BUFSZ, "XML-STRUCT: %lu> ", i);
            sz += strlen(buf);

            for (j=0; j < in_xtree->hier_row_lengths[i]; ++j){

                long here = in_xtree->hierarchy[i][j];

                if (is_child(here)) {
                        snprintf(buf, BUFSZ, "%ld\t", child_to_offset(here) );
                        sz += strlen(buf);
                    } else {
                        const char * tx = in_xtree->items[here];
                        if (is_sectional_item(tx)) {
                            snprintf(buf, BUFSZ, "%s\t", in_xtree->items[here]);
                            sz += strlen(buf);
                        } else {
                            const char * valu = get_field_from_tag(here,in_xtree);
                            if (NULL == valu) {
                                erri = ORCM_ERR_BAD_PARAM;
                                break;
                            }
                            snprintf(buf, BUFSZ, "%s=%s\t",tx,valu);
                            sz += strlen(buf);
                        }
                    }
                }

                if (0 == sz) {
                    /*Seems impossible to get here.  But Clockworks complains...*/
                    continue;
                }

                if (sz > sz_txt) {
                    if (NULL != txt) {
                        free(txt);
                        txt=NULL;
                    }
                    sz_txt = 2*sz; /* Times 2 to avoid superfluous callocs */
                    txt = (char*) calloc(sz_txt, sizeof(char) );
                    if (NULL == txt){
                        /* This should not happen.  Bail out . */
                        break;
                    }
                }

                /* Then assemble the line and print */
                t = txt;
                snprintf(t, BUFSZ, "XML-STRUCT: %lu> ", i);
                t += strlen(t);

                for (j=0; j < in_xtree->hier_row_lengths[i]; ++j){
                    if ( j+1 >= in_xtree->hier_row_lengths[i]) {
                        c = ' ';
                    } else {
                        c = '\t';
                    }

                    long here = in_xtree->hierarchy[i][j];

                    if (is_child(here)) {
                        snprintf(t, BUFSZ, "%ld%c", child_to_offset(here), c );
                    } else {
                        const char * tx = in_xtree->items[ here ];
                        if (is_sectional_item(tx)) {
                            snprintf(t, BUFSZ, "%s%c", tx, c );
                        } else {
                            const char * valu = get_field_from_tag(here,in_xtree);
                            if (NULL == valu) {
                                erri = ORCM_ERR_BAD_PARAM;
                                break;
                            }
                            snprintf(t, BUFSZ, "%s=%s%c", tx, valu, c );
                        }
                    }
                    t += strlen(t);
                }

                opal_output(0, "%s", txt);
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }


    if (NULL != txt) {
        free(txt);
    }
}

static void set_parent_isa_node(bool * io_parent_isa_node, const xml_tree_t * in_xtree)
{
    unsigned long i;
    for (i=0;i <in_xtree->sz_hierarchy; ++i) {
        io_parent_isa_node[i]=false;
    }
    for (i=0;i <in_xtree->sz_hierarchy; ++i) {
        long dex = in_xtree->hierarchy[i][0];
        const char * tag = in_xtree->items[dex];
        if (0 != strcasecmp(tag,TXjunction)) {
            continue;
        }
        const char * jtype = get_field( TXtype, i, in_xtree);
        if (NULL == jtype || 0 != strcasecmp(jtype,FDnode)) {
            continue;
        }

        long child = get_first_child(TXcontrol, i, in_xtree);
        if (xtNOTA_CHILD == child) {
            continue;
        }
        io_parent_isa_node[child] = true;
    }
}

static int parse_config(xml_tree_t * io_xtree, opal_list_t *io_config)
{
    /*
      io_config is a list of items of type orcm_cfgi_xml_parser_t
    */
    const int NEED_RECORD=1;

    char * t=NULL;
    unsigned long i=0, j=0;

    unsigned long record_begin=0;
    unsigned long record_end=0;

    long * hstack=NULL;  /*Stores row offset into io_xtree->hierarchy*/
    orcm_cfgi_xml_parser_t **parent_stack=NULL; /*Array of pointer to xml struct*/
    unsigned long sz_stack=0;
    long u=0, uu=0;

    bool * parent_isa_node = NULL; /* The array will tell a controller
                                   * if its parent is a node.
                                   * false -> not a node
                                   * true -> parent is a node
                                   */

    orcm_cfgi_xml_parser_t *parent=NULL;
    orcm_cfgi_xml_parser_t *xml=NULL;

    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        /*===== Find the RECORD configuration */
        record_begin = 0;
        erri = find_beginend_configuration(record_begin, io_xtree->items,
                                           io_xtree->sz_items, NEED_RECORD,
                                           &record_begin, &record_end);
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
        erri = structure_lexed_items(record_begin, record_end, io_xtree);
        if (ORCM_SUCCESS != erri) {
            break;
        }
        if (0 == io_xtree->sz_hierarchy) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        erri = check_duplicate_singletons( io_xtree );
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = check_junctions_have_names( io_xtree );
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = check_parent_has_uniquely_named_children( io_xtree );
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = check_host_field_has_matching_port( io_xtree );
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = check_unique_scheduler( io_xtree );
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = check_unique_host_port_pair( io_xtree );
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = check_unique_cluster_on_top( io_xtree );
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = check_no_sublevel_on_controller( io_xtree );
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = check_no_sublevel_on_scheduler( io_xtree );
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = check_controller_on_node_junction( io_xtree );
        if (ORCM_SUCCESS != erri) {
            break;
        }

        erri = augment_hierarchy( io_xtree );
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (V_HIGHER < opal_output_get_verbosity(orcm_cfgi_base_framework.framework_output)) {
            opal_output(0, "FINAL XML OUTPUT");
            xml_tree_print_lexer(io_xtree);
            xml_tree_print_structure(io_xtree);
        }

        erri = check_hierarchy_integrity( io_xtree );
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*===== Allocate the working stacks */

        /*Here multiply by 2 in order to be generous. */
        sz_stack = 2 * io_xtree->sz_hierarchy;
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

        parent_isa_node = calloc(io_xtree->sz_hierarchy,sizeof(int));
        if (NULL == parent_isa_node) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        /*===== Start the stacks */
        t = io_xtree->items[ io_xtree->hierarchy[0][0] ];
        if (0 != strcasecmp(t,TXconfig)) {
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

        for (i=0; i < io_xtree->hier_row_lengths[0]; ++i) {
            if ( is_child(io_xtree->hierarchy[0][i]) ) {
                parent_stack[sz_stack] = xml;
                hstack[sz_stack] = child_to_offset(io_xtree->hierarchy[0][i]);
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

        set_parent_isa_node(parent_isa_node, io_xtree);

        /*===== Stuff the XML into the config */
        while (0 < sz_stack){
            --sz_stack;
            i = hstack[sz_stack];
            parent = parent_stack[sz_stack];

            u = io_xtree->hierarchy[i][0];
            if (is_child(u)) {
                erri = ORCM_ERR_BAD_PARAM; /*That should never happen.*/
                break;
            }

            t = io_xtree->items[u];
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "XML-XFER: on item %lu = %s", i, t);
            if (0 == strcasecmp(t,TXcontrol)) {
                if (parent_isa_node[i]) {
                    continue; /*This controller will have been handle
                               *by the node junction.
                               */
                    opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                            "XML-XFER:\t\tcontroller on node");
                }
                erri = transfer_to_controller(i, io_xtree, parent);
                if (ORCM_SUCCESS != erri) {
                    break;
                }
            } else if (0 == strcasecmp(t,TXjunction)) {
                const char * jtype = get_field(TXtype, i, io_xtree);
                bool on_a_node = false;
                if (NULL != jtype && 0==strcasecmp(FDnode, jtype)) {
                    /*As node junctions are the leaves of the hierarchy,
                     *we must flatten any controller it must have in
                     *order to conform to the CFGI file2.0 back-end.
                     */
                     on_a_node = true;
                }

                if (on_a_node) {
                    erri = transfer_to_node(i, io_xtree, parent);
                    if (ORCM_SUCCESS != erri) {
                        break;
                    }
                } else {
                    xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
                    if (NULL == xml) {
                        erri = ORCM_ERR_OUT_OF_RESOURCE;
                        break;
                    }
                    opal_list_append(&parent->subvals, &xml->super);

                    for (j=1; j < io_xtree->hier_row_lengths[i]; ++j) {
                        uu = io_xtree->hierarchy[i][j];
                        if (is_child(uu)) {
                            /*Just stack child for processing later.*/
                            hstack[sz_stack]       = child_to_offset(uu);
                            parent_stack[sz_stack] = xml;
                            ++sz_stack;
                            continue;
                        } else {
                            char * tag = io_xtree->items[uu];
                            const char * field = get_field_from_tag(uu,io_xtree);
                            if (NULL == field) {
                                erri = ORCM_ERR_BAD_PARAM;
                                break;
                            }
                            if (0 == strcasecmp(tag,TXtype)) {
                                xml->name = strdup(field);
                                tolower_cstr(xml->name);
                            } else if (0 == strcasecmp(tag,TXname)) {
                                if (OPAL_SUCCESS != opal_argv_append_nosize(&xml->value, field)) {
                                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                                    break;
                                }
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
            } else if ('s' == t[0] || 'S' == t[0]) {
                if (0 == strcasecmp(t,TXscheduler)) {
                    erri = transfer_to_scheduler(i, io_xtree, parent);
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

    if (NULL != parent_isa_node) {
        free(parent_isa_node);
        parent_isa_node = NULL;
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
    int rc;

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

        node->name = strdup(x->value[0]);
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

        replace_ampersand(&x->value[0], rack->name);
        rack->controller.name = strdup(rack->name);
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
                    replace_ampersand(&names[m], rack->name);
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
                        node->rack = (struct orcm_rack_t *) rack;
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
        replace_ampersand(&x->value[0], row->name);
        row->controller.name = pack_charname(row->name[0], x->value[0]);
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
                    replace_ampersand(&names[m], row->name);
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

            replace_ampersand(&x->value[0], cluster->name);

            if (0 == only_one_cluster_controller) {
                cluster->controller.name = strdup(x->value[0]);
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
                    /* see if we have each row object - it not, create it */
                    int m;
                    for (m=0; NULL != names[m]; m++) {
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
                scheduler->controller.name = strdup(x->value[0]);
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
            opal_output_verbose(2, orcm_cfgi_base_framework.framework_output,
                                "PUSHING %s TO ENVIRON", t);
            putenv(t);
        }
    }
    opal_argv_free(tmp);
}


static int is_whitespace(char c){
    if (' ' == c || '\t' == c || '\n' == c || '\r' == c) {
        return 1;
    }
    return 0;
}

static int is_forbidden(char c){
    int x=c;
    if (31 >= x || 127 <= x) {
        if ('\t' == c) {
            /*The auto tab is ok. */
            return 0;
        }
        return 1; /*Return 1 for forbidden char */
    }
    return 0;
}

    /* This function takes the XML text found in in_filename
     * lex the file thereby creating an array of lexicographic items.
     * These items are c-strings and are stored in
     *      io_xtree->sz_items and
     *      io_xtree.items
     * Both of which are owned by io_xtree
     * NOTE:
     *  =All data field are preprended by '\t'.
     *  =All closing marker are prepended with '/'.
     */
static int lex_xml( char * in_filename, xml_tree_t * io_xtree)
{
    FILE * fin =NULL;
    unsigned long fsize; /*The number of bytes in the in_filename file*/
    const unsigned long min_file_size = 32; /*TODO: update this number*/
    char * tx = NULL; /*Contains all the text before transfer. Owned. */

    char * p=NULL, *q=NULL; /*Utility pointers */

    const int some_added_extra=32;
    unsigned long i=0,j=0,k=0;
    int ic=0;
    char c=0;
    unsigned long line_count=0;
    unsigned long text_length=0;

    int came_from_data=-1; /* -1 -> Undecided
                            *  0 -> False
                            *  1 -> True
                            */
    io_xtree->sz_items = 0;
    io_xtree->items = NULL;

    int erri = ORCM_SUCCESS;
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
                /*@1:Going From Window OS to Linux and back does funny things with
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

        fsize = i;        /* @1:Correcting fsize if needed. See comment above.*/
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
        for (i = 0; i < fsize; ++i) {
            c = tx[i];
            if (is_whitespace(c)) {
                if ('\n' == c ) {
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
                    if ('?' == *(p-1) || ('-' == *(p-1) && '-' == *(p-2))) {
                        i += (p-(tx+i)); /*Ratchet up to the ending '>' */
                        if (i >= fsize) {
                            erri = ORCM_ERR_BAD_PARAM;
                            break;
                        }
                    } else {
                        /*Keep looking*/
                        while (!('?' == *(p-1) || ('-' == *(p-1) && '-' == *(p-2)))) {
                            ++p;
                            p = strchr(p, '>');
                            if (NULL == p) {
                                /*Missing closing '>' */
                                erri = ORCM_ERR_BAD_PARAM;
                                break;
                            }
                        }
                        if (ORCM_SUCCESS != erri) {
                            break;
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
        io_xtree->sz_items = k;

        /*Set up the item array */
        erri = xml_tree_alloc_items(io_xtree);
        if (ORCM_SUCCESS != erri){
            break;
        }

        j=0;
        k=0;
        for (i=0; i<text_length; ++i) {
            if ('\n' == tx[i]) {
                tx[i ]= '\0';
                io_xtree->items[j] = strdup( &tx[k] );
                ++j;
                k=i+1;
            }
        }
        if (j != io_xtree->sz_items) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        break;
    }

    if (NULL != tx) {
        free (tx);
        tx = NULL;
    }

    if (NULL != fin){
        fclose(fin);
        fin=NULL;
    }
    return erri;
}

static bool is_regex(const char * in_cstring)
{
    /*This is a very simple test.  Simple means that it does not check if
     *the string is a valid regex.
     */
    if (NULL == in_cstring) {
        return false;
    }

    unsigned long sz = strlen(in_cstring);
    if (2 >= sz) {
        return false;
    }

    char * beginb = strchr(in_cstring, '[');
    if (NULL == beginb) {
        return false;
    }

    char *endb = strchr(in_cstring, ']');
    if (NULL == endb) {
        return false;
    }

    if ( (beginb - in_cstring) < (endb - in_cstring) ) {
        return true;
    }

    return false;
}

/*Returns 1 if the tag given is one of the one used.
 *Returns 0 otherwise.
 */
static int is_known_tag(const char * in_tagtext)
{
    const char * t = in_tagtext;
    /*First a quick check than a fuller one*/
    if (NULL == t) {
        return 0;
    }
    switch (t[0]) {
    case 'a':
    case 'A':
        if (0 == strcasecmp(t,TXaggregat)) {
            return 1;
        }
        break;
    case 'c':
    case 'C':
        if (0 == strcasecmp(t,TXconfig) ||
            0 == strcasecmp(t,TXcontrol) ||
            0 == strcasecmp(t,TXcount) ||
            0 == strcasecmp(t,TXcores) ||
            0 == strcasecmp(t,TXcreation)) {
            return 1;
        }
        break;
    case 'e':
    case 'E':
        if (0 == strcasecmp(t,TXenvar)) {
            return 1;
        }
        break;
    case 'h':
    case 'H':
        if (0 == strcasecmp(t,TXhost)) {
            return 1;
        }
        break;
    case 'j':
    case 'J':
        if (0 == strcasecmp(t,TXjunction)) {
            return 1;
        }
        break;
    case 'l':
    case 'L':
        if (0 == strcasecmp(t,TXlog)) {
            return 1;
        }
        break;
    case 'm':
    case 'M':
        if (0 == strcasecmp(t,TXmca_params)) {
            return 1;
        }
        break;
    case 'n':
    case 'N':
        if (0 == strcasecmp(t,TXname)) {
            return 1;
        }
        break;
    case 'p':
    case 'P':
        if (0 == strcasecmp(t,TXport)) {
            return 1;
        }
        break;
    case 'q':
    case 'Q':
        if (0 == strcasecmp(t,TXqueues)) {
            return 1;
        }
        break;
    case 'r':
    case 'R':
        if (0 == strcasecmp(t,TXrole)) {
            return 1;
        }
        break;
    case 's':
    case 'S':
        if (0 == strcasecmp(t,TXscheduler) ||
            0 == strcasecmp(t,TXshost)) {
            return 1;
        }
        break;
    case 't':
    case 'T':
        if (0 == strcasecmp(t,TXtype)) {
            return 1;
        }
        break;
    case 'v':
    case 'V':
        if (0 == strcasecmp(t,TXversion)) {
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
static int is_sectional_item(const char * in_tagtext)
{
    const char * t = in_tagtext;
    if (NULL == t) {
        return 0;
    }
    /*First a quick check than a fuller one*/
    switch (t[0]) {
    case 'c':
    case 'C':
        if (0 == strcasecmp(t, TXcontrol) ||
            0 == strcasecmp(t,TXconfig)) {
            return 1;
        }
        break;
    case 'j':
    case 'J':
        if (0 == strcasecmp(t, TXjunction)) {
            return 1;
        }
        break;
    case 's':
    case 'S':
        if (0 == strcasecmp(t,TXscheduler)) {
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
static int is_singleton(const char * in_tagtext)
{
    const char * t = in_tagtext;
    /*First a quick check than a fuller one*/
    if (NULL == t) {
        return 0;
    }
    switch (t[0]) {
    case 'a':
    case 'A':
        if (0 == strcasecmp(t,TXaggregat)) {
            return 1;
        }
    case 'c':
    case 'C':
        if (0 == strcasecmp(t,TXcontrol) ||
            0 == strcasecmp(t,TXcount) ||
            0 == strcasecmp(t,TXcores) ||
            0 == strcasecmp(t,TXcreation)) {
            return 1;
        }
        break;
    case 'e':
    case 'E':
        if (0 == strcasecmp(t,TXenvar)) {
            return 1;
        }
        break;
    case 'h':
    case 'H':
        if (0 == strcasecmp(t,TXhost)) {
            return 1;
        }
    case 'j':
    case 'J':
        break;
    case 'l':
    case 'L':
        if (0 == strcasecmp(t,TXlog)) {
            return 1;
        }
        break;
    case 'm':
    case 'M':
        break;
    case 'n':
    case 'N':
        if (0 == strcasecmp(t,TXname)) {
            return 1;
        }
        break;
    case 'p':
    case 'P':
        if (0 == strcasecmp(t,TXport)) {
            return 1;
        }
        break;
    case 'q':
    case 'Q':
        break;
    case 'r':
    case 'R':
        if (0 == strcasecmp(t,TXrole)) {
            return 1;
        }
        break;
    case 's':
    case 'S':
        if (0 == strcasecmp(t,TXscheduler) ||
            0 == strcasecmp(t,TXshost)) {
            return 1;
        }
        break;
    case 't':
    case 'T':
        if ( ! strcasecmp(t,TXtype)) return 1;
        break;
    case 'v':
    case 'V':
        if (0 == strcasecmp(t,TXversion)) {
            return 1;
        }
        break;
    default:
        break;
    }
    return 0;
}

static int check_lex_aggregator_yes_no(xml_tree_t * in_xtree)
{
    unsigned long i=0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (0 == in_xtree->sz_items) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: No XML items to examine.");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /*Check if the aggregator field is really yes or no*/
        for (i=0; i < in_xtree->sz_items; ++i) {
            char * t;
            t = in_xtree->items[i];

            if ('\t' == t[0] || '/' == t[0]) {
                /*Skip ending tags and data fields*/
                continue;
            }

            if ('a' != t[0] && 'A' != t[0]) {
                /*Not the right tag*/
                continue;
            }
            if (0 != strcasecmp(t,TXaggregat)) {
                continue;
            }

            t = in_xtree->items[i+1];
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
            /*Do not use break in order to find all errors.
             *Uncomment the following in order to stop at the first error.
             *break;
             */
        }

        break;
    }
    return erri;
}
static int check_lex_allowed_junction_type(xml_tree_t * in_xtree)
{
    unsigned long i=0;

    unsigned long t_cluster=0;
    unsigned long t_rack=0;
    unsigned long t_row=0;
    unsigned long t_inter=0;
    unsigned long t_node=0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (0 == in_xtree->sz_items) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: No XML items to examine.");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /*Making sure that only the allowed junction type are present */
        /*Make sure that at least one node is present */
        t_cluster = 0;
        t_rack = 0;
        t_row = 0;
        t_inter = 0;
        t_node = 0;
        for (i=0; i < in_xtree->sz_items; ++i) {
            char * t;
            t = in_xtree->items[i];

            if ('\t' == t[0] || '/' == t[0]) {
                /*Skip ending tags and data fields*/
                continue;
            }

            if ('t' != t[0] && 'T' != t[0]) {
                /*Not the right tag*/
                continue;
            }
            if (0 != strcasecmp(t,TXtype)) {
                continue;
            }
            unsigned long k;
            k=i+1;
            /* k overflow and presence of data field should have
               been checked above. */
            t = in_xtree->items[k];
            ++t; /*Jump over the '\t' */
            if (0 == strcasecmp(t,FDcluster)) {
                ++t_cluster;
            } else if (0 == strcasecmp(t, FDrack)) {
                ++t_rack;
            } else if (0 == strcasecmp(t, FDrow)) {
                ++t_row;
            } else if (0 == strcasecmp(t, FDnode)) {
                ++t_node;
            } else if (0 == strcasecmp(t, FDinter)) {
                ++t_inter;
            }
        }

        if (0 != t_inter) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                "ERROR: Junction NOT of type (cluster|row|rack|node) have yet to be implemented.");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if (0 == t_node) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: At least one junction of type \"node\" must be defined.");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        break;
    }
    return erri;
}
static int check_lex_single_record(xml_tree_t * in_xtree)
{
    unsigned long i=0;

    unsigned long record_count=0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (0 == in_xtree->sz_items) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: No XML items to examine.");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /* Make sure there is only a single RECORD configuration */
        record_count=0;
        for (i=0; i < in_xtree->sz_items; ++i) {
            char * t;
            t = in_xtree->items[i];
            /* Looking for the TXrole tag */
            if ('r' != t[0] && 'R' != t[0]) {
                continue;
            }
            if (0 != strcasecmp(t,TXrole)) {
                continue;
            }
            unsigned long k;
            k = i+1;
            /* k overflow and presence of data field should have
               been checked above. */
            t = in_xtree->items[k];
            ++t; /*Jump over the '\t' */
            if (0 == strcasecmp(t,FDrecord)){
                ++record_count;
            } else if (0 == strcasecmp(t,FDmod)) {
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
                                "ERROR: Incorrect count of XML tag configuration of \"RECORD\" role:%lu",
                                record_count);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        break;
    }
    return erri;
}
static int check_lex_open_close_tags_matching(xml_tree_t * in_xtree)
{
    unsigned long i=0;

    unsigned long openings=0;
    unsigned long closings=0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (0 == in_xtree->sz_items) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: No XML items to examine.");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /*Make sure the number of opening tag matches the number of closing tags*/
        /*For non-sectional tags, the closing tag should be close to the
         *opening tag, with a data field in between them.
         */
        for (i=0; i < in_xtree->sz_items; ++i) {
            char * t = in_xtree->items[i];
            if ('/' == t[0]) {
                ++closings;
            } else {
                if ('\t' != t[0]) {
                    ++openings;
                }
            }

            if (is_sectional_item(t)) {
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
            unsigned long k;
            k=i+1;
            if (k >= in_xtree->sz_items || '\t' != in_xtree->items[k][0]) {
                opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                    "ERROR: Missing data field for XML tag ->%s", t);
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }

            k=i+2;
            if (k >= in_xtree->sz_items || '/' != in_xtree->items[k][0]) {
                opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                    "ERROR: Missing closing tag for XML tag ->%s", t);
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
        }
        if (ORCM_SUCCESS != erri){
            break;
        }

        if (openings != closings) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: The number of opening and closing XML tags do not match: open=%lu close=%lu",
                                openings, closings);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }
        break;
    }
    return erri;
}
static int check_lex_tags_valid(xml_tree_t * in_xtree)
{
    unsigned long i=0;
    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (0 == in_xtree->sz_items) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: No XML items to examine.");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        for (i=0; i < in_xtree->sz_items; ++i) {
            char * t = in_xtree->items[i];
            if (NULL == t) {
                opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                    "ERROR: NULL tag found.");
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
            if ('\t' == t[0]) {
                continue;
            }
            unsigned long k = 0;
            if ('/' == t[0]) {
                k=1;
            } else {
                k=0;
            }
            if (!is_known_tag(t+k)) {
                opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                    "ERROR: Unknown XML tag->%s", t);
                erri = ORCM_ERR_BAD_PARAM;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }
    return erri;
}

static int check_lex_all_port_fields(xml_tree_t * in_xtree)
{
    /*Check that port field content is a positive bound integer */
    unsigned long i=0;
    char *t=NULL;
    char * endptr=NULL;
    const int base = 10;
    long number=0;

    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri){
        for (i=0; i < in_xtree->sz_items; ++i){
            t = in_xtree->items[i];

            if ('\t' == t[0] || '/' == t[0]) {
                /*Skip ending tags and data fields*/
                continue;
            }
            if ('p' != t[0] && 'P' != t[0]) {
                /*Not the right tag*/
                continue;
            }
            if (0 != strcasecmp(t,TXport)) {
                continue;
            }

            t = in_xtree->items[i+1];
            t += 1; /*Jump over the tab*/

            endptr=NULL;
            number = strtol(t, &endptr, base);

            if ('\0' != *endptr) {
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                     "ERROR: The value of a port is not a valid integer");
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                     "Error on lexer element %lu", i+1);
                erri = ORCM_ERR_BAD_PARAM;
                /*Do not bail out right away.  Survey all nodes.
                  That gives a chance to see all mistakes. */
                /*break;*/
            } else {
                if (0 > number || USHRT_MAX < number) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                         "ERROR: The value of a port is not in an acceptable range (0<=n<=SHRT_MAX): %ld",
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
                                 xml_tree_t * io_xtree)
{
    /* in_begin_offset : The start of the offset in io_xtree->items
     *                   where to begin the search.
     *                   ==>IMPORTANT: It has to be on a sectional item
     * in_end_offset :  Where to stop the search.
     *                   ==>IMPORTANT: It has include the ending tag of the
     *                                 sectional item refer to by in_begin_offset.
     * io_xtree : the xml syntax tree to be constructed
     *            It is assumed that io_xtree->items and io_xtree->sz_items have
     *            already been constructed by the lex_xml function.
     *
     * A row in io_xtree->hierarchy contains a sequence of signed integers:
     *          A positive integer is an offset into io_xtree->items
     *          A negative integer is an offset into io_xtree->hierarchy
     *      The first, at offset=0, positive integer points to where that
     *      sectional item starts in io_xtree->items.
     *      All other positive integers points to non-sectional entries
     *      in io_xtree->items, for that sectional item.
     *          Only beginning tags are pointed to; ending tags and data fields
     *          are not included.  Those are assumed to be there.
     *
     *      All negative integers points to a sectional entry.
     *      So negative integers are the child sections of the current row.
     *      The positive integers are the properties local to the current row.
     *
     */

    /*The algorithm is essentially a hierarchical search without using recursion
     * or a stack.
     * A little slower maybe but it is real easy to fix if wrong.
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
    io_xtree->sz_hierarchy = 0;
    io_xtree->hier_row_lengths = NULL;
    io_xtree->hierarchy = NULL;

    while (ORCM_SUCCESS == erri){
        if (0 == io_xtree->sz_items || NULL == io_xtree->items) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }
        if (in_begin_offset >= in_end_offset ||
            in_begin_offset >= io_xtree->sz_items ||
            in_end_offset > io_xtree->sz_items) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if (!is_sectional_item(io_xtree->items[in_begin_offset])) {
            /*We are not starting on a sectional element*/
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /*First count the number of sections we have */
        k=0;
        ending_sectional_tags=0;
        for (i=in_begin_offset; i < in_end_offset; ++i) {
            t = io_xtree->items[i];
            if ('\t' == t[0]) {
                continue;
            }
            if ('/' == t[0]) {
                if (is_sectional_item(t+1)) {
                    ++ending_sectional_tags;
                }
            }
            if (!is_sectional_item(t)) {
                continue;
            }
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "Section =%s",t);
            if ('/' == t[0]) {
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

        io_xtree->sz_hierarchy=k;
        k=0;
        erri = xml_tree_alloc_hierarchy(io_xtree);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        sz_section=0;
        section_starts = (unsigned long *) calloc(io_xtree->sz_hierarchy, sizeof(unsigned long));
        if (NULL == section_starts) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        section_ends = (unsigned long *) calloc(io_xtree->sz_hierarchy, sizeof(unsigned long));
        if (NULL == section_ends) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        section_parents = (unsigned long *) calloc(io_xtree->sz_hierarchy, sizeof(unsigned long));
        if (NULL == section_parents) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        section_counts = (unsigned long *) calloc(io_xtree->sz_hierarchy, sizeof(unsigned long));
        if (NULL == section_counts) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        /*Find the beginning and the end of each section, and their parent section */

        /*NOTE: Section_parents does not store indices in io_xtree->items.
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
            t = io_xtree->items[i];
            if ('\t' == t[0]) { /*Skip data field*/
                continue;
            }
            if ('/' == t[0]) { /*An ending*/
                if (!is_sectional_item(t+1)) {
                    continue;
                }
            } else if (!is_sectional_item(t)) {
                /*Not a sectional tag, not an ending tag and not a data field*/
                if ((unsigned long)(-1) == sz_section) {
                    /*I did not expect that here. */
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                long x;
                for (x=sz_section; x >= 0; --x) {
                    if (0 == section_ends[x]) {
                        /*Found the first empty ending*/
                        break;
                    }
                }
                if (0==x && 0 != section_ends[0]) {
                    /* No empty ending found ???*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                ++section_counts[x];
                continue;
            }

            if ('/' == t[0]) { /*An ending tag of a section */
                long x;
                for (x=sz_section; x >= 0; --x) {
                    if (0 == section_ends[x]) {
                        /*Found the first empty ending*/
                        break;
                    }
                }
                if (0 == x && 0 != section_ends[0]) {
                    /* No empty ending found ???*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                section_ends[x]=i;
            } else { /*A beginning tag of a section */
                ++sz_section;
                section_starts[sz_section]=i;
                section_ends[sz_section]=0;
                if (0 != sz_section) {
                    /*Find first empty ending */
                    long x;
                    for (x=sz_section-1;  x>= 0; --x) {
                        if (0 == section_ends[x]) {
                            /*Found the first available parent*/
                            section_parents[sz_section] = x;
                            break;
                        }
                    }
                    if (0 == x && 0 != section_ends[0]) {
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

        if (sz_section+1 != io_xtree->sz_hierarchy) {
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR:: sz_section=%lu    sz_hierarchy=%lu\n",
                                sz_section, io_xtree->sz_hierarchy);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        /*Add to the count the number of childrens*/
        erri = xml_tree_alloc_hier_row_lengths(io_xtree);
        if (ORCM_SUCCESS != erri) {
            break;
        }
        memcpy(io_xtree->hier_row_lengths,section_counts,(io_xtree->sz_hierarchy)*sizeof(unsigned long));

        /*Add the sub-sectional items */
        for (k=0; k < io_xtree->sz_hierarchy; ++k) {
            if (nota_index == section_parents[k]) {
                continue;
            }
            i = section_parents[k];
            ++io_xtree->hier_row_lengths[i];
        }
        /*Add the space for section_starts */
        for (k=0; k < io_xtree->sz_hierarchy; ++k) {
            ++io_xtree->hier_row_lengths[k];
        }

        /*Allocate the memory for the entries in io_xtree->hierarchy*/
        for (k=0; k < io_xtree->sz_hierarchy; ++k) {
            i = io_xtree->hier_row_lengths[k];
            io_xtree->hierarchy[k] = (long *) calloc(i, sizeof(long));
            if (NULL == io_xtree->hierarchy[k]) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }
        }
        if (ORCM_SUCCESS != erri){
            break;
        }

        /*Fill each row of o_hierarchy up */
        memset(section_ends, 0, (io_xtree->sz_hierarchy)*sizeof(unsigned long));
        memset(section_counts, 0, (io_xtree->sz_hierarchy)*sizeof(unsigned long));

        /*Add the section_starts to the hierarchy */
        for (k=0; k < io_xtree->sz_hierarchy; ++k) {
            io_xtree->hierarchy[k][0] = section_starts[k];
            ++section_counts[k];
        }

        /*Do the same as above but only to get the non-sectional items*/
        /*No need to recalculate the section_starts.  Keep them for later*/
        /*No need to recalculate the section_parents.  Keep them for later*/
        /*section_counts & sections_ends are re-built*/

        sz_section=-1;
        for (i=in_begin_offset; i < in_end_offset; ++i) {
            t = io_xtree->items[i];
            if ('\t' == t[0]) { /*Skip data field*/
                continue;
            }
            if ('/' == t[0]) { /*An ending*/
                if (!is_sectional_item(t+1)) {
                    continue;
                }
            }else if (!is_sectional_item(t)) {
                /*Not a sectional tag, not an ending tag and not a data field*/
                if ((unsigned long)(-1) == sz_section) {
                    /*I did not expect that here. */
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                long x;
                for (x=sz_section; x >= 0; --x) {
                    if (0 == section_ends[x]) {
                        /*Found the first empty ending*/
                        break;
                    }
                }
                if (0 == x && 0 != section_ends[0]) {
                    /* No empty ending found ???*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                io_xtree->hierarchy[x][section_counts[x]] = i;
                ++section_counts[x];
                continue;
            }

            if ('/' == t[0]) { /*An ending tag of a section */
                long x;
                for (x=sz_section; x >= 0; --x) {
                    if (0 == section_ends[x]) {
                        /*Found the first empty ending*/
                        break;
                    }
                }
                if (0 == x && 0!=section_ends[0]) {
                    /* No empty ending found ???*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                section_ends[x]=i;
            } else { /*A beginning tag of a section */
                ++sz_section;
                section_ends[sz_section]=0;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*Add the missing children */
        /*This will consume section_counts */
        for (k=0; k < io_xtree->sz_hierarchy; ++k) {
            if (nota_index == section_parents[k]){
                continue;
            }
            i = section_parents[k];
            io_xtree->hierarchy[i][section_counts[i]] = (-1L)*k;
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

static int transfer_to_node(unsigned long in_node_row,
                            const xml_tree_t * in_xtree,
                            orcm_cfgi_xml_parser_t * io_parent)
{
    orcm_cfgi_xml_parser_t * xml = NULL;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
        if (NULL == xml) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        opal_list_append(&io_parent->subvals, &xml->super);

        xml->name = strdup(FDnode);

        /* Adjust the parent, it will be re-used*/
        io_parent = xml;
        xml=NULL;

        /*First deal with items local to the node, then after its controller */
        unsigned long j=0;
        for (j=0; j!=in_xtree->hier_row_lengths[in_node_row]; ++j) {
            long uu = in_xtree->hierarchy[in_node_row][j];
            if (is_child(uu)) {
                continue; /*to be dealt with later */
            }

            char * tag = in_xtree->items[uu];
            const char * field = get_field_from_tag(uu, in_xtree);

            if ( 0 == strcasecmp(tag,TXtype) ||
                 0 == strcasecmp(tag,TXjunction)
               ) {
                /*These are not needed in CFGI version 2.0 */
                continue;
            }
            if (NULL == field) {
                erri = ORCM_ERR_BAD_PARAM;
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                     "ERROR: XML syntax corruption found.");
                break;
            }

            if (0 == strcasecmp(tag, TXname)) {
                if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&io_parent->value, field))) {
                    break;
                }
                continue;
            }

            xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
            if (NULL == xml) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }

            opal_list_append(&io_parent->subvals, &xml->super);

            xml->name = strdup(tag);
            tolower_cstr(xml->name);

            if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&xml->value, field))) {
                break;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*The following simply drops the node controller's host field.
         * It is assumed that it is compatible with its node's name, or that
         * it was checked for error before getting here.
         */
        long ctrl_row = get_first_child(TXcontrol, in_node_row, in_xtree);
        if (xtNOTA_CHILD != ctrl_row) {
            erri = transfer_other_items(ctrl_row, in_xtree, io_parent);
            if (ORCM_SUCCESS != erri) {
                break;
            }

            erri = transfer_mca_params(ctrl_row, in_xtree, io_parent);
            if (ORCM_SUCCESS != erri) {
                break;
            }
        }

        break;
    }

    return erri;
}

static int transfer_mca_params(unsigned long in_row_in_hier,
                               const xml_tree_t * in_xtree,
                               orcm_cfgi_xml_parser_t * io_parent)
{
    char * txt = NULL;
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        unsigned long text_length = 0;
        unsigned long j = 0;
        for (j=1; j < in_xtree->hier_row_lengths[in_row_in_hier]; ++j) {
            long uu = in_xtree->hierarchy[in_row_in_hier][j];
            if (is_child(uu)){
                continue;
            }

            const char * tt = in_xtree->items[uu];
            const char * field = get_field_from_tag(uu, in_xtree);
            if (NULL == field) {
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }

            if (0 == strcasecmp(tt,TXmca_params)) {
                /*Add 1 for either the comma or the ending '\0'*/
                text_length += strlen(field) +1;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (0 < text_length){
            txt = (char*) calloc(text_length, sizeof(char));
            if (NULL == txt) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }

            char comma = ',';
            for (j=1; j < in_xtree->hier_row_lengths[in_row_in_hier]; ++j) {
                if (j+1 == in_xtree->hier_row_lengths[in_row_in_hier]) {
                    comma = '\0';
                }
                long uu = in_xtree->hierarchy[in_row_in_hier][j];
                if (is_child(uu)){
                    continue;
                }
                const char * tt=in_xtree->items[uu];
                const char * field = get_field_from_tag(uu, in_xtree);
                if (NULL == field) {
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                if (0 == strcasecmp(tt,TXmca_params)) {
                    text_length = strlen(txt);
                    sprintf(txt+text_length, "%s%c", field, comma);
                }
            }
            if (ORCM_SUCCESS != erri) {
                break;
            }

            orcm_cfgi_xml_parser_t * xml=NULL;
            xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
            if (NULL == xml) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }
            opal_list_append(&io_parent->subvals, &xml->super);

            xml->name = strdup(TXmca_params);
            if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&xml->value, txt))) {
                free(txt);
                txt = NULL;
                break;
            }
            if (NULL != txt) {
                free(txt);
                txt = NULL;
            }
        }
        break;
    }

    if (NULL != txt) {
        free(txt);
        txt = NULL;
    }

    return erri;
}

static int transfer_other_items(unsigned long in_row_in_hier,
                                const xml_tree_t * in_xtree,
                                orcm_cfgi_xml_parser_t * io_parent)
{
    /* This function does not process:
     *      - host tag
     *      - shost tag
     *      - mca-parameters
     *      - queues
     */
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        unsigned long j=0;
        /*Then the other fields, but not the mca-params */
        /*Start at 1 since 0 is the controller tag*/
        for (j=1; j < in_xtree->hier_row_lengths[in_row_in_hier]; ++j) {
            long uu = in_xtree->hierarchy[in_row_in_hier][j];
            if (is_child(uu)) {
                continue;
            }
            const char * tag = in_xtree->items[uu];
            const char * field = get_field_from_tag(uu, in_xtree);

            if (0 == strcasecmp(tag,TXhost) ||
                0 == strcasecmp(tag,TXshost) ||
                0 == strcasecmp(tag,TXqueues) ||
                0 == strcasecmp(tag,TXmca_params)
               ) {
                /*Suppose to have been, or will be, handled elsewhere.*/
                continue;
            }
            if (NULL == field) {
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }

            orcm_cfgi_xml_parser_t * xml=NULL;
            xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
            if (NULL == xml) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }
            opal_list_append(&io_parent->subvals, &xml->super);

            xml->name = strdup(tag);
            tolower_cstr(xml->name);

            if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&xml->value, field))) {
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

static int transfer_to_controller(unsigned long in_ctrl_row,
                                  const xml_tree_t * in_xtree,
                                  orcm_cfgi_xml_parser_t * io_parent)
{
    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri){
        orcm_cfgi_xml_parser_t * xml = NULL;
        xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
        if (NULL == xml) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        opal_list_append(&io_parent->subvals, &xml->super);

        xml->name = strdup(TXcontrol);

        /* Adjust the parent*/
        io_parent = xml;
        xml=NULL;

        /*First resolve the host */
        const char * hostf = get_field(TXhost, in_ctrl_row, in_xtree);
        if (NULL == hostf) {
            erri = ORCM_ERR_BAD_PARAM;
            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                "ERROR: Controller with missing host field");
            break;
        }
        if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&io_parent->value, hostf))) {
            break;
        }

        /*Transfer all other items but not host and mca-params*/
        erri = transfer_other_items(in_ctrl_row, in_xtree, io_parent);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*Then handle the <mca_params> */
        erri = transfer_mca_params(in_ctrl_row, in_xtree, io_parent);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }
    return erri;
}

static int transfer_queues_to_scheduler(unsigned long in_sched_row,
                                        const xml_tree_t * in_xtree,
                                        orcm_cfgi_xml_parser_t * io_parent)
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        unsigned long queues_count = 0;
        unsigned long j=0;
        for (j=1; j < in_xtree->hier_row_lengths[in_sched_row]; ++j) {
            long uu = in_xtree->hierarchy[in_sched_row][j];
            if (is_child(uu)) {
                continue;
            }
            char * tt = in_xtree->items[uu];
            if (0 == strcasecmp(tt,TXqueues) ){
                ++queues_count;
            }
        }

        if (0 < queues_count) {
            orcm_cfgi_xml_parser_t * xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
            if (NULL == xml) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }
            opal_list_append(&io_parent->subvals, &xml->super);

            xml->name = strdup(TXqueues);

            for (j=1; j < in_xtree->hier_row_lengths[in_sched_row]; ++j) {
                long uu = in_xtree->hierarchy[in_sched_row][j];
                if (is_child(uu)) {
                    continue;
                }
                char * tt = in_xtree->items[uu];
                const char * field = get_field_from_tag(uu, in_xtree);
                if (NULL == field) {
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                if (0 == strcasecmp(tt,TXqueues)) {
                    if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&xml->value, field) )) {
                        break;
                    }
                }
            }
            if (ORCM_SUCCESS != erri) {
                break;
            }
        }
        break;
    }
    return erri;
}

static int transfer_to_scheduler(unsigned long in_sched_row,
                                 const xml_tree_t * in_xtree,
                                 orcm_cfgi_xml_parser_t * io_parent)
{
    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri){
        orcm_cfgi_xml_parser_t * xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
        if (NULL == xml) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        opal_list_append(&io_parent->subvals, &xml->super);

        xml->name = strdup(TXscheduler);

        /* Adjust the parent */
        io_parent = xml;
        xml=NULL;

        /*Handle the shost */
        unsigned long j=0;
        for (j=1; j < in_xtree->hier_row_lengths[in_sched_row]; ++j) {
            long uu=in_xtree->hierarchy[in_sched_row][j];
            if (is_child(uu)) {
                continue;
            }
            const char * tt = in_xtree->items[uu];
            const char * field = get_field_from_tag(uu, in_xtree);

            if (0 == strcasecmp(tt,TXshost)) {
                xml = NULL;
                xml = OBJ_NEW(orcm_cfgi_xml_parser_t);
                if (NULL == xml) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                opal_list_append(&io_parent->subvals, &xml->super);

                xml->name = strdup(FDnode);

                if (OPAL_SUCCESS != (erri = opal_argv_append_nosize(&xml->value, field))) {
                    break;
                }
            }
        }

        erri = transfer_other_items(in_sched_row, in_xtree, io_parent);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*Then handle the <queues> */
        erri = transfer_queues_to_scheduler(in_sched_row, in_xtree, io_parent);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*Then handle the <mca_params> */
        erri = transfer_mca_params(in_sched_row, in_xtree, io_parent);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }
    return erri;
}

static int remove_empty_items(xml_tree_t * io_xtree)
{
    unsigned long i=0,k=0;
    char * t=NULL;
    char * tt=NULL;

    int erri=ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (0 == io_xtree->sz_items || NULL == io_xtree->items) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }
        if (0 == io_xtree->sz_items) {
            break;
        }

        for (i=0; i < io_xtree->sz_items; ++i) {
            t = io_xtree->items[i];
            if (NULL == t){
                continue;
            }
            if ('\t' == t[0] || '/' == t[0]) {
                continue;
            }

            if (i+1 == io_xtree->sz_items) {
                continue;
            }

            tt = io_xtree->items[i+1];
            if ('\t' == tt[0]) {
                continue;
            }

            ++tt; /*Jump over the foward slash*/
            if (0 == strcasecmp(t,tt)) {
                /*Empty field found*/
                free ( io_xtree->items[i] );
                io_xtree->items[i] = NULL;
                free ( io_xtree->items[i+1] );
                io_xtree->items[i+1] = NULL;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*Remove nulled entries */
        k=-1;
        for (i=0; i < io_xtree->sz_items; ++i) {
            t=io_xtree->items[i];
            if (NULL != t) {
                ++k;
                io_xtree->items[k] = io_xtree->items[i];
            }
        }
        /*free superflous entries of items*/
        for (i=k+1; i < io_xtree->sz_items; ++i) {
            if (NULL != io_xtree->items[i]) {
                free (io_xtree->items[i]);
                io_xtree->items[i] = NULL;
            }
        }
        io_xtree->sz_items = k+1;

        break;
    }
    return erri;
}

static int check_duplicate_singletons(xml_tree_t * in_xtree)
{
    unsigned long i=0, j=0, k=0;
    long u=0, v=0;
    char * t=NULL, *tt=NULL, *context=NULL;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        for (i=0; i < in_xtree->sz_hierarchy; ++i) {
            context = in_xtree->items[ in_xtree->hierarchy[i][0] ];
            for (j=0; j < in_xtree->hier_row_lengths[i]; ++j) {
                u = in_xtree->hierarchy[i][j];
                if (is_child(u)) {
                    /*=====  Sectional items */
                    t = in_xtree->items[in_xtree->hierarchy[-u][0]];
                    if (!is_singleton(t)) {
                        continue;
                    }
                    for (k=j+1; k < in_xtree->hier_row_lengths[i]; ++k) {
                        v = in_xtree->hierarchy[i][k];
                        if (is_field(v)) {
                            erri = ORCM_ERR_BAD_PARAM;
                            opal_output_verbose(V_LO, orcm_cfgi_base_framework.framework_output,
                                                "ERROR: XML parser inconsistency found: %d", __LINE__);
                            break;
                        }
                        tt = in_xtree->items[ in_xtree->hierarchy[child_to_offset(v)][0] ];
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
                    t = in_xtree->items[u];
                    if (!is_singleton(t)) {
                        continue;
                    }
                    for (k=j+1; k < in_xtree->hier_row_lengths[i]; ++k) {
                        v = in_xtree->hierarchy[i][k];
                        if (is_child(v)) {
                            /*No need to check sectional items */
                            break;
                        }
                        tt = in_xtree->items[v];
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

static bool is_child(long in_hierarchy_index)
{
    return (0 > in_hierarchy_index);
}
static bool is_field(long in_hierarchy_index)
{
    return ( ! is_child(in_hierarchy_index));
}
static long child_to_offset(long in_hierarchy_index)
{
    return (-in_hierarchy_index);
}

static const char * get_field_from_tag(long in_tag_offset_in_items,
                                       const xml_tree_t * in_tree)
{
    const char * field = NULL;
    if ((unsigned long)in_tag_offset_in_items >= in_tree->sz_items) {
        return field;
    }

    const char * tag = in_tree->items[in_tag_offset_in_items];

    if (NULL == tag || '/' == tag[0] || '\t' == tag[0]) {
        /*Invalid tag*/
        return field;
    }

    const char * t = in_tree->items[in_tag_offset_in_items +1];
    if ('\t' != t[0]) {
        return field;
    }

    field = t+1;

    return field;
}

static const char * get_field( const char * in_xml_tag, unsigned long in_which_row,
                               const xml_tree_t * in_tree )
{
    char * found_field = NULL;

    unsigned long i = 0;
    long u = 0;
    char *t = NULL;

    if (in_which_row >= in_tree->sz_hierarchy){
        /*No way we'll find a field*/
        return found_field;
    }
    if (NULL ==  in_xml_tag || '\0' == in_xml_tag[0]) {
        /*No way we'll find a field*/
        return found_field;
    }

    for (i=0; i < in_tree->hier_row_lengths[in_which_row]; ++i) {
        u = in_tree->hierarchy[in_which_row][i];
        if (is_child(u)) {
            continue;
        }
        t = in_tree->items[u];

        if (0 == strcasecmp(t,in_xml_tag)) {
            t = in_tree->items[u+1];
            ++t; /*Jump over the tab marker*/
            found_field = t;
            break;
        }
    }

    return found_field;
}

static long get_first_child(const char * in_child_xml_tag,
                            unsigned long in_parent_row,
                            const xml_tree_t * in_tree )
{
    long found_child = xtNOTA_CHILD;

    if (in_parent_row >= in_tree->sz_hierarchy) {
        /*No way we'll find a child*/
        return found_child;
    }
    if (NULL ==  in_child_xml_tag || '\0' == in_child_xml_tag[0]) {
        /*No way we'll find a child*/
        return found_child;
    }

    unsigned long i=0;
    for (i=0; i<in_tree->hier_row_lengths[in_parent_row]; ++i) {
        long dex = in_tree->hierarchy[in_parent_row][i];
        if (is_field(dex)) {
            continue;  /*Skip fields*/
        }
        long child = in_tree->hierarchy[ child_to_offset(dex) ][0];
        const char * childtxt = in_tree->items[child];
        if (0 == strcasecmp(in_child_xml_tag, childtxt)) {
            found_child = child_to_offset(dex);
            break;
        }
    }
    return found_child;
}

static int check_junctions_have_names(xml_tree_t * in_tree)
{
    unsigned long i = 0;
    char * t = NULL;
    long u = 0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        for (i = 0; i < in_tree->sz_hierarchy; ++i) {
            u = in_tree->hierarchy[i][0];
            if (is_child(u)) {
                continue;
            }
            t = in_tree->items[ u ];

            if ( 0 != strcasecmp(t,TXjunction) ) {
                continue;
            }

            const char * name_found = get_field( TXname, i, in_tree);

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

static int check_parent_has_uniquely_named_children( xml_tree_t * in_tree )
{
    unsigned long i = 0, j = 0, k = 0;
    long u = 0, v = 0, context = 0;
    char *t = NULL, *tt = NULL;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        /*First find a parent junction*/
        for (i=0; i < in_tree->sz_hierarchy; ++i) {
            context= in_tree->hierarchy[i][0];
            if ( is_child(context) ) {
                continue;
            }
            t = in_tree->items[ context ];

            if (0 != strcasecmp(t,TXjunction)) {
                continue;
            }

            /*Now look for a child junction */
            for (j=0; j < in_tree->hier_row_lengths[i]; ++j) {
                u = in_tree->hierarchy[i][j];
                if (is_field(u)) {
                    continue;
                }

                tt = in_tree->items[ in_tree->hierarchy[child_to_offset(u)][0] ];
                if ( 0 != strcasecmp(tt,TXjunction)) {
                    continue;
                }

                const char * cA_name = get_field( TXname, child_to_offset(u), in_tree);
                if ( NULL == cA_name) {
                    /*Child without a name --> That should have been checked*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                for (k=j+1; k<in_tree->hier_row_lengths[i]; ++k){
                    /*Look at the other children, if any */
                    v = in_tree->hierarchy[i][k];
                    if (is_field(v)) {
                        continue;
                    }

                    tt = in_tree->items[ in_tree->hierarchy[child_to_offset(v)][0] ];
                    if ( 0 != strcasecmp(tt,TXjunction)) {
                        continue;
                    }
                    const char * cB_name = get_field( TXname, child_to_offset(v), in_tree);
                    if ( NULL == cB_name) {
                        /*Child without a name --> That should have been checked*/
                        erri = ORCM_ERR_BAD_PARAM;
                        break;
                    }
                    if ( 0 == strcmp(cA_name, cB_name) ) {
                        opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                            "ERROR: Junction %lu has two children with the same name: %s", i, cB_name);
                        erri = ORCM_ERR_BAD_PARAM;
                        break;
                    }
                }
                if (ORCM_SUCCESS != erri) {
                    break;
                }
            }

            /* Do not bail out right away in order to try flagging as many errors
             * as possible.
             * Enable the following code if one wants to bail out right away.
             * if (ORCM_SUCCESS != erri) {
             *     break;
             * }
             */
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }/*while(!erri)*/
    return erri;
}

static int check_host_field_has_matching_port( xml_tree_t * in_tree )
{
    unsigned long i = 0;
    char *t = NULL;
    long u = 0;
    const char * host = NULL; /*This will hold a <host> or a <shost> field. */
    int ona_scheduler = 0;

    const char msg_sched[] = "ERROR: Scheduler (lex item= %ld) needs both a shost and a port";
    const char msg_ctrlr[] = "ERROR: Controller (lex item= %ld) needs both a host and a port";

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {

        for (i=0; i < in_tree->sz_hierarchy; ++i) {
            u= in_tree->hierarchy[i][0];
            if ( is_child(u) ) {
                continue;
            }
            t = in_tree->items[ u ];

            if (0 != strcasecmp(t,TXcontrol) && 0 != strcasecmp(t,TXscheduler)) {
                continue;
            }

            const char * port = get_field(TXport, i, in_tree);

            ona_scheduler = 0;
            if (0 == strcasecmp(t,TXcontrol)) {
                host = get_field(TXhost, i, in_tree);
            } else {
                /*On a scheduler */
                host = get_field(TXshost, i, in_tree);
                ona_scheduler = 1;
            }

            if (NULL == port) {
                if (NULL == host) {
                    /* All good as neither have been mentioned.  So a pait it is */
                } else {
                    if (1 == ona_scheduler) {
                        opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                             msg_sched, u);
                    } else {
                        opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                             msg_ctrlr, u);
                    }
                    erri = ORCM_ERR_BAD_PARAM;
                }
            } else {
                if (NULL == host) {
                    if (1 == ona_scheduler) {
                        opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                             msg_sched, u);
                    } else {
                        opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                             msg_ctrlr, u);
                    }
                    erri = ORCM_ERR_BAD_PARAM;
                } else {
                    /*All good as both are present*/
                }
            }

            /* Do not bail out right away in order to try flagging as many errors
             * as possible.
             * Enable the following code if one wants to bail out right away.
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

static int check_unique_scheduler( xml_tree_t * in_tree )
{
    unsigned long i = 0;
    char *t = NULL;
    long u = 0;
    unsigned long scheduler_count = 0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {

        for (i=0; i < in_tree->sz_hierarchy; ++i) {
            u= in_tree->hierarchy[i][0];
            if ( is_child(u) ) {
                continue;
            }
            t = in_tree->items[ u ];

            if (0 != strcasecmp(t,TXscheduler)) {
                continue;
            }
            ++scheduler_count;
        }

        if ( 1 != scheduler_count) {
            opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                 "ERROR: Only a single scheduler is allowed at a time: %lu found.",
                                 scheduler_count);
            erri = ORCM_ERR_BAD_PARAM;
        }

        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }/*while(!erri)*/
    return erri;
}

static int check_unique_host_port_pair( xml_tree_t * in_tree )
{
    unsigned long i = 0, j = 0;
    const char *t = NULL, *t2 = NULL;
    unsigned long p1=0, p2=0;
    long u = 0;
    const char ** allhosts = NULL; /*array pointing to all <host> and <shost> fields*/
                                   /*Its length is in_tree->sz_hierarchy */
    unsigned long * allports = NULL; /*array of port values*/

    char * ampersand_found_in_t  = NULL;

    unsigned long port_value = 0;
    char * endptr = NULL;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {

        allhosts = (const char**) calloc(in_tree->sz_hierarchy, sizeof(char*));
        if (NULL == allhosts) {
            erri =ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }
        allports = (unsigned long*) calloc(in_tree->sz_hierarchy, sizeof(unsigned long));
        if (NULL == allports) {
            erri =ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        /* ASSUMPTIONS:
         * 1) All <host>, or <short>, are properly paired with their
         *    respective ports.
         *    Consequently, a missing <host> , =NULL, will also have a missing
         *    port.
         * 2) The fielded values for ports have been properly ranged.
         */

        /* Not using a hash function yet, as I'm not too sure yet how to handle
         * the ampersand @ hierarchical operator.
         */

        /* First get all <host> <port> pairs */
        for (i=0; i < in_tree->sz_hierarchy; ++i) {
            u= in_tree->hierarchy[i][0];
            if ( is_child(u) ) {
                continue;
            }
            t = in_tree->items[ u ];

            if (0 != strcasecmp(t,TXcontrol) && 0 != strcasecmp(t,TXscheduler)) {
                continue;
            }

            const char * port = get_field(TXport, i, in_tree);
            if (NULL == port) {
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                     "ERROR: A conroller (XML-STRUCT=%lu) has a missing port", i);
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }

            port_value = strtoul(port, &endptr, 10);
            /*endptr not checked --> It is assumed that it has been checked before.*/
            allports[i] = port_value;

            const char * host = NULL;
            if (0 == strcasecmp(t,TXcontrol)) {
                host = get_field(TXhost, i, in_tree);
                /*host == NULL is used as a semaphore.  See below*/
            } else {
                /*On a scheduler */
                host = get_field(TXshost, i, in_tree);
                /*host == NULL is used as a semaphore.  See below*/
            }

            allhosts[i] = host;
        }

        /*Make sure no duplicates are found */
        for (i=0; i < in_tree->sz_hierarchy; ++i) {
            t  = allhosts[i];
            p1 = allports[i];
            if (NULL == t) {
                continue;
            }

            ampersand_found_in_t = NULL;
            ampersand_found_in_t = strchr(t, '@');

            for (j=i+1; j < in_tree->sz_hierarchy; ++j) {
                t2 = allhosts[j];
                p2 = allports[j];
                if (NULL == t2) {
                    continue;
                }

                if (p1 == p2) {
                    if ( 0 == strcmp(t,t2) ) {
                        if (NULL == ampersand_found_in_t) {
                            opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                 "ERROR: Duplicate (s)host-port pair found: port=%lu\t(s)host=%s",
                                 p1, t
                                 );
                            erri = ORCM_ERR_BAD_PARAM;
                            /*Do not bail out as we want to get all possible errors. */
                        } else {
                            /* Possible duplicate because the @ character could
                             * expand to different strings. So assign no error.
                             */
                            opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                                 "WARNING: Possible duplicate (s)host-port pair found: port=%lu\t(s)host=%s",
                                 p1, t
                                 );
                        }
                    }
                }
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }/*while(!erri)*/

    if (NULL != allhosts) {
        free(allhosts);
        allhosts = NULL;
    }
    if (NULL != allports) {
        free(allports);
        allports = NULL;
    }

    return erri;
}

static int check_unique_cluster_on_top( xml_tree_t * in_tree )
{
    unsigned long i = 0, j = 0;
    long u = 0, v = 0;
    char *t = NULL, *tt = NULL;

    unsigned long cluster_count = 0;
    unsigned long cluster_ontop = 0;
    unsigned long record_count = 0;

    const char * jtype = NULL;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {

        for (i=0; i < in_tree->sz_hierarchy; ++i) {
            u= in_tree->hierarchy[i][0];
            if ( is_child(u) ) {
                continue;
            }
            t = in_tree->items[ u ];

            if (0 != strcasecmp(t,TXconfig)) {
                if (0 == strcasecmp(t,TXjunction)) {
                    jtype = get_field(TXtype, i, in_tree);
                    if (NULL == jtype) {
                        opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                       "ERROR: Each junction (LEX item %ld) must have a type property.", u);
                        erri = ORCM_ERR_BAD_PARAM;
                        /*Do not break out.  There might be other errors. */
                        continue;
                    }
                    if (0 == strcasecmp(jtype, FDcluster)) {
                        ++cluster_count;
                    }
                }
                continue;
            }

            const char * crole = get_field(TXrole, i, in_tree);
            if (NULL == crole){
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                       "ERROR: Each configuration must have a role property.");
                erri = ORCM_ERR_BAD_PARAM;
                /*Do not break out.  There might be other errors. */
                continue;
            }
            if (0 != strcasecmp(crole, FDrecord)) {
                continue;
            }
            ++record_count;

            /*Make sure a cluster is at the top of the configuration */
            for (j=0; j < in_tree->hier_row_lengths[i]; ++j) {
                v = in_tree->hierarchy[i][j];
                if (is_field(v)) {
                    continue;
                }
                jtype = get_field(TXtype, child_to_offset(v), in_tree);
                if (NULL == jtype) {
                    tt = in_tree->items[ in_tree->hierarchy[child_to_offset(v)][0] ];
                    if (0 == strcasecmp(tt,TXjunction) ) {
                        opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                        "ERROR: Each junction (LEX item %ld) must have a type property.",
                        in_tree->hierarchy[child_to_offset(v)][0]);
                        erri = ORCM_ERR_BAD_PARAM;
                        /* Break out but continue the complete search. */
                        break;
                    } else {
                        continue;
                    }
                }
                if (0 == strcasecmp(jtype, FDcluster)) {
                    ++cluster_ontop;
                }
            }
        } /* for (i=0; i < in_tree->sz_hierarchy; ++i) */

        if (1 != record_count) {
            opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
            "ERROR: There must be a single RECORD configuration: %lu found.", record_count);
            erri = ORCM_ERR_BAD_PARAM;
        }

        if (1 != cluster_count) {
            opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
            "ERROR: Each RECORD configuration must have a single cluster junction: %lu found.", cluster_count);
            erri = ORCM_ERR_BAD_PARAM;
        }

        if (1 != cluster_ontop) {
            opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
            "ERROR: Each RECORD configuration must have a single cluster junction at the top: %lu found.", cluster_count);
            erri = ORCM_ERR_BAD_PARAM;
        }

        break;
    }/*while(!erri)*/

    return erri;
}

static int check_no_sublevel_on_scheduler( xml_tree_t * in_tree )
{
    int erri = ORCM_SUCCESS;

    unsigned long i=0;
    for (i = 0; i < in_tree->sz_hierarchy; ++i) {
        long u = in_tree->hierarchy[i][0];
        const char * t = in_tree->items[u];
        if (0 != strcasecmp(t, TXscheduler)) {
            continue;
        }
        unsigned long j=0;
        for (j = 1; j < in_tree->hier_row_lengths[i]; ++j) {
            long uu = in_tree->hierarchy[i][j];
            if (is_child(uu)) {
                erri = ORCM_ERR_BAD_PARAM;
                /*Do not break here. Spit out all error.*/
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                "ERROR: Scheduler (%lu) cannot have a child item (%ld)", i, -uu);
            }
        }
    }

    return erri;
}

static int check_no_sublevel_on_controller( xml_tree_t * in_tree )
{
    int erri = ORCM_SUCCESS;

    unsigned long i = 0;
    for (i = 0; i < in_tree->sz_hierarchy; ++i) {
        long u = in_tree->hierarchy[i][0];
        const char * t = in_tree->items[u];
        if (0 != strcasecmp(t, TXcontrol)) {
            continue;
        }
        unsigned long j=0;
        for (j=1; j < in_tree->hier_row_lengths[i]; ++j) {
            long uu = in_tree->hierarchy[i][j];
            if (is_child(uu)) {
                erri = ORCM_ERR_BAD_PARAM;
                /*Do not break here. Spit out all error.*/
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                "ERROR: Controller (%lu) cannot have a child item (%ld)", i, -uu);
            }
        }
    }

    return erri;
}

static int check_controller_on_node_junction( xml_tree_t * in_tree )
{
    /*By convention, the following must be true.
     * =A node may, or not, have a controller.
     * =The field's content of the host tag of the node's controller
     *  must be the same as the name of its parent node junction,
     *  or "@".
     * =If the parent node junction's name is a regular expression,
     *  then that node's controller host field must be "@".
     */

    unsigned long i = 0;
    long u = 0;
    char *t = NULL;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {

        for (i=0; i < in_tree->sz_hierarchy; ++i) {
            u= in_tree->hierarchy[i][0];
            if ( is_child(u) ) {
                continue;
            }
            t = in_tree->items[ u ];

            if (0 != strcasecmp(t,TXjunction)) {
                continue;
            }

            const char * jtype = get_field(TXtype, i, in_tree);
            if (NULL == jtype) {
                /*This should have been checked before as it is fundamental*/
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
            if (0 != strcasecmp(jtype, FDnode)) {
                continue;
            }

            const char * node_name = get_field(TXname, i, in_tree);
            if (NULL == node_name) {
                /*This should not happen.  But it should have been caught elsewhere.*/
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }

            bool node_name_is_regex = is_regex(node_name);

            long child = get_first_child(TXcontrol, i, in_tree);
            if (xtNOTA_CHILD == child) {
                continue;
            }

            const char * ctrl_host = get_field(TXhost, child, in_tree);
            if (NULL == ctrl_host) {
                /*This is not necessarily an error.  For example, it is not an
                 *an error on a MOD configuration.
                 */
                continue;
            }

            if (0 == strcmp(ctrl_host,"@")) {
                /*The controller can do no wrong with "@" as host name.*/
                continue;
            } else {
                if (node_name_is_regex) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                           "ERROR: A node controller host name must be @ if the parent node name is a regex.");
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                } else {
                    if (0 != strcmp(ctrl_host, node_name)) {
                        opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                               "ERROR: A node controller host name must be the same as the non-regex parent node name.");
                        erri = ORCM_ERR_BAD_PARAM;
                        break;
                    }
                }
            }
        }

        break;
    }/*while(!erri)*/

    return erri;
}

static int check_hierarchy_integrity( xml_tree_t * in_tree )
{
    unsigned long i = 0, j = 0;
    long u = 0, v = 0;
    char *t = NULL, *tt = NULL;

    const char * jtype = NULL, *jtype2 = NULL;

    unsigned long junctions_in_cluster = 0;
    unsigned long junctions_in_rows = 0;
    unsigned long junctions_in_racks = 0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {

        /*First make sure that below cluster are only rows */
        for (i=0; i < in_tree->sz_hierarchy; ++i) {
            u= in_tree->hierarchy[i][0];
            if ( is_child(u) ) {
                continue;
            }
            t = in_tree->items[ u ];

            if (0 != strcasecmp(t,TXjunction)) {
                continue;
            }

            jtype = get_field(TXtype, i, in_tree);
            if (NULL == jtype) {
                /*This should have been checked before as it is fundamental*/
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
            if (0 != strcasecmp(jtype, FDcluster)) {
                continue;
            }

            junctions_in_cluster = 0;
            for (j=0; j < in_tree->hier_row_lengths[i]; ++j) {
                v = in_tree->hierarchy[i][j];
                if ( is_field(v) ) {
                    continue;
                }
                tt = in_tree->items[ in_tree->hierarchy[child_to_offset(v)][0] ];

                if ( 0 != strcasecmp(tt,TXjunction)) {
                    continue;
                }

                ++junctions_in_cluster;

                jtype2 = get_field(TXtype, child_to_offset(v), in_tree);
                if (NULL == jtype2) {
                    /*This should have been checked before as it is fundamental*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }

                if (0 != strcasecmp(jtype2, FDrow)) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                    "ERROR: A cluster junction must contain only row junction: %s found.",jtype2);
                    erri = ORCM_ERR_BAD_PARAM;
                    /*Do not break out.  There might be other errors. */
                    continue;
                }
            }
            if (ORCM_SUCCESS != erri) {
                break;
            }
            if (0 == junctions_in_cluster) {
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                "ERROR: A cluster junction must contain at least one row junction: 0 found.");
                erri = ORCM_ERR_BAD_PARAM;
            }

            /*We found our unique cluster.  Time to go. */
            break;
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*First make sure that below rows are only racks */
        for (i=0; i < in_tree->sz_hierarchy; ++i) {
            u= in_tree->hierarchy[i][0];
            if ( is_child(u) ) {
                continue;
            }
            t = in_tree->items[ u ];

            if (0 != strcasecmp(t,TXjunction)) {
                continue;
            }

            jtype = get_field(TXtype, i, in_tree);
            if (NULL == jtype) {
                /*This should have been checked before as it is fundamental*/
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
            if (0 != strcasecmp(jtype, FDrow)) {
                continue;
            }

            junctions_in_rows = 0;
            for (j=0; j < in_tree->hier_row_lengths[i]; ++j) {
                v = in_tree->hierarchy[i][j];
                if ( is_field(v) ) {
                    continue;
                }
                tt = in_tree->items[ in_tree->hierarchy[-v][0] ];

                if ( 0 != strcasecmp(tt,TXjunction)) {
                    continue;
                }

                ++junctions_in_rows;

                jtype2 = get_field(TXtype, -v, in_tree);
                if (NULL == jtype2) {
                    /*This should have been checked before as it is fundamental*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                if (0 != strcasecmp(jtype2, FDrack)) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                    "ERROR: A row junction must contain only rack junction: %s found.",jtype2);
                    erri = ORCM_ERR_BAD_PARAM;
                    /*Do not break out.  There might be other errors. */
                    continue;
                }
            }
            if (ORCM_SUCCESS != erri) {
                break;
            }
            if (0 == junctions_in_rows) {
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                "ERROR: Row junction (LEX item %ld) must contain at least one rack junction",u);
                erri = ORCM_ERR_BAD_PARAM;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*First make sure that below racks are only nodes */
        for (i=0; i < in_tree->sz_hierarchy; ++i) {
            u= in_tree->hierarchy[i][0];
            if ( is_child(u) ) {
                continue;
            }
            t = in_tree->items[ u ];

            if (0 != strcasecmp(t,TXjunction)) {
                continue;
            }

            jtype = get_field(TXtype, i, in_tree);
            if (NULL == jtype) {
                /*This should have been checked before as it is fundamental*/
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
            if (0 != strcasecmp(jtype, FDrack)) {
                continue;
            }

            junctions_in_racks = 0;
            for (j=0; j < in_tree->hier_row_lengths[i]; ++j) {
                v = in_tree->hierarchy[i][j];
                if ( is_field(v) ) {
                    continue;
                }
                tt = in_tree->items[ in_tree->hierarchy[-v][0] ];

                if ( 0 != strcasecmp(tt,TXjunction)) {
                    continue;
                }

                ++junctions_in_racks;

                jtype2 = get_field(TXtype, -v, in_tree);
                if (NULL == jtype2) {
                    /*This should have been checked before as it is fundamental*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                if (0 != strcasecmp(jtype2, FDnode)) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                    "ERROR: A rack junction must contain only node junction: %s found.",jtype2);
                    erri = ORCM_ERR_BAD_PARAM;
                    /*Do not break out.  There might be other errors. */
                    continue;
                }
            }
            if (ORCM_SUCCESS != erri) {
                break;
            }
            if (0 == junctions_in_racks) {
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                "ERROR: Rack junction (LEX item %ld) must contain at least one node junction",u);
                erri = ORCM_ERR_BAD_PARAM;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*First make sure nothing is below node junctions */
        for (i=0; i < in_tree->sz_hierarchy; ++i) {
            u= in_tree->hierarchy[i][0];
            if ( is_child(u) ) {
                continue;
            }
            t = in_tree->items[ u ];

            if (0 != strcasecmp(t,TXjunction)) {
                continue;
            }

            jtype = get_field(TXtype, i, in_tree);
            if (NULL == jtype) {
                /*This should have been checked before as it is fundamental*/
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
            if (0 != strcasecmp(jtype, FDnode)) {
                continue;
            }

            for (j=0; j < in_tree->hier_row_lengths[i]; ++j) {
                v = in_tree->hierarchy[i][j];
                if ( is_field(v) ) {
                    continue;
                }
                tt = in_tree->items[ in_tree->hierarchy[-v][0] ];

                if ( 0 != strcasecmp(tt,TXjunction)) {
                    continue;
                }

                jtype2 = get_field(TXtype, -v, in_tree);
                if (NULL == jtype2) {
                    /*This should have been checked before as it is fundamental*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                "ERROR: A node junction must contain no other junction: %s found.",jtype2);
                erri = ORCM_ERR_BAD_PARAM;
                /*Do not break out.  There might be other errors. */
            }
            if (ORCM_SUCCESS != erri) {
                break;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }/*while(!erri)*/

    return erri;
}

static int augment_hierarchy( xml_tree_t * io_xtree )
{
    unsigned long i, j, k;
    long u,v;
    char *t, *tt;

    const char * jtype;

    unsigned int nb_inserted_row = 0;
    unsigned int nb_inserted_rack = 0;

    char txtbuf[64];
    const unsigned long szH = 8;
    char * holder[szH]; /*Temporary data holder */

    unsigned long startsize;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {

        /*=================================================*/
        /*FIRST: Insert rows to cluster junction if needed */
        startsize = io_xtree->sz_hierarchy;
        for (i=0; i < startsize; ++i) {
            u= io_xtree ->hierarchy[i][0];
            if ( is_child(u) ) {
                continue;
            }
            t = io_xtree ->items[ u ];

            if (0 != strcasecmp(t,TXjunction)) {
                continue;
            }

            jtype = get_field(TXtype, i, io_xtree);
            if (NULL == jtype) {
                /*This should have been checked before as it is fundamental*/
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }

            if (0 != strcasecmp(jtype, FDcluster)) {
                continue;
            }

            for (j=0; j < io_xtree->hier_row_lengths[i]; ++j) {
                v = io_xtree->hierarchy[i][j];
                if ( is_field(v) ) {
                    continue;
                }
                tt = io_xtree->items[ io_xtree->hierarchy[-v][0] ];

                if ( 0 != strcasecmp(tt,TXjunction)) {
                    continue;
                }

                jtype = get_field(TXtype, -v, io_xtree);
                if (NULL == jtype) {
                    /*This should have been checked before as it is fundamental*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }

                if (0 == strcasecmp(jtype, FDrow)) {
                    continue;
                }

                if (0 != strcasecmp(jtype, FDrack) &&
                    0 != strcasecmp(jtype, FDnode) ) {
                    /*This should not happen but it should be picked up later
                     *by a check_* function.
                     */
                    continue;
                }

                /*Insert the needed row */
                /*Here we will use the io_xtree->items as a dictionary.
                 *So the ordering in it will no longer be exactly
                 *what one would see in the CFGI XML input file.
                 *However io_xtree->hierarchy will be correct.
                 */

                unsigned long sz = io_xtree->sz_hierarchy;

                /* 1) deal with io_xtree->hier_row_lengths */
                erri = xml_tree_grow( sizeof(unsigned long), xtBLOCK_SIZE,
                                      sz, (void**)&io_xtree->hier_row_lengths);
                if (ORCM_SUCCESS != erri) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                    "ERROR: Failed to grow row container for a row.");
                    break;
                }
                io_xtree->hier_row_lengths[sz] = 4;
                ++ io_xtree->sz_hierarchy;

                /* 2) deal with io_xtree->hier_row_lengths */
                long * newrow = (long*) calloc(4, sizeof(long));
                if (NULL == newrow) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                    "ERROR: Acquire location for fictitious row failed.");
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }

                erri = xml_tree_grow( sizeof(long *), xtBLOCK_SIZE,
                                      sz, (void**)&io_xtree->hierarchy);
                if (ORCM_SUCCESS != erri) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                    "ERROR: Failed to grow hierarchy for a row.");
                    free(newrow);
                    newrow=NULL;
                    break;
                }
                io_xtree->hierarchy[sz] = newrow;
                newrow = NULL;
                /*No need to increment io_xtree->sz_hierarchy.It was done above.*/

                long herer = io_xtree->sz_items;

                /* io_xtree->items will have
                 *      offset      string
                 *      herer       "junction"  TXjunction
                 *      herer+1     "type"      TXtype
                 *      herer+2     "\trow"     FDrow
                 *      herer+3     "/type"     TXtype
                 *      herer+4     "name"      TXname
                 *      herer+5     "\tFictitiousRow?"  See nb_inserted_row
                 *      herer+6     "/name      TXname
                 *      herer+7     "/junction" TXjunction <--Close junction
                 *
                 *  Note that the above does not make a valid XML inside
                 *  io_xtree->items.
                 */

                io_xtree->hierarchy[sz][0] = herer; /*This is for <junction>.*/
                io_xtree->hierarchy[sz][1] = herer+1; /*This is for <type>.*/
                io_xtree->hierarchy[sz][2] = herer+4; /*This is for <name>.*/
                io_xtree->hierarchy[sz][3] = v;
                io_xtree->hierarchy[i][j] = -1 * sz;

                /*3) Deal with io_xtree->items */
                holder[0] = strdup(TXjunction);
                if (NULL == holder[0]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                holder[1] = strdup(TXtype);
                if (NULL == holder[1]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                sprintf(txtbuf,"\t%s", FDrow);
                holder[2] = strdup(txtbuf);
                if (NULL == holder[2]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                sprintf(txtbuf,"/%s", TXtype);
                holder[3] = strdup(txtbuf);
                if (NULL == holder[3]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                holder[4] = strdup(TXname);
                if (NULL == holder[4]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                ++nb_inserted_row;
                sprintf(txtbuf,"\tFictitiousRow%u", nb_inserted_row);
                holder[5] = strdup(txtbuf);
                if (NULL == holder[5]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                sprintf(txtbuf,"/%s", TXname);
                holder[6] = strdup(txtbuf);
                if (NULL == holder[6]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                sprintf(txtbuf,"/%s", TXjunction);
                holder[7] = strdup(txtbuf);
                if (NULL == holder[7]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }

                /*Now to put them in. */
                for (k=0; k < szH; ++k) {
                    erri = xml_tree_grow(sizeof(char *), xtBLOCK_SIZE,
                                         io_xtree->sz_items,
                                         (void**)&io_xtree->items);
                    if (ORCM_SUCCESS != erri) {
                        opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                        "ERROR: Failed to grow XML dictionary for a row.");
                        break;
                    }
                    io_xtree->items[io_xtree->sz_items++] = holder[k];
                    holder[k]=NULL;
                }
                if (ORCM_SUCCESS != erri) {
                    break;
                }
            } /*End of for (j=0; ...*/
            if (ORCM_SUCCESS != erri) {
                if (ORCM_ERR_OUT_OF_RESOURCE == erri) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                    "ERROR: Ressource acquisition for a row failed.");
                }
                break;
            }

            /*Do not break here in case there is more than one cluster???*/
            /* break; */
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        /*=================================================*/
        /*FIRST: Insert racks to row junction if needed */
        startsize = io_xtree->sz_hierarchy;
        for (i=0; i < startsize; ++i) {

            u= io_xtree ->hierarchy[i][0];
            if ( is_child(u) ) {
                continue;
            }

            t = io_xtree ->items[ u ];
            if (0 != strcasecmp(t,TXjunction)) {
                continue;
            }

            jtype = get_field(TXtype, i, io_xtree);
            if (NULL == jtype) {
                /*This should have been checked before as it is fundamental*/
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }
            if (0 != strcasecmp(jtype, FDrow)) {
                continue;
            }

            for (j=0; j < io_xtree->hier_row_lengths[i]; ++j) {

                v = io_xtree->hierarchy[i][j];
                if ( is_field(v) ) {
                    continue;
                }
                tt = io_xtree->items[ io_xtree->hierarchy[-v][0] ];

                if ( 0 != strcasecmp(tt,TXjunction)) {
                    continue;
                }

                jtype = get_field(TXtype, -v, io_xtree);
                if (NULL == jtype) {
                    /*This should have been checked before as it is fundamental*/
                    erri = ORCM_ERR_BAD_PARAM;
                    break;
                }
                if (0 == strcasecmp(jtype, FDrack)) {
                    continue;
                }

                if ( 0 != strcasecmp(jtype, FDnode) ) {
                    /*This should not happen but it should be picked up later
                     *by a check_* function.
                     */
                    continue;
                }

                /*Insert the needed rack */
                /*Here we will use the io_xtree->items as a dictionary.
                 *So the ordering in it will no longer be exactly
                 *what one would see in the CFGI XML input file.
                 *However io_xtree->hierarchy will be correct.
                 */
                unsigned long sz = io_xtree->sz_hierarchy;

                /* 1) deal with io_xtree->hier_row_lengths */
                erri = xml_tree_grow( sizeof(unsigned long), xtBLOCK_SIZE,
                                      sz, (void**)&io_xtree->hier_row_lengths);
                if (ORCM_SUCCESS != erri) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                    "ERROR: Failed to grow row container for a rack.");
                    break;
                }
                io_xtree->hier_row_lengths[sz] = 4;
                ++ io_xtree->sz_hierarchy;

                /* 2) deal with io_xtree->hier_row_lengths */
                long * newrow = (long*) calloc(4, sizeof(long));
                if (NULL == newrow) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                    "ERROR: Acquire location for fictitious rack failed.");
                    break;
                }

                erri = xml_tree_grow( sizeof(long *), xtBLOCK_SIZE,
                                      sz, (void**)&io_xtree->hierarchy);
                if (ORCM_SUCCESS != erri) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                    "ERROR: Failed to grow hierarchy for a rack.");
                    free(newrow);
                    newrow=NULL;
                    break;
                }
                io_xtree->hierarchy[sz] = newrow;
                newrow = NULL;
                /*No need to increment io_xtree->sz_hierarchy.It was done above.*/

                long herer = io_xtree->sz_items;

                /* io_xtree->items will have
                 *      offset      string
                 *      herer       "junction"  TXjunction
                 *      herer+1     "type"      TXtype
                 *      herer+2     "\trow"     FDrack
                 *      herer+3     "\type"     TXtype
                 *      herer+4     "name"      TXname
                 *      herer+5     "\tFictitiousRack?"  See nb_inserted_rack
                 *      herer+6     "/name      TXname
                 *      herer+7     "/junction" TXjunction <--Close junction
                 *
                 *  Note that the above does not make a valid XML inside
                 *  io_xtree->items.
                 */

                io_xtree->hierarchy[sz][0] = herer; /*This is for <junction>.*/
                io_xtree->hierarchy[sz][1] = herer+1; /*This is for <type>.*/
                io_xtree->hierarchy[sz][2] = herer+4; /*This is for <name>.*/
                io_xtree->hierarchy[sz][3] = v;
                io_xtree->hierarchy[i][j] = -1 * sz;

                /*3) Deal with io_xtree->items */
                holder[0] = strdup(TXjunction);
                if (NULL == holder[0]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                holder[1] = strdup(TXtype);
                if (NULL == holder[1]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                sprintf(txtbuf,"\t%s", FDrack);
                holder[2] = strdup(txtbuf);
                if (NULL == holder[2]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                sprintf(txtbuf,"/%s", TXtype);
                holder[3] = strdup(txtbuf);
                if (NULL == holder[3]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                holder[4] = strdup(TXname);
                if (NULL == holder[4]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                ++nb_inserted_rack;
                sprintf(txtbuf,"\tFictitiousRack%u", nb_inserted_rack);
                holder[5] = strdup(txtbuf);
                if (NULL == holder[5]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                sprintf(txtbuf,"/%s", TXname);
                holder[6] = strdup(txtbuf);
                if (NULL == holder[6]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }
                sprintf(txtbuf,"/%s", TXjunction);
                holder[7] = strdup(txtbuf);
                if (NULL == holder[7]) {
                    erri = ORCM_ERR_OUT_OF_RESOURCE;
                    break;
                }

                /*Now to put them in. */
                for (k=0; k < szH; ++k) {
                    erri = xml_tree_grow(sizeof(char *), xtBLOCK_SIZE,
                                         io_xtree->sz_items,
                                         (void**)&io_xtree->items);
                    if (ORCM_SUCCESS != erri) {
                        opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                        "ERROR: Failed to grow XML dictionary for a rack.");
                        break;
                    }
                    io_xtree->items[io_xtree->sz_items++] = holder[k];
                    holder[k]=NULL;
                }
                if (ORCM_SUCCESS != erri) {
                    break;
                }
            }/*End of for (j=0; ...*/
            if (ORCM_SUCCESS != erri) {
                if (ORCM_ERR_OUT_OF_RESOURCE == erri) {
                    opal_output_verbose( V_LO, orcm_cfgi_base_framework.framework_output,
                    "ERROR: Ressource acquisition for a rack failed.");
                }
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

static int xml_tree_create(xml_tree_t * io_tree)
{
    memset(io_tree, 0, sizeof(xml_tree_t));
    return ORCM_SUCCESS;
}

static int xml_tree_alloc_items(xml_tree_t * io_tree)
{
    /*Set io_xtree->sz_items before using this function */
    if (0 == io_tree->sz_items) {
        return ORCM_ERROR;
    }

    unsigned long sz = (io_tree->sz_items + xtBLOCK_SIZE-1) / xtBLOCK_SIZE;
    sz *= xtBLOCK_SIZE;

    io_tree->items = NULL;
    io_tree->items = (char **) calloc(sz, sizeof(char*));

    if (NULL == io_tree->items) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    return ORCM_SUCCESS;
}

static int xml_tree_alloc_hier_row_lengths(xml_tree_t * io_tree)
{
    /*Set io_tree->sz_hierarchy before using this function */
    if (0 == io_tree->sz_hierarchy) {
        return ORCM_ERROR;
    }

    unsigned long sz = (io_tree->sz_hierarchy + xtBLOCK_SIZE-1) / xtBLOCK_SIZE;
    sz *= xtBLOCK_SIZE;

    io_tree->hier_row_lengths = NULL;
    io_tree->hier_row_lengths = (unsigned long*) calloc(sz, sizeof(unsigned long));

    if (NULL == io_tree->hier_row_lengths) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    return ORCM_SUCCESS;
}

static int xml_tree_alloc_hierarchy(xml_tree_t * io_tree)
{
    /*Set io_tree->sz_hierarchy before using this function */
    if (0 == io_tree->sz_hierarchy) {
        return ORCM_ERROR;
    }

    unsigned long sz = (io_tree->sz_hierarchy + xtBLOCK_SIZE-1) / xtBLOCK_SIZE;
    sz *= xtBLOCK_SIZE;

    io_tree->hierarchy = NULL;
    io_tree->hierarchy = (long**) calloc(sz, sizeof(long*));

    if (NULL == io_tree->hierarchy) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    return ORCM_SUCCESS;
}

static void xml_tree_destroy(xml_tree_t * io_tree)
{
    unsigned long i = 0;

    if (NULL == io_tree) {
        return;
    }

    if (NULL != io_tree->items) {
        for (i=0; i < io_tree->sz_items; ++i)
        {
            if (NULL != io_tree->items[i]) {
                free( io_tree->items[i] );
                io_tree->items[i] = NULL;
            }
        }
        free (io_tree->items);
    }
    io_tree->sz_items = 0;
    io_tree->items = NULL;

    if (NULL != io_tree->hierarchy) {
        for (i=0; i < io_tree->sz_hierarchy; ++i)
        {
            if (NULL != io_tree->hierarchy[i]) {
                free (io_tree->hierarchy[i]);
                io_tree->hierarchy[i] = NULL;
            }
        }
        free (io_tree->hierarchy);
        io_tree->hierarchy = NULL;
    }

    if (NULL != io_tree->hier_row_lengths) {
        free (io_tree->hier_row_lengths );
        io_tree->hier_row_lengths = NULL;
    }

    return;
}

static int xml_tree_grow(unsigned long in_data_size,unsigned long in_block_size,
                         unsigned long in_sz_array, void * *io_array)
{
    if ( 0 ==  in_sz_array % in_block_size ) {
        unsigned long sz = in_sz_array + in_block_size;
        void * t = NULL;
        t = (void *) calloc(sz, in_data_size);
        if (NULL == t) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        if (NULL != *io_array) {
            memcpy(t, *io_array , in_sz_array * in_data_size );
        }
        free(*io_array);
        *io_array = NULL;
        *io_array = t;
        t = NULL;
    }
    return ORCM_SUCCESS;
}

static void xml_tree_print_lexer(const xml_tree_t * in_tree)
{
    unsigned long i;
    if (NULL == in_tree || NULL == in_tree->items) {
        return;
    }
    for (i=0; i < in_tree->sz_items; ++i) {
        if (NULL == in_tree->items[i]) {
            continue;
        }
        /* The orcm_cfgi_base_framework.framework_output of zero used here
         * was taken from orcm_util_print_xml.
         */
        opal_output(0, "XML-LEXR:%lu: %s", i, in_tree->items[i]);
    }
}

void replace_ampersand(char** io_name_to_modify, char * in_parent_name)
{
    if (NULL == io_name_to_modify || NULL == *io_name_to_modify) {
        return;
    }

    if (NULL == strchr(*io_name_to_modify,'@')) {
        return;
    }

    char * parent = "";
    if (NULL != in_parent_name) {
        parent = in_parent_name;
    }

    char * text = strdup(*io_name_to_modify);
    if (NULL == text) {
        return;
    }

    unsigned long sz_parent = strlen(parent);
    unsigned long sz_text = strlen(text);
    unsigned long sz_final = sz_text;

    unsigned long i = 0;

    for (i=0; i < sz_text; ++i) {
        if ('@' == text[i]) {
            sz_final += -1 + sz_parent;
        }
    }
    ++sz_final; //+1 for the ending '\0'.

    char * newtext = (char*) calloc(sz_final, sizeof(char));
    if (NULL == newtext) {
        free(text);
        return;
    }
    free(*io_name_to_modify);
    *io_name_to_modify = NULL;

    unsigned long j = 0;
    unsigned long k = 0;
    for (i=0; i < sz_text; ++i) {
        if ('@' == text[i]) {
           for (j=0; j < sz_parent; ++j) {
              newtext[k++] = parent[j];
           }
        } else {
           newtext[k++] = text[i];
        }
    }


    *io_name_to_modify = newtext;
    newtext = NULL;
    free(text);
    return;
}

