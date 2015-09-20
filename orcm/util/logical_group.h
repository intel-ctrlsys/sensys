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
#include "opal/class/opal_hash_table.h"
#include "orcm/util/utils.h"

#define MAX_LINE_LENGTH 1024
#define HASH_SIZE 1000

BEGIN_C_DECLS

/* structure definition of a node or a comma separated nodelist */
typedef struct {
    opal_list_item_t super;
    char *node;
} orcm_logical_group_node_t;

void logical_group_node_construct(orcm_logical_group_node_t *node_item);
void logical_group_node_destruct(orcm_logical_group_node_t *node_item);

/* structure definition of the logical grouping */
typedef struct {
    /* In-memory hash table: the key is the group name (tag), and the value is the list
     * of orcm_logical_group_node_t.
     */
    opal_hash_table_t *groups;

    /* Persistent storage file name */
    char *storage_filename;
} orcm_logical_group_t;

extern orcm_logical_group_t LOGICAL_GROUP;
extern char *current_tag;

/* public APIs to be used by everywhere in ORCM */

/* Give a node regex (could be a group name of a collection of nodes),
 * return a comma separated node list */
ORCM_DECLSPEC int orcm_logical_group_node_names_list(char *regex, char **o_nodelist);

/* Give a node regex (could be a group name of a collection of nodes),
 * return an array of nodes */
ORCM_DECLSPEC int orcm_logical_group_node_names(char *regex, char ***o_names);

/* Internal APIs corresponding to the octl command line tools of logical grouping */

/* initialize the logical grouping */
int orcm_logical_group_init(void);

/* add nodes to a group */
int orcm_logical_group_add(char *tag, char *node_regex, opal_hash_table_t *io_groups);

/* remove nodes from a group */
int orcm_logical_group_remove(char *tag, char *node_regex, opal_hash_table_t *io_groups);

/* list nodes from a group */
opal_hash_table_t *orcm_logical_group_list(char *tag, char *node_regex,
                                           opal_hash_table_t *groups);

/* cleanup the logical grouping */
int orcm_logical_group_finalize(void);

/* Internal APIs for file operations */

/* load the content of the storage file to an in-memory hash table */
int orcm_logical_group_load_from_file(char *tag, char *storage_filename,
                                      opal_hash_table_t *io_groups);

/* save the content of an in-memory hash table to the storage file */
int orcm_logical_group_save_to_file(char *tag, char *storage_filename,
                                    opal_hash_table_t *groups);

/* functions that may be used by others */

/* concatenate multiple items in a list to one */
opal_list_t *orcm_logical_group_convert_nodes_list(opal_list_t *nodes_list,
                                                   unsigned int max_size);

/* check if the text is a star that stands for a wild card*/
int is_do_all_wildcard(char *text);
END_C_DECLS

#endif //ORCM_UTIL_LOGICAL_GROUP_H
