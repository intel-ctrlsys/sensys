/*
 * Copyright (c) 2015      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "orcm/tools/octl/common.h"
#include "orcm/util/logical_group.h"
#include "orte/util/regex.h"

int orcm_octl_logical_group_add(int argc, char **argv)
{
    int erri = ORCM_SUCCESS;
    char *tag = NULL, *node_regex = NULL;

    if (4 != argc) {
        orte_show_help("help-octl.txt",
                       "octl:grouping:add-usage", true, "invalid arguments!");
        return ORCM_ERR_BAD_PARAM;
    }
    tag = argv[2];
    node_regex = argv[3];
    if (0 == strncmp(tag, "*", strlen(tag) + 1) ||
        0 == strncmp(node_regex, "*", strlen(node_regex) + 1)) {
        orte_show_help("help-octl.txt", "octl:grouping:add-wildcard", true);
        return ORCM_ERR_BAD_PARAM;
    }

    if (ORCM_SUCCESS != (erri = orcm_logical_group_init())) {
        goto cleanup;
    }

    if (ORCM_SUCCESS != (erri = orcm_logical_group_load_from_file(tag,
                         LOGICAL_GROUP.storage_filename, LOGICAL_GROUP.groups))) {
        goto cleanup;
    }

    if (ORCM_SUCCESS != (erri = orcm_logical_group_add(tag,
                         node_regex, LOGICAL_GROUP.groups))) {
        goto cleanup;
    }

    if (ORCM_SUCCESS != (erri = orcm_logical_group_save_to_file(tag,
                         LOGICAL_GROUP.storage_filename, LOGICAL_GROUP.groups))) {
        goto cleanup;
    }

    ORCM_UTIL_MSG("\nGrouping: Add done successfully!");

cleanup:
    orcm_logical_group_finalize();
    return erri;
}

int orcm_octl_logical_group_remove(int argc, char **argv)
{
    int erri = ORCM_SUCCESS;
    char *tag = NULL, *node_regex = NULL, *pass_tag = NULL, *err_str = NULL;

    if (4 != argc) {
        orte_show_help("help-octl.txt",
                       "octl:grouping:remove-usage", true, "invalid arguments!");
        return ORCM_ERR_BAD_PARAM;
    }
    tag = argv[2];
    node_regex = argv[3];

    if (ORCM_SUCCESS != (erri = orcm_logical_group_init()))
        goto cleanup;

    if (0 == is_do_all_wildcard(node_regex)) {
        pass_tag = tag;
    }
    if (ORCM_SUCCESS != (erri = orcm_logical_group_load_from_file(pass_tag,
                         LOGICAL_GROUP.storage_filename, LOGICAL_GROUP.groups))) {
        goto cleanup;
    }

    if (ORCM_SUCCESS != (erri = orcm_logical_group_remove(tag,
                         node_regex, LOGICAL_GROUP.groups))) {
        if (ORCM_ERR_NO_ANY_GROUP == erri ||
            ORCM_ERR_GROUP_NOT_EXIST == erri || ORCM_ERR_NODE_NOT_EXIST == erri) {
            orcm_err2str(erri, (const char**)(&err_str));
            orte_show_help("help-octl.txt", "octl:grouping:remove-failure", true, err_str);
            erri = ORCM_ERR_BAD_PARAM;
        }
        goto cleanup;
    }

    if (ORCM_SUCCESS != (erri = orcm_logical_group_save_to_file(tag,
                         LOGICAL_GROUP.storage_filename, LOGICAL_GROUP.groups))) {
        goto cleanup;
    }

    ORCM_UTIL_MSG("\nGrouping: Remove done successfully!");

cleanup:
    orcm_logical_group_finalize();
    return erri;
}

static int orcm_octl_logical_group_print_list(opal_hash_table_t *groups)
{
    char *key = NULL;
    size_t key_size = 0;
    opal_list_t *value = NULL, *new_value = NULL;
    void *in_node = NULL, *o_node = NULL;
    orcm_logical_group_node_t *node_item = NULL;

    while (ORCM_SUCCESS == opal_hash_table_get_next_key_ptr(groups, (void**)&key,
                                         &key_size, (void**)&value, in_node, &o_node)) {
        new_value = orcm_logical_group_convert_nodes_list(value, MAX_LINE_LENGTH);
        if (NULL != new_value && !opal_list_is_empty(new_value)) {
            ORCM_UTIL_MSG_WITH_ARG("\ngroup name=%s", key);
            OPAL_LIST_FOREACH(node_item, new_value, orcm_logical_group_node_t) {
                ORCM_UTIL_MSG_WITH_ARG("nodelist=%s", node_item->node);
            }
        }
        if (NULL != new_value) {
            OPAL_LIST_RELEASE(new_value);
        }
        in_node = o_node;
        o_node = NULL;
    }

    return ORCM_SUCCESS;
}

int orcm_octl_logical_group_list(int argc, char **argv)
{
    int erri = ORCM_SUCCESS;
    opal_hash_table_t *o_groups = NULL;
    char *tag = NULL, *node_regex = NULL, *pass_tag = NULL;

    if (4 != argc) {
        orte_show_help("help-octl.txt",
                       "octl:grouping:list-usage", true, "invalid arguments!");
        return ORCM_ERR_BAD_PARAM;
    }
    tag = argv[2];
    node_regex = argv[3];

    if (ORCM_SUCCESS != (erri = orcm_logical_group_init())) {
        goto cleanup;
    }

    if (0 == is_do_all_wildcard(node_regex)) {
        pass_tag = tag;
    }
    if (ORCM_SUCCESS != (erri = orcm_logical_group_load_from_file(pass_tag,
                         LOGICAL_GROUP.storage_filename, LOGICAL_GROUP.groups))) {
        goto cleanup;
    }

    o_groups = orcm_logical_group_list(tag, node_regex, LOGICAL_GROUP.groups);
    if (NULL == o_groups || 0 == opal_hash_table_get_size(o_groups)) {
        ORCM_UTIL_MSG("\nThere is no record!");
        goto cleanup;
    }

    erri = orcm_octl_logical_group_print_list(o_groups);

cleanup:
    if (o_groups != LOGICAL_GROUP.groups && NULL != o_groups) {
        opal_hash_table_remove_all(o_groups);
        OBJ_RELEASE(o_groups);
    }
    orcm_logical_group_finalize();
    return erri;
}
