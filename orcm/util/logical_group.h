/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#ifndef ORCM_UTIL_LOGICAL_GROUP_H
#define ORCM_UTIL_LOGICAL_GROUP_H

#include "orcm_config.h"

#include "opal/class/opal_list.h"

BEGIN_C_DECLS

typedef struct {
    opal_list_item_t super;
    char * tag;
    char * nodename;
} orcm_logro_pair_t;
OBJ_CLASS_DECLARATION(orcm_logro_pair_t);

typedef struct orcm_lgroup_t {
    opal_list_t * logro; //It contains object of type orcm_logro_pair_t.
                         //It is not owned by this struct.
    char * storage_filename;
} orcm_lgroup_t;

extern opal_list_t LOGRO; //Global as requested.
extern orcm_lgroup_t LGROUP; //Global as requested.

//====== ACTUAL PUBLIC INTERFACE

//=This function returns ORCM_SUCCESS upon success.
//=in_tag is a c-string representing the name of the tag requested;
// "*" indicates all tags.
//=in_node_regex is a c-string representing the regex for the requested nodes.
// Use "*" if all nodes are required.
//=If nothing is found, both o_tags and o_nodes will bet set to NULL,
// and o_count is zero.
//=If someting is found, o_count will be set to the number of non-NULL elements
// in o_tags and o_nodes.
// o_tags and o_nodes are arrays of strings, each entry representing a tag-node pair.
// Each array has an extra NULL pointer at the end, for compatibility with OPAL argv.
ORCM_DECLSPEC int orcm_grouping_list(char * in_tag, char * in_node_regex,
                                     unsigned int * o_count, char ** *o_tags,
                                     char ** *o_nodes);

//=This function works the same way as orcm_grouping_list.
// But it outputs only a comma separated list of nodes as a single c-string.
ORCM_DECLSPEC int orcm_grouping_listnodes(char * in_tag, unsigned int * o_count,
                                          char ** o_csvlist_nodes);

//=This functions takes in a regex and will expand it as a regex normally would.
//=If in_regexp starts with a '$' then it is considered a grouping.
// The corresponding tag is expanded for all the nodes it refers.
//=For example:
//   in_regexp = N[1:1-2]  --> normal regex -->    o_names = N1 N2
//   in_regexp = $abc      --> logical grouping tag = abc
ORCM_DECLSPEC int orcm_node_names(char *in_regexp, char ***o_names);

ORCM_DECLSPEC int orcm_logical_group_delete(void);

//====== PRIVATE SECTIONS
ORCM_DECLSPEC int orcm_grouping_op_add(int argc, char **argv, opal_list_t * io_group);
ORCM_DECLSPEC int orcm_grouping_op_remove(int argc, char **argv, opal_list_t * io_group);
ORCM_DECLSPEC int orcm_grouping_op_save(int argc, char **argv, opal_list_t * io_group);
ORCM_DECLSPEC int orcm_grouping_op_load(int argc, char **argv, opal_list_t * io_group);

ORCM_DECLSPEC int grouping_parse_from_file(opal_list_t * io_group,
                                           const char * in_filename,
                                           int * o_file_missing);
ORCM_DECLSPEC int grouping_save_to_file(opal_list_t * io_group,
                                        const char * in_filename);

ORCM_DECLSPEC int is_do_all_wildcard(const char * in_text);

ORCM_DECLSPEC void logro_pair_ctor(orcm_logro_pair_t *ptr);
ORCM_DECLSPEC void logro_pair_dtor(orcm_logro_pair_t *ptr);

ORCM_DECLSPEC int is_comment(const char * in_line);
ORCM_DECLSPEC int is_tag(const char * in_line);
ORCM_DECLSPEC int is_attribute(const char * in_line);

ORCM_DECLSPEC void trim(char * in_line, char ** o_new_start);
ORCM_DECLSPEC int get_newline(FILE * in_fin, char * io_line, int in_max_line_length,
                              int * o_eof_found);

ORCM_DECLSPEC int orcm_logical_group_init(void);
ORCM_DECLSPEC int orcm_adjust_logical_grouping_path(char * in_install_dirs_prefix);

END_C_DECLS

#endif //ORCM_UTIL_LOGICAL_GROUP_H
