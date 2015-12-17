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
#include <fcntl.h>

#define MAX_LINE_LENGTH 1024
#define HASH_SIZE 1000

BEGIN_C_DECLS

/* structure definition of a group member */
typedef struct {
    opal_list_item_t super;
    char *member;
} orcm_logical_group_member_t;

void logical_group_member_construct(orcm_logical_group_member_t *member_item);
void logical_group_member_destruct(orcm_logical_group_member_t *member_item);

/* structure definition of the logical grouping */
typedef struct {
    /* In-memory hash table: the key is the group name (tag), and the value is the list
     * of orcm_logical_group_member_t.
     */
    opal_hash_table_t *groups;

    /* Persistent storage file name */
    char *storage_filename;
} orcm_logical_group_t;

typedef struct {
    FILE *file;
    int fd;
    struct flock file_lock;
} file_with_lock_t;

extern file_with_lock_t logical_group_file_lock;
extern orcm_logical_group_t LOGICAL_GROUP;
extern char *current_tag;

/* public APIs to be used by everywhere in ORCM */

/* Give a regex (could be comma separated items), return an
 * array of strings with each one representing an item
 *
 * params:
 *   Input:
 *     regex: A string. Could be a regex or a group name staring with a '$'. Multiple items
 *            are separated with comma
 *            Example: "node1", "node-[1:1-9]", "$group1", "node1,node2,node[3:1-100]"
 *                     "node1,$group1", "$group1,$group2".
 *   Output:
 *     o_array_string: An array of strings with each string representing an item.
 *
 * */
ORCM_DECLSPEC int orcm_logical_group_parse_array_string(char *regex, char ***o_array_string);

/* Give a node regex (could be comma separated items), return a
 * comma separated string containing all the items
 *
 * params:
 *   Input:
 *     regex: A string. Could be a regex or a group name staring with a '$'. Multiple items
 *            are separated with comma
 *            Example: "node1", "node-[1:1-9]", "$group1", "node1,node2,node[3:1-100]"
 *                     "node1,$group1", "$group1,$group2".
 *   Output:
 *     o_string: A string of comma separated items.
 *
 * */
ORCM_DECLSPEC int orcm_logical_group_parse_string(char *regex, char **o_string);

/* Load the content of the logical group config file into an in-memory hash table, in which
 * the key is the group name, the value is a list of the members in the group
 *
 * params:
 *   Input:
 *     config_file: The logical group configure file. If set to be NULL, the file is at the
 *                  default location: orcm_install/etc/orcm-logical-grouping.txt.
 *   Output:
 *     The global data: the LOGICAL_GROUP is filled.
 *
 * */
ORCM_DECLSPEC int orcm_logical_group_load_to_memory(char *config_file);

/* cleanup the logical grouping */
ORCM_DECLSPEC int orcm_logical_group_finalize(void);

/* Internal APIs corresponding to the octl command line tools of logical grouping */

/* add members to a group stored in hash table */
int orcm_logical_group_add(char *tag, char *regex, opal_hash_table_t *io_groups);

/* remove members from a group stored in hash table */
int orcm_logical_group_remove(char *tag, char *regex, opal_hash_table_t *io_groups);

/* list members from a group stored in hash table */
opal_hash_table_t *orcm_logical_group_list(char *tag, char *regex, opal_hash_table_t *groups);

/* save the content of an in-memory hash table to the storage file */
int orcm_logical_group_save_to_file(char *storage_filename, opal_hash_table_t *groups);

opal_list_t *orcm_logical_group_convert_members_list(opal_list_t *members_list,
                                                     unsigned int max_size);

END_C_DECLS

#endif
