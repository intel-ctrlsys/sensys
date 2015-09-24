/*
 * Copyright (c) 2015      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "logical_group.h"
#include "orcm/constants.h"
#include "orte/util/regex.h"
#include "opal/mca/installdirs/installdirs.h"

/* add just the storage file path to the default location of installing ORCM */
static int orcm_logical_group_adjust_path(char *install_dirs_prefix);

/* whether the node to be added is already exist */
static bool node_exist(opal_list_t *group_nodes, char *new_node);

/* fill the node structure which is a list item */
static orcm_logical_group_node_t *orcm_logical_group_fill_group_node(char *node_regex);

/* internal function to add ndoes to a group */
static int orcm_logical_group_add_internal(char *tag, char **nodes,
                                           opal_list_t *group_nodes,
                                           opal_hash_table_t *io_groups);

/* remove nodes of a group in the hash table */
static int orcm_logical_group_hash_table_remove_nodes(char *tag,
                                                      opal_list_t *value,
                                                      char **nodes,
                                                      opal_hash_table_t *io_groups);

/* remove nodes of all groups */
static int orcm_logical_group_remove_all_tags(char **nodes, int do_all_node,
                                              opal_hash_table_t *io_groups);

/* remove nodes of a group */
static int orcm_logical_group_remove_a_tag(char *tag, char **nodes, int do_all_node,
                                           opal_hash_table_t *io_groups);

/* internal function to remove nodes from groups */
static int orcm_logical_group_remove_internal(char *tag, int do_all_tag,
                                              char **nodes, int do_all_node,
                                              opal_hash_table_t *io_groups);

/* find the specified nodes in the list of nodes */
static opal_list_t *orcm_logical_group_list_specific_nodes(opal_list_t *value,
                                                           char **nodes);

/* append a list of nodes to the node list that is an opal list */
static int orcm_logical_group_list_append(char *nodelist, opal_list_t *nodes_list);

/* list the nodes of all tags */
static opal_hash_table_t* orcm_logical_group_list_all_tags(char **nodes, int do_all_nodes,
                                                           opal_hash_table_t *groups);

/* list the nodes of a tag */
static opal_hash_table_t *orcm_logical_group_list_a_tag(char *tag, char **nodes,
                                                        int do_all_node,
                                                        opal_hash_table_t *groups);

/* internal function to list nodes of groups */
static opal_hash_table_t *orcm_logical_group_list_nodes_internal(char *tag, int do_all_tag,
                                                                 char **nodes,
                                                                 int do_all_node,
                                                                 opal_hash_table_t *groups);

/* whether a line is a comment or not */
static int orcm_logical_group_is_comment(char *line);

/* split a line in a file */
static int orcm_logical_group_split_line(char *line, char ***o_line_fields);

/* process a line that is a tag */
static int orcm_logical_group_process_tag_line(char *tag);

/* process a line */
static int orcm_logical_group_process_line(char *tag, char *line,
                                           opal_hash_table_t *io_groups);

/* trim a line */
static void orcm_logical_group_trim_line(char *line, char **o_line);

/* get a new line from the storage file */
static int orcm_logical_group_get_newline(FILE *storage_file, char *io_line,
                                          int max_line_length, int *o_eof);

/* load the line of a storage file into the hash table the way it is */
static int orcm_logical_group_pass(char *tag, char *node_regex, opal_hash_table_t *io_groups);

/* parsing a storage file */
static int orcm_logical_group_parsing(char *tag, FILE *storage_file, char *line_buf,
                                      opal_hash_table_t *io_groups);

/* pass the storage file */
static int orcm_logical_group_parse_from_file(char *tag, FILE *storage_file,
                                              opal_hash_table_t *io_groups);

/* same the value in the hash table the way it is to a storage file */
static int orcm_logical_group_save_to_file_copy(char *tag, opal_list_t *nodes_list,
                                                FILE *storage_file);

/* combining multiple list item of nodes into one */
static opal_list_t *orcm_logical_group_do_convertion(opal_list_t *nodes_list,
                                                     char *nodelist,
                                                     unsigned int reserved_size);

/* concatenate multiple list item of nodes into one */
static int orcm_logical_group_save_to_file_concat(char *tag, opal_list_t *nodes_list,
                                                  FILE *storage_file);

/* internal function to save the in-memory content to a storage file */
static int orcm_logical_group_save_to_file_internal(char *tag, FILE *storage_file,
                                                    opal_hash_table_t *groups);
/* open the storage file with a specific mode */
static FILE *orcm_logical_group_open_file(char *storage_filename,
                                          char *mode, int *o_file_missing);

/* trim the node regex */
static int orcm_logical_group_trim_noderegex(char *regex, char **o_regex);

/* whether a given group name is valid or not */
static int orcm_logical_group_is_valid_tag(char *tag);

/* prepare for getting a collection of nodes */
static int orcm_logical_group_prepare_for_nodes(char *regex, char **o_regex);

/* check whether a group name exists in the hash table or not */
static int orcm_logical_group_check_tag(char *tag, opal_list_t **value, int *count,
                                        opal_hash_table_t **io_groups);

/* convert the list of nodes to an array of nodes */
static int orcm_logical_group_listval_to_nodes(opal_list_t *value,
                                               char ***o_names, int count);

/* dereference a group name to an array of nodes */
static int orcm_logical_group_tag_to_nodes(char *tag, char ***o_names);

/* calculate the memory size of all the nodes in the list */
static unsigned int orcm_logical_group_list_addup_size(opal_list_t *value, int count);

/* internal function to convert a list of nodes to a comma separated node list */
static int orcm_logical_group_listval_to_nodelist_internal(opal_list_t *value,
                                                           char **o_nodelist, int count);

/* convert a list of nodes to a comma separated node list */
static int orcm_logical_group_listval_to_nodelist(opal_list_t *value,
                                                  char **o_nodelist, int count);

/* dereference a group name to a comma separated node list */
static int orcm_logical_group_tag_to_nodelist(char *tag, char **o_nodelist);

void logical_group_node_construct(orcm_logical_group_node_t *node_item)
{
    node_item->node = NULL;
}

void logical_group_node_destruct(orcm_logical_group_node_t *node_item)
{
    SAFEFREE(node_item->node);
}

OBJ_CLASS_INSTANCE(orcm_logical_group_node_t, opal_list_item_t,
                   logical_group_node_construct, logical_group_node_destruct);

orcm_logical_group_t LOGICAL_GROUP = {NULL, NULL};
char *current_tag = NULL;

static int orcm_logical_group_adjust_path(char *install_dirs_prefix)
{
    int check = -1;
    char *new_file = NULL;

    /* Not much can be done without a user directory. So use default. */
    if(NULL == install_dirs_prefix) {
        return ORCM_SUCCESS;
    }

    check = asprintf(&new_file, "%s/etc/orcm-logical_grouping.txt", install_dirs_prefix);
    if (-1 == check) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    /* Nothing much more can be done.  So let everything as is. */
    if (NULL == new_file) {
        return ORCM_SUCCESS;
    }

    SAFEFREE(LOGICAL_GROUP.storage_filename);
    LOGICAL_GROUP.storage_filename = new_file;
    new_file = NULL;

    return ORCM_SUCCESS;
}

int orcm_logical_group_init(void)
{
    int erri = -1;

    if (NULL == LOGICAL_GROUP.groups) {
        SAFEFREE(LOGICAL_GROUP.storage_filename);
        LOGICAL_GROUP.storage_filename = strdup("./orcm-logical_grouping.txt");
        if (NULL == LOGICAL_GROUP.storage_filename) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }

        erri = orcm_logical_group_adjust_path(opal_install_dirs.prefix);
        if( ORCM_SUCCESS != erri) {
            return erri;
        }

        LOGICAL_GROUP.groups = OBJ_NEW(opal_hash_table_t);
        if (NULL == LOGICAL_GROUP.groups) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        opal_hash_table_init(LOGICAL_GROUP.groups, HASH_SIZE);
    }

    return ORCM_SUCCESS;
}

static int orcm_logical_group_hash_table_get(opal_hash_table_t *groups, char *tag,
                                             opal_list_t **o_group_nodes)
{
    int erri = opal_hash_table_get_value_ptr(groups, tag, strlen(tag), (void**)o_group_nodes);
    if (OPAL_ERR_NOT_FOUND == erri || NULL == *o_group_nodes) {
        *o_group_nodes = OBJ_NEW(opal_list_t);
        if (NULL == *o_group_nodes) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
    }

    return ORCM_SUCCESS;
}

static bool node_exist(opal_list_t *group_nodes, char *new_node)
{
    bool exist = false;
    orcm_logical_group_node_t *node_item = NULL;

    OPAL_LIST_FOREACH(node_item, group_nodes, orcm_logical_group_node_t) {
        if (NULL == node_item) {
            break;
        }
        if (0 == strncmp(node_item->node, new_node, strlen(node_item->node))) {
            exist = true;
            break;
        }
    }

    return exist;
}

static orcm_logical_group_node_t *orcm_logical_group_fill_group_node(char *node_regex)
{
    orcm_logical_group_node_t *node_item = OBJ_NEW(orcm_logical_group_node_t);
    if (NULL != node_item) {
        node_item->node = strdup(node_regex);
    }

    return node_item;
}

static int orcm_logical_group_add_internal(char *tag, char **nodes,
                                           opal_list_t *group_nodes,
                                           opal_hash_table_t *io_groups)
{
    int index = -1, count = -1, erri = ORCM_SUCCESS;
    orcm_logical_group_node_t *node_item = NULL;

    count = opal_argv_count(nodes);
    for (index = 0; index < count; index++) {
        if (false == node_exist(group_nodes, nodes[index])) {
            node_item = orcm_logical_group_fill_group_node(nodes[index]);
            if (NULL == node_item) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }
            opal_list_append(group_nodes, &node_item->super);
        }
    }

    if (erri == ORCM_SUCCESS) {
        erri = opal_hash_table_set_value_ptr(io_groups, tag, strlen(tag), group_nodes);
    }

    return erri;
}

int orcm_logical_group_add(char *tag, char *node_regex, opal_hash_table_t *io_groups)
{
    int erri = ORCM_SUCCESS;
    char **nodes = NULL;
    opal_list_t *group_nodes = NULL;

    erri = orcm_logical_group_hash_table_get(io_groups, tag, &group_nodes);
    if (ORCM_SUCCESS != erri) {
        return erri;
    }

    erri = orte_regex_extract_node_names(node_regex, &nodes);
    if (ORTE_SUCCESS != erri) {
        goto cleanup;
    }

    erri = orcm_logical_group_add_internal(tag, nodes, group_nodes, io_groups);

cleanup:
    opal_argv_free(nodes);
    if (ORCM_SUCCESS != erri) {
        OPAL_LIST_RELEASE(group_nodes);
    }
    return erri;
}

int is_do_all_wildcard(char *text)
{
    int answer = 0;
    if (NULL != text) {
        if(0 == strncmp(text, "*", strlen(text))) {
            answer = 1;
        }
    }
    return answer;
}

static int orcm_logical_group_hash_table_remove_nodes(char *tag,
                                                      opal_list_t *value,
                                                      char **nodes,
                                                      opal_hash_table_t *io_groups)
{
    int index = -1, count = opal_argv_count(nodes);
    orcm_logical_group_node_t *node_item = NULL, *next_node_item = NULL;
    for (index = 0; index < count; index++) {
        OPAL_LIST_FOREACH_SAFE(node_item, next_node_item, value, orcm_logical_group_node_t) {
            if (0 == strncmp(nodes[index], node_item->node, strlen(nodes[index]))) {
                opal_list_remove_item(value, &node_item->super);
                OBJ_RELEASE(node_item);
                break;
            }
        }
    }

    if (opal_list_is_empty(value)) {
        return opal_hash_table_remove_value_ptr(io_groups, tag, strlen(tag));
    }

    return opal_hash_table_set_value_ptr(io_groups, tag, strlen(tag), value);
}

static int orcm_logical_group_remove_all_tags(char **nodes, int do_all_node,
                                              opal_hash_table_t *io_groups)
{
    int erri = ORCM_SUCCESS;
    char *key = NULL;
    size_t key_size = 0;
    opal_list_t *value = NULL;
    void *in_node = NULL, *out_node = NULL;

    if (do_all_node) {
        return opal_hash_table_remove_all(io_groups);
    }

    while (OPAL_SUCCESS == opal_hash_table_get_next_key_ptr(io_groups, (void**)&key,
                                         &key_size, (void**)&value, in_node, &out_node)) {
            erri = orcm_logical_group_hash_table_remove_nodes(key, value, nodes, io_groups);
            if (ORCM_SUCCESS != erri) {
                return erri;
            }
            in_node = out_node;
            out_node = NULL;
    }

    return erri;
}

static int orcm_logical_group_remove_a_tag(char *tag, char **nodes, int do_all_node,
                                           opal_hash_table_t *io_groups)
{
    int erri = ORCM_SUCCESS;
    opal_list_t *value = NULL;

    if (OPAL_SUCCESS == (erri = opal_hash_table_get_value_ptr(io_groups, tag,
                                                      strlen(tag), (void**)&value))) {
        if (do_all_node) {
            return opal_hash_table_remove_value_ptr(io_groups, tag, strlen(tag));
        }

        return orcm_logical_group_hash_table_remove_nodes(tag, value, nodes, io_groups);
    }

    return erri;
}

static int
orcm_logical_group_remove_internal(char *tag, int do_all_tag, char **nodes,
                                   int do_all_node, opal_hash_table_t *io_groups)
{
    if (do_all_tag) {
        return orcm_logical_group_remove_all_tags(nodes, do_all_node, io_groups);
    }

    return orcm_logical_group_remove_a_tag(tag, nodes, do_all_node, io_groups);
}

int orcm_logical_group_remove(char *tag, char *node_regex, opal_hash_table_t *io_groups)
{
    int erri = ORCM_SUCCESS, do_all_tag = 0, do_all_node = 0;
    char **nodes = NULL;

    if (0 == opal_hash_table_get_size(io_groups)) {
        return erri;
    }

    do_all_tag = is_do_all_wildcard(tag);
    if (0 == (do_all_node = is_do_all_wildcard(node_regex))) {
        erri = orte_regex_extract_node_names(node_regex, &nodes);
        if (ORTE_SUCCESS != erri) {
            goto cleanup;
        }
    }

    erri = orcm_logical_group_remove_internal(tag, do_all_tag, nodes, do_all_node, io_groups);

cleanup:
    opal_argv_free(nodes);

    return erri;
}

static int orcm_logical_group_list_append(char *nodelist, opal_list_t *nodes_list)
{
    int erri = ORCM_SUCCESS;

    orcm_logical_group_node_t *new_node_item = orcm_logical_group_fill_group_node(nodelist);
    if (NULL == new_node_item) {
        return ORCM_ERR_BAD_PARAM;
    }

    opal_list_append(nodes_list, &new_node_item->super);

    return erri;
}

static opal_list_t *
orcm_logical_group_list_specific_nodes(opal_list_t *value, char **nodes)
{
    int index = -1, count = opal_argv_count(nodes);
    orcm_logical_group_node_t *node_item = NULL;
    opal_list_t *new_value = NULL;

    if (0 == count) {
        return NULL;
    }

    new_value = OBJ_NEW(opal_list_t);
    if (NULL != new_value) {
        for (index = 0; index < count; index++) {
            OPAL_LIST_FOREACH(node_item, value, orcm_logical_group_node_t) {
                if (NULL == node_item) {
                    return NULL;
                }
                if (0 == strncmp(nodes[index], node_item->node, strlen(nodes[index]))) {
                    orcm_logical_group_list_append(node_item->node, new_value);
                    break;
                }
            }
        }

        if (opal_list_is_empty(new_value)) {
            OPAL_LIST_RELEASE(new_value);
        }
    }

    return new_value;
}

static opal_hash_table_t*
orcm_logical_group_list_all_tags(char **nodes, int do_all_nodes, opal_hash_table_t *groups)
{
    opal_hash_table_t *o_groups = NULL;
    char *key = NULL;
    size_t key_size = 0;
    opal_list_t *value = NULL, *new_value = NULL;
    void *in_node = NULL, *o_node = NULL;

    if (do_all_nodes) {
        return groups;
    }

    o_groups = OBJ_NEW(opal_hash_table_t);
    if (NULL != o_groups) {
        opal_hash_table_init(o_groups, HASH_SIZE);
        while (OPAL_SUCCESS == opal_hash_table_get_next_key_ptr(groups, (void**)&key,
                                           &key_size, (void**)&value, in_node, &o_node)) {
            new_value = orcm_logical_group_list_specific_nodes(value, nodes);
            if (NULL != new_value) {
                opal_hash_table_set_value_ptr(o_groups, key, key_size, new_value);
            }
            in_node = o_node;
            o_node = NULL;

        }
    }

    return o_groups;
}

static opal_hash_table_t *orcm_logical_group_list_a_tag(char *tag, char **nodes,
                                                        int do_all_node,
                                                        opal_hash_table_t *groups)
{
    opal_hash_table_t *o_groups = NULL;
    opal_list_t *value = NULL, *new_value = NULL;

    if (OPAL_SUCCESS == opal_hash_table_get_value_ptr(groups, tag,
                                                      strlen(tag), (void**)&value)) {
        o_groups = OBJ_NEW(opal_hash_table_t);
        if (NULL != o_groups) {
            opal_hash_table_init(o_groups, HASH_SIZE);
            if (do_all_node) {
                opal_hash_table_set_value_ptr(o_groups, tag, strlen(tag), value);
            } else {
                new_value = orcm_logical_group_list_specific_nodes(value, nodes);
                if (NULL != new_value) {
                    opal_hash_table_set_value_ptr(o_groups, tag, strlen(tag), new_value);
                }
            }
        }
    }

    return o_groups;
}

static opal_hash_table_t *
orcm_logical_group_list_nodes_internal(char *tag, int do_all_tag, char **nodes,
                                       int do_all_node, opal_hash_table_t *groups)
{
    if (do_all_tag) {
        return orcm_logical_group_list_all_tags(nodes, do_all_node, groups);
    }

    return orcm_logical_group_list_a_tag(tag, nodes, do_all_node, groups);
}

opal_hash_table_t *orcm_logical_group_list(char *tag, char *node_regex,
                                           opal_hash_table_t *groups)
{
    int do_all_tag = 0, do_all_node = 0;
    char **nodes = NULL;
    opal_hash_table_t *o_groups = NULL;

    do_all_tag = is_do_all_wildcard(tag);
    if (0 == (do_all_node = is_do_all_wildcard(node_regex))) {
        if (ORTE_SUCCESS != orte_regex_extract_node_names(node_regex, &nodes)) {
            goto cleanup;
        }
    }

    o_groups = orcm_logical_group_list_nodes_internal(tag, do_all_tag,
                                                      nodes, do_all_node, groups);

cleanup:
    opal_argv_free(nodes);
    return o_groups;
}

int orcm_logical_group_finalize(void)
{
    if (NULL != LOGICAL_GROUP.groups) {
        opal_hash_table_remove_all(LOGICAL_GROUP.groups);
        OBJ_RELEASE(LOGICAL_GROUP.groups);
    }

    SAFEFREE(LOGICAL_GROUP.storage_filename);
    SAFEFREE(current_tag);

    return ORCM_SUCCESS;
}

static int orcm_logical_group_is_comment(char *line)
{
    if (NULL == line || '\0' == line[0] || '\n' == line[0] ||
        '\r' == line[0] || '#'  == line[0]
       ) {
        return 1;
    }

    return 0;
}

static int orcm_logical_group_split_line(char *line, char ***o_line_fields)
{
    *o_line_fields = opal_argv_split(line, '=');
    if (2 != opal_argv_count(*o_line_fields)) {
        return ORCM_ERR_BAD_PARAM;
    }

    return ORCM_SUCCESS;
}

static int orcm_logical_group_process_tag_line(char *tag)
{
    if (NULL == current_tag || (0 != strncmp(current_tag, tag, strlen(current_tag)))) {
        SAFEFREE(current_tag);
        current_tag = strdup(tag);
    }

    return ORCM_SUCCESS;
}

static int orcm_logical_group_process_line(char *tag, char *line,
                                           opal_hash_table_t *groups)
{
    int erri = ORCM_SUCCESS;
    char **line_fields = NULL;

    if (orcm_logical_group_is_comment(line)) {
        return erri;
    }

    if (ORCM_SUCCESS != (erri = orcm_logical_group_split_line(line, &line_fields))) {
        goto cleanup;
    }

    if (0 == strncmp(line_fields[0], "group name", strlen(line_fields[0]))) {
        erri = orcm_logical_group_process_tag_line(line_fields[1]);
    } else if (0 == strncmp(line_fields[0], "nodelist", strlen(line_fields[0]))) {
        if (tag == NULL || (1 != is_do_all_wildcard(tag) &&
                            0 != strncmp(current_tag, tag, strlen(current_tag)))) {
            erri = orcm_logical_group_pass(current_tag, line_fields[1], groups);
        } else {
            erri = orcm_logical_group_add(current_tag, line_fields[1], groups);
        }
    } else {
        ORCM_UTIL_ERROR_MSG_WITH_ARG("Not recognize the current line: %s", line);
        erri = ORCM_ERR_BAD_PARAM;
    }

cleanup:
    opal_argv_free(line_fields);
    return erri;
}

static void orcm_logical_group_trim_line(char *line, char **o_line)
{
    char *in_line_travesal = NULL;

    if (NULL == line || NULL == o_line) {
        return;
    }

    /* trim the beginning */
    while(' ' == *line || '\t' == *line) {
        line++;
    }
    *o_line = line;

    /* trim the end */
    if ('\0' != *line) {
        in_line_travesal = line + strlen(line) - 1;
        while (in_line_travesal > line &&
               (' ' == *in_line_travesal || '\t' == *in_line_travesal)) {
            *in_line_travesal = '\0';
            in_line_travesal--;
        }
    }
}

static int orcm_logical_group_get_newline(FILE *storage_file, char *io_line,
                                          int max_line_length, int *o_eof)
{
    char *ret = NULL;

    /* max_line_length -1 is to give space for ending '\0' */
    ret = fgets(io_line, max_line_length - 1, storage_file);
    if (NULL == ret) {
        if (0 != feof(storage_file)) {
            *o_eof = 1;
            return ORCM_SUCCESS;
        }
        return ORCM_ERR_FILE_READ_FAILURE;
    }

    return ORCM_SUCCESS;
}

static int orcm_logical_group_pass(char *tag, char *node_regex,
                                   opal_hash_table_t *io_group)
{
    int erri = ORCM_SUCCESS;
    opal_list_t *group_nodes = NULL;
    erri = orcm_logical_group_hash_table_get(io_group, tag, &group_nodes);
    if (ORCM_SUCCESS != erri) {
        return erri;
    }

    orcm_logical_group_node_t *node_item = orcm_logical_group_fill_group_node(node_regex);
    if (NULL == node_item) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    opal_list_append(group_nodes, &node_item->super);

    return opal_hash_table_set_value_ptr(io_group, tag, strlen(tag), group_nodes);
}

static int orcm_logical_group_parsing(char *tag, FILE *storage_file, char *line_buf,
                                      opal_hash_table_t *io_groups)
{
    int eof = -1, erri = -1;
    char *line_buf_after_trim = NULL;

    while (1) {
        eof = 0;
        memset(line_buf, '\0', strlen(line_buf));
        erri = orcm_logical_group_get_newline(storage_file, line_buf,
                                              MAX_LINE_LENGTH, &eof);
        if (ORCM_SUCCESS != erri || 1 == eof) {
            break;
        }

        if (orcm_logical_group_is_comment(line_buf)) {
            continue;
        }

        orcm_logical_group_trim_line(line_buf, &line_buf_after_trim);
        erri = orcm_logical_group_process_line(tag, line_buf_after_trim, io_groups);
        if (ORCM_SUCCESS != erri) {
            break;
        }
    }

    return erri;
}

static int orcm_logical_group_parse_from_file(char *tag, FILE *storage_file,
                                              opal_hash_table_t *io_groups)
{
    char *line_buf = NULL;
    int erri = -1;

    if (1 >= MAX_LINE_LENGTH) {
        ORCM_UTIL_ERROR_MSG("The line length needs to be set larger than 1");
        return ORCM_ERR_BAD_PARAM;
    }

    line_buf = (char*)malloc(MAX_LINE_LENGTH * sizeof(char));
    if (NULL == line_buf) {
        ORCM_UTIL_ERROR_MSG("Failed to allocate line buffer for logical groupings.");
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    erri = orcm_logical_group_parsing(tag, storage_file, line_buf, io_groups);

    SAFEFREE(line_buf);
    fclose(storage_file);

    return erri;
}

static int orcm_logical_group_save_to_file_copy(char *tag, opal_list_t *nodes_list,
                                                FILE *storage_file)
{
    orcm_logical_group_node_t *node_regex = NULL;
    int ret = 0;

    if (NULL == tag || NULL == nodes_list) {
        return ORCM_SUCCESS;
    }

    ret = fprintf(storage_file, "group name=%s\n", tag);
    if (0 > ret) {
        return ORCM_ERR_FILE_WRITE_FAILURE;
    }

    OPAL_LIST_FOREACH(node_regex, nodes_list, orcm_logical_group_node_t) {
        if (NULL == node_regex) {
            return ORCM_ERR_FILE_WRITE_FAILURE;
        }
        ret = fprintf(storage_file, "nodelist=%s\n", node_regex->node);
        if (0 > ret) {
            return ORCM_ERR_FILE_WRITE_FAILURE;
        }
    }

    return ORCM_SUCCESS;
}

static opal_list_t *orcm_logical_group_do_convertion(opal_list_t *nodes_list,
                                                     char *nodelist,
                                                     unsigned int reserved_size)
{
    orcm_logical_group_node_t *node_item = NULL;
    unsigned int current_size = 0;
    int index = 0, count = opal_list_get_size(nodes_list);
    opal_list_t *new_nodes_list = OBJ_NEW(opal_list_t);
    if (NULL == new_nodes_list) {
        return NULL;
    }

    OPAL_LIST_FOREACH(node_item, nodes_list, orcm_logical_group_node_t) {
        index++;
        current_size = strlen(nodelist);
        if (reserved_size < current_size + strlen(node_item->node) + 1) {
            if (ORCM_SUCCESS != orcm_logical_group_list_append(nodelist, new_nodes_list)) {
                goto clean;
            }
            memset(nodelist, '\0', strlen(nodelist));
        }
        if (0 < strlen(nodelist)) {
            strncat(nodelist, ",", sizeof(char));
        }
        strncat(nodelist, node_item->node, strlen(node_item->node));

        if (index == count && 0 < strlen(nodelist)) {
            if (ORCM_SUCCESS != orcm_logical_group_list_append(nodelist, new_nodes_list)) {
                goto clean;
            }
        }
    }

    return new_nodes_list;

clean:
    OPAL_LIST_RELEASE(new_nodes_list);
    return NULL;
}

opal_list_t *orcm_logical_group_convert_nodes_list(opal_list_t *nodes_list,
                                                   unsigned int max_size)
{
    char *nodelist = NULL;
    opal_list_t *o_nodes_list = NULL;

    if (NULL == nodes_list || opal_list_is_empty(nodes_list) || 0 >= max_size) {
        return NULL;
    }

    nodelist = (char*)calloc(max_size, sizeof(char));
    if (NULL == nodelist) {
        return NULL;
    }

    o_nodes_list = orcm_logical_group_do_convertion(nodes_list, nodelist, max_size);
    SAFEFREE(nodelist);
    return o_nodes_list;
}

static int orcm_logical_group_save_to_file_concat(char *tag, opal_list_t *nodes_list,
                                                  FILE *storage_file)
{
    int erri = ORCM_SUCCESS;
    opal_list_t *new_nodes_list = orcm_logical_group_convert_nodes_list(nodes_list,
                                                  MAX_LINE_LENGTH - strlen("namelist="));
    erri = orcm_logical_group_save_to_file_copy(tag, new_nodes_list, storage_file);
    if (NULL != new_nodes_list) {
        OPAL_LIST_RELEASE(new_nodes_list);
    }
    return erri;
}

static int orcm_logical_group_save_to_file_internal(char *tag, FILE *storage_file,
                                                    opal_hash_table_t *groups)
{
    int erri = ORCM_SUCCESS, ret = 0;
    char *key = NULL;
    size_t key_size = 0;
    opal_list_t *value = NULL;
    void *in_node = NULL, *out_node = NULL;

    if (0 == opal_hash_table_get_size(groups)) {
        ret = fprintf(storage_file, "# Empty logical grouping set\n");
        if (0 > ret) {
            erri = ORCM_ERR_FILE_WRITE_FAILURE;
        }
    } else {
        while (ORCM_SUCCESS == opal_hash_table_get_next_key_ptr(groups, (void**)&key,
                                        &key_size, (void**)&value, in_node, &out_node)) {
            if (0 == strncmp(key, tag, strlen(key)) || 0 == strncmp(tag, "*", strlen(tag))) {
                erri = orcm_logical_group_save_to_file_concat(key, value, storage_file);
                if (ORCM_SUCCESS != erri) {
                    break;
                }
            } else {
                erri = orcm_logical_group_save_to_file_copy(key, value, storage_file);
                if (ORCM_SUCCESS != erri) {
                    break;
                }
            }
            in_node = out_node;
            out_node = NULL;
        }
    }
    fflush(storage_file);
    fclose(storage_file);
    return erri;
}

static FILE *orcm_logical_group_open_file(char *storage_filename,
                                          char *mode, int *o_file_missing)
{
    FILE *storage_file = NULL;

    if (NULL == storage_filename || '\0' == storage_filename[0]) {
        ORCM_UTIL_ERROR_MSG("Bad setup for parsing logical groupings.");
    } else {
        if (NULL == (storage_file = fopen(storage_filename, mode))) {
            if (ENOENT == errno) {
                *o_file_missing = 1;
            } else {
                ORCM_UTIL_ERROR_MSG("Failed to open file for logical groupings.");
            }
        }
    }

    return storage_file;
}

int orcm_logical_group_load_from_file(char *tag, char *storage_filename,
                                      opal_hash_table_t *io_groups)
{
    int file_missing = 0;
    FILE *storage_file = NULL;

    if (NULL == io_groups) {
        ORCM_UTIL_ERROR_MSG("Missing logical group.");
        return ORCM_ERR_BAD_PARAM;
    }

    storage_file = orcm_logical_group_open_file(storage_filename, "r", &file_missing);
    if (NULL == storage_file) {
        if (1 == file_missing) {
            return ORCM_SUCCESS;
        }
        return ORCM_ERR_FILE_OPEN_FAILURE;
    }

    return orcm_logical_group_parse_from_file(tag, storage_file, io_groups);
}

int orcm_logical_group_save_to_file(char *tag, char *storage_filename,
                                    opal_hash_table_t *groups)
{
    int file_missing = 0;
    FILE *storage_file = NULL;

    if (NULL == groups) {
        ORCM_UTIL_ERROR_MSG("Missing logical group.");
        return ORCM_ERR_BAD_PARAM;
    }

    storage_file = orcm_logical_group_open_file(storage_filename, "w", &file_missing);
    if (NULL == storage_file) {
        return ORCM_ERR_FILE_OPEN_FAILURE;
    }

    return orcm_logical_group_save_to_file_internal(tag, storage_file, groups);
}

static int orcm_logical_group_trim_noderegex(char *regex, char **o_regex)
{
    char *regexp = regex;
    int erri = ORCM_SUCCESS;

    if (NULL != regexp) {
        while('\0' != *regexp && (' ' == *regexp || '\t' == *regexp)) {
            ++regexp;
        }

        if ('\0' == *regexp) {
            fprintf(stderr, "\n  ERROR: Given node regex is empty.\n");
            regexp = NULL;
            erri = ORCM_ERR_BAD_PARAM;
        }
    }

    *o_regex = regexp;
    return erri;
}

static int orcm_logical_group_is_valid_tag(char *tag)
{
    char *comma = NULL, *openb = NULL, *closeb = NULL;

    if ('\0' == tag[0]) {
        ORCM_UTIL_ERROR_MSG("ERROR: Given logical grouping tag is empty.");
        return ORCM_ERR_BAD_PARAM;
    }

    if (0 == strncmp(tag, "*", strlen(tag))) {
        ORCM_UTIL_ERROR_MSG("Given logical grouping tag could not be *");
        return ORCM_ERR_BAD_PARAM;
    }
    comma = strchr(tag, ',');
    openb = strchr(tag, '[');
    closeb= strchr(tag, ']');

    /* the regular expression can not include comma, nor the openb bigger than the closeb*/
    if (NULL != comma) {
        ORCM_UTIL_ERROR_MSG("ERROR: Given logical grouping tag must not be a csv list.");
        return ORCM_ERR_BAD_PARAM;
    }
    if (openb < closeb) {
        ORCM_UTIL_ERROR_MSG("ERROR: Given logical grouping tag must not be a regex.");
        return ORCM_ERR_BAD_PARAM;
    }

    return ORCM_SUCCESS;
}

static int orcm_logical_group_prepare_for_nodes(char *regex, char **o_regex)
{
    int erri = orcm_logical_group_trim_noderegex(regex, o_regex);
    if (ORCM_SUCCESS != erri) {
        return erri;
    }

    return orcm_logical_group_init();
}

static int orcm_logical_group_check_tag(char *tag, opal_list_t **value, int *count,
                                        opal_hash_table_t **io_groups)
{
    int erri = orcm_logical_group_is_valid_tag(tag);
    if (ORCM_SUCCESS != erri) {
        return erri;
    }

    *io_groups = orcm_logical_group_list(tag, "*", LOGICAL_GROUP.groups);
    if (NULL == *io_groups || 0 == opal_hash_table_get_size(*io_groups) || ORCM_SUCCESS !=
        opal_hash_table_get_value_ptr(*io_groups, tag, strlen(tag), (void**)value)) {
        erri = ORCM_ERR_BAD_PARAM;
    } else if (NULL == *value || 0 == (*count = opal_list_get_size(*value))) {
        erri = ORCM_ERR_BAD_PARAM;
    }

    return erri;
}

static int
orcm_logical_group_listval_to_nodes(opal_list_t *value, char ***o_names, int count)
{
    int index = 0;
    orcm_logical_group_node_t *node_item = NULL;

    *o_names = (char**)calloc(count, sizeof(char*));
    if (NULL == *o_names) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    OPAL_LIST_FOREACH(node_item, value, orcm_logical_group_node_t) {
        if (NULL == node_item) {
            opal_argv_free(*o_names);
            return ORCM_ERR_BAD_PARAM;
        }
        (*o_names)[index++] = strdup(node_item->node);
    }

    return ORCM_SUCCESS;
}

static int orcm_logical_group_tag_to_nodes(char *tag, char ***o_names)
{
    int count = -1, erri = ORCM_SUCCESS;
    opal_hash_table_t *io_groups = NULL;
    opal_list_t *value = NULL;

    erri = orcm_logical_group_check_tag(tag, &value, &count, &io_groups);
    if (ORCM_SUCCESS != erri) {
        goto cleanup;
    }

    erri = orcm_logical_group_listval_to_nodes(value, o_names, count);

cleanup:
    if (NULL != io_groups) {
        opal_hash_table_remove_all(io_groups);
        OBJ_RELEASE(io_groups);
    }
    return erri;
}

int orcm_logical_group_node_names(char *regex, char ***o_names)
{
    int erri = ORCM_SUCCESS;
    char *o_regex = NULL;

    erri = orcm_logical_group_prepare_for_nodes(regex, &o_regex);
    if (ORCM_SUCCESS != erri) {
        goto finalize;
    }

    /* If the node regex is a tag: starting with $ */
    if ('$' == o_regex[0]) {
        ++o_regex; //Omit the '$' character
        erri = orcm_logical_group_load_from_file(o_regex, LOGICAL_GROUP.storage_filename,
                                          LOGICAL_GROUP.groups);
        if (ORCM_SUCCESS != erri) {
            goto finalize;
        }
        erri = orcm_logical_group_tag_to_nodes(o_regex, o_names);
    } else {
        erri = orte_regex_extract_node_names(regex, o_names);
    }

finalize:
    orcm_logical_group_finalize();
    return erri;
}

static unsigned int orcm_logical_group_list_addup_size(opal_list_t *value, int count)
{
    unsigned int size = 0;
    int index = 0;
    orcm_logical_group_node_t *node_item = NULL;

    OPAL_LIST_FOREACH(node_item, value, orcm_logical_group_node_t) {
        index++;
        if (NULL == node_item) {
            break;
        }
        size += (strlen(node_item->node) + 1);
    }

    return size;
}

static int orcm_logical_group_listval_to_nodelist_internal(opal_list_t *value,
                                                           char **o_nodelist, int count)
{
    int index = 0, erri = ORCM_SUCCESS;
    orcm_logical_group_node_t *node_item = NULL;

    OPAL_LIST_FOREACH(node_item, value, orcm_logical_group_node_t) {
        index++;
        if (NULL == node_item) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        } else {
            strncat(*o_nodelist, node_item->node, strlen(node_item->node));
            if (index != count) {
                strncat(*o_nodelist, ",", sizeof(char));
            }
        }
    }

    return erri;
}

static int orcm_logical_group_listval_to_nodelist(opal_list_t *value,
                                                  char **o_nodelist, int count)
{
    unsigned int size = 0;
    int erri = ORCM_SUCCESS;

    size = orcm_logical_group_list_addup_size(value, count);
    if (0 == size) {
        return ORCM_ERR_BAD_PARAM;
    }

    *o_nodelist = (char*)calloc(size, sizeof(char));
    if (NULL == *o_nodelist) {
        return ORCM_ERR_BAD_PARAM;
    }

    erri = orcm_logical_group_listval_to_nodelist_internal(value, o_nodelist, count);
    if (ORCM_SUCCESS != erri) {
        SAFEFREE(*o_nodelist);
    }

    return erri;
}

static int orcm_logical_group_tag_to_nodelist(char *tag, char **o_nodelist)
{
    int count = -1, erri = ORCM_SUCCESS;
    opal_hash_table_t *io_groups = NULL;
    opal_list_t *value = NULL;

    erri = orcm_logical_group_check_tag(tag, &value, &count, &io_groups);
    if (ORCM_SUCCESS != erri) {
        goto cleanup;
    }

    erri = orcm_logical_group_listval_to_nodelist(value, o_nodelist, count);

cleanup:
    if (NULL != io_groups) {
        opal_hash_table_remove_all(io_groups);
        OBJ_RELEASE(io_groups);
    }
    return erri;
}

int orcm_logical_group_node_names_list(char *regex, char **o_nodelist)
{
    int erri = ORCM_SUCCESS;
    char *o_regex = NULL;

    erri = orcm_logical_group_prepare_for_nodes(regex, &o_regex);
    if (ORCM_SUCCESS != erri) {
        goto finalize;
    }

    if ('$' == o_regex[0]) {
        ++o_regex;
        erri = orcm_logical_group_load_from_file(NULL, LOGICAL_GROUP.storage_filename,
                                                 LOGICAL_GROUP.groups);
        if (ORCM_SUCCESS != erri) {
            goto finalize;
        }
        erri = orcm_logical_group_tag_to_nodelist(o_regex, o_nodelist);
    } else {
        *o_nodelist = regex;
    }

finalize:
    orcm_logical_group_finalize();
    return erri;
}
