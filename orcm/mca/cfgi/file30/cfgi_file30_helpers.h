/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef CFGI_FILE30_HELPERS_H
#define CFGI_FILE30_HELPERS_H

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

#define GET_ITEM_FROM_OPAL_LIST(list,type) \
    (type *) ((list))->opal_list_sentinel.opal_list_next

#include <regex.h>
#include "orcm/mca/cfgi/cfgi.h"

int stringMatchRegex(char* str, char* pattern);
int print_pugi_opal_list (opal_list_t *root);
orcm_cfgi_xml_parser_t * build_cfgi_xml_parser_object(char* name);
orcm_value_t * get_child(opal_list_t* parent, char* child_name);
void get_type_and_name(orcm_value_t * parent, char **type, char **name);
int convert_to_orcm_cfgi_xml_parser_t_list(opal_list_t* source, opal_list_t *target);
int parse_configuration_tag(opal_list_t* source, opal_list_t *target);
int parse_scheduler_tag(opal_list_t* source, opal_list_t *target);
int parse_controller_tag(opal_list_t* source, opal_list_t *target);
int parse_junction_tag(opal_list_t* source, opal_list_t *target, char *name, char *type);
int merge_controller_with_node(opal_list_t* source, opal_list_t *target);

int parse_row_junction(opal_list_t* source, opal_list_t *target, char *name, char *parent_type);
int parse_rack_junction(opal_list_t* source, opal_list_t *target, char *name, char *parent_type);
int parse_node_junction(opal_list_t* source, opal_list_t *target, char *name, char *parent_type);
int select_parse_junction_tag(orcm_value_t* source_item, opal_list_t* target, char* parent_type);
int parse_cluster_junction(opal_list_t* source, opal_list_t *target, char *name, char *parent_type);
char * tolower_cstr(char * in);
int check_record_field(char *field_value, int *record);
int check_lex_tags_and_field(opal_list_t *root);
int is_not_ignored(char* tag);
int search_lex_tags_and_field(opal_list_t *root, int *role_count, int *aggs_count);
int check_lex_port_field(char *field_value);
int check_aggregator_yes_no_field(char *field_value);
int is_singleton(const char * in_tagtext);
int check_duplicate_singleton(opal_list_t *root);
int search_singletons(opal_list_t *root, int *mem_counter);

int cfgi30_check_configure_hosts_ports(opal_list_t *elements_list);
int cfgi30_check_scheduler_hosts_ports(opal_list_t *scheduler, char ***hosts,
                                       int **ports);
int cfgi30_check_junction_hosts_ports(opal_list_t *junction, char ***hosts,
                                      int **ports, char **lastName);
int cfgi30_check_controller_hosts_ports(opal_list_t *controller, char **host,
                                        int *port);
int cfgi30_check_unique_hosts_ports(char ***hosts, int **ports, char **name,
                                    char **host, int *port, char **lastName,
                                    opal_list_t **next_junctions, char is_node);
int cfgi30_expand_name_at(char **name, char **lastName);
int cfgi30_add_valid_noregex_host_port(char ***hosts, int **ports, char **host,
                                       int *port, char ***new_names,
                                       char is_node);
int cfgi30_add_valid_regex_hosts_ports(char ***hosts, int **ports, char **host,
                                       int *port, char ***new_names,
                                       char is_node, char is_regex,
                                       opal_list_t **next_junctions);
int cfgi30_add_uniq_host_port(char ***hosts, int **ports, char **host, int *port);
int cfgi30_add_host_port(char ***hosts, int **ports, char **host, int *port);
char cfgi30_exists_host_port(char ***hosts, int **ports, char **host,
                             int *port);
void cfgi30_free_string_array(char ***array);

#endif /* CFGI_FILE30_H */
