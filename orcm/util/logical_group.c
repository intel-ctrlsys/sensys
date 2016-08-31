/*
 * Copyright (c) 2015-2016 Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "orcm/util/logical_group.h"
#include "orcm/constants.h"
#include "orte/util/regex.h"
#include "opal/mca/installdirs/installdirs.h"

/* Initialization the logical grouping */
static int orcm_logical_group_init(char *config_file);

/* whether the member to be added is already exist */
static bool member_exist(opal_list_t *group_members, char *new_member);

/* internal function to add members to a group */
static int orcm_logical_group_add_internal(char *tag, char **new_members,
                                           opal_list_t *group_members,
                                           opal_hash_table_t *io_groups);

/* check if all the members do exist in a group */
static int orcm_logical_group_all_members_exist_one_tag(opal_list_t *value, char **members);

/* check if all the members do exist in all groups */
static int orcm_logical_group_all_members_exist_all_tags(opal_hash_table_t *groups,
                                                         char **members);

/* remove members of a group in the hash table */
static int orcm_logical_group_hash_table_remove_members(char *tag,
                                                        opal_list_t *value,
                                                        char **members,
                                                        opal_hash_table_t *io_groups);

/* remove members of all groups */
static int orcm_logical_group_remove_all_tags(char **members, int do_all_member,
                                              opal_hash_table_t *io_groups);

/* remove members of a group */
static int orcm_logical_group_remove_a_tag(char *tag, char **members, int do_all_member,
                                           opal_hash_table_t *io_groups);

/* internal function to remove members from groups */
static int orcm_logical_group_remove_internal(char *tag, int do_all_tag,
                                              char **members, int do_all_member,
                                              opal_hash_table_t *io_groups);

/* find the specified members in the list of members */
static opal_list_t *orcm_logical_group_list_specific_members(opal_list_t *value,
                                                             char **members);

/* append a list of members to the member list that is an opal list */
static int orcm_logical_group_list_append(char *memberlist, opal_list_t *members_list);

/* list the members of all tags */
static opal_hash_table_t* orcm_logical_group_list_all_tags(char **members, int do_all_member,
                                                           opal_hash_table_t *groups);

/* list the members of a tag */
static opal_hash_table_t *orcm_logical_group_list_a_tag(char *tag, char **members,
                                                        int do_all_member,
                                                        opal_hash_table_t *groups);

/* internal function to list members of groups */
static opal_hash_table_t *orcm_logical_group_list_internal(char *tag, int do_all_tag,
                                                           char **members,
                                                           int do_all_member,
                                                           opal_hash_table_t *groups);
/* open the storage file */
static int orcm_logical_group_open_file(char *storage_filename);

/* create an XML file with logicalgroup tag in it */
static int orcm_logical_group_crate_xml_file(char *storage_filename);

/* load the content of the storage file to an in-memory hash table */
static int orcm_logical_group_load_from_file(char *storage_filename,
                                             opal_hash_table_t *io_groups);

/* trim a line */
static void orcm_logical_group_trim_string(char *string, char **o_string);

/* parsing a storage file */
static int orcm_logical_group_parsing(opal_list_t* result_list, opal_hash_table_t *io_groups);

/* pass the storage file */
static int orcm_logical_group_parse_from_file(opal_hash_table_t *io_groups);

/* combining multiple list item of members into one */
static opal_list_t *orcm_logical_group_do_convertion(opal_list_t *members_list,
                                                     char *memberlist,
                                                     unsigned int reserved_size);

/* concatenate multiple list item of members into one */
static int orcm_logical_group_save_to_file_concat(opal_list_t *parser_input, char *tag,
                                                  opal_list_t *members_list);

/* internal function to save the in-memory content to a storage file */
static int orcm_logical_group_save_to_file_internal(opal_hash_table_t *groups);

/* dereference a group name to an array of members */
static int orcm_logical_group_tag_to_members_nested(char ***tag_array, char ***o_array_string);

/* calculate the memory size of all the members in an array of string */
static unsigned int orcm_logical_group_argv_addup_size(char **argv, int *count);

void logical_group_member_construct(orcm_logical_group_member_t *member_item)
{
    member_item->member = NULL;
}

void logical_group_member_destruct(orcm_logical_group_member_t *member_item)
{
    SAFEFREE(member_item->member);
}

OBJ_CLASS_INSTANCE(orcm_logical_group_member_t, opal_list_item_t,
                   logical_group_member_construct, logical_group_member_destruct);

orcm_logical_group_t LOGICAL_GROUP = {NULL, NULL};
char *current_tag = NULL;

file_with_lock_t logical_group_file_lock = {NULL, -1, -1, {F_RDLCK, SEEK_SET, 0, 0, 0}};

static int orcm_logical_group_init(char *config_file)
{
    int erri = ORCM_SUCCESS;

    if (NULL == config_file) {
        if (-1 == (erri = asprintf(&(LOGICAL_GROUP.storage_filename),
                   "%s", orcm_cfgi_base.config_file))) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
    } else if (NULL == (LOGICAL_GROUP.storage_filename = strdup(config_file))) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    if (NULL == (LOGICAL_GROUP.groups = OBJ_NEW(opal_hash_table_t))) {
            return ORCM_ERR_OUT_OF_RESOURCE;
    }

    if (ORCM_SUCCESS != (erri = opal_hash_table_init(LOGICAL_GROUP.groups, HASH_SIZE))) {
        return erri;
    }

    logical_group_file_lock.file_lock.l_pid = getpid();

    return erri;
}

int orcm_logical_group_finalize()
{
    if (NULL != LOGICAL_GROUP.groups) {
        opal_hash_table_remove_all(LOGICAL_GROUP.groups);
        OBJ_RELEASE(LOGICAL_GROUP.groups);
    }

    SAFEFREE(LOGICAL_GROUP.storage_filename);
    SAFEFREE(current_tag);

    return ORCM_SUCCESS;
}

static int orcm_logical_group_hash_table_get(opal_hash_table_t *groups, char *tag,
                                             opal_list_t **o_group_members)
{
    int erri = opal_hash_table_get_value_ptr(groups, tag, strlen(tag) + 1,
                                             (void**)o_group_members);
    if (OPAL_ERR_NOT_FOUND == erri || NULL == *o_group_members) {
        *o_group_members = OBJ_NEW(opal_list_t);
        if (NULL == *o_group_members) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
    }

    return ORCM_SUCCESS;
}

static bool member_exist(opal_list_t *group_members, char *new_member)
{
    bool exist = false;
    orcm_logical_group_member_t *member_item = NULL;

    OPAL_LIST_FOREACH(member_item, group_members, orcm_logical_group_member_t) {
        if (NULL == member_item) {
            break;
        }
        if (0 == strncmp(member_item->member, new_member, strlen(member_item->member) + 1)) {
            exist = true;
            break;
        }
    }

    return exist;
}

static int orcm_logical_group_list_append(char *member, opal_list_t *member_list)
{
    int erri = ORCM_SUCCESS;

    orcm_logical_group_member_t *member_item = OBJ_NEW(orcm_logical_group_member_t);
    if (NULL == member_item) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    if (NULL == (member_item->member = strdup(member))) {
        OBJ_RELEASE(member_item);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    opal_list_append(member_list, &member_item->super);

    return erri;
}

static int orcm_logical_group_add_internal(char *tag, char **new_members,
                                           opal_list_t *group_members,
                                           opal_hash_table_t *io_groups)
{
    int index = -1;
    int erri = ORCM_SUCCESS;
    int count = opal_argv_count(new_members);

    for (index = 0; index < count; index++) {
        if (false == member_exist(group_members, new_members[index])) {
            erri = orcm_logical_group_list_append(new_members[index], group_members);
            if (ORCM_SUCCESS != erri) {
                break;
            }
        }
    }

    if (erri == ORCM_SUCCESS) {
        erri = opal_hash_table_set_value_ptr(io_groups, tag, strlen(tag) + 1, group_members);
    }

    return erri;
}

int orcm_logical_group_add(char *tag, char *regex, opal_hash_table_t *io_groups)
{
    int erri = ORCM_SUCCESS;
    char **new_members = NULL;
    opal_list_t *group_members = NULL;

    if (NULL == io_groups) {
        erri = ORCM_ERR_BAD_PARAM;
        return erri;
    }
    erri = orcm_logical_group_hash_table_get(io_groups, tag, &group_members);
    if (ORCM_SUCCESS != erri) {
        return erri;
    }

    erri = orte_regex_extract_node_names(regex, &new_members);
    if (ORTE_SUCCESS != erri) {
        goto cleanup;
    }

    erri = orcm_logical_group_add_internal(tag, new_members, group_members, io_groups);

cleanup:
    opal_argv_free(new_members);
    if (ORCM_SUCCESS != erri && NULL != group_members) {
        OBJ_RELEASE(group_members);
    }
    return erri;
}

static bool is_do_all_wildcard(char *text)
{
    bool answer = false;
    if (NULL != text) {
        if(0 == strncmp(text, "*", strlen(text) + 1)) {
            answer = true;
        }
    }
    return answer;
}

static int orcm_logical_group_all_members_exist_one_tag(opal_list_t *value, char **members)
{
    int index = -1;
    bool exist = false;
    int count = opal_argv_count(members);

    for (index = 0; index < count; index++) {
        if (false == (exist = member_exist(value, members[index]))) {
            return ORCM_ERR_NODE_NOT_EXIST;
        }
    }

    return ORCM_SUCCESS;
}

static int orcm_logical_group_all_members_exist_all_tags(opal_hash_table_t *groups,
                                                         char **members)
{
    int erri = ORCM_SUCCESS;
    char *key = NULL;
    size_t key_size = 0;
    opal_list_t *value = NULL;
    void *in_member = NULL;
    void *out_member = NULL;

    while (OPAL_SUCCESS == opal_hash_table_get_next_key_ptr(groups, (void**)&key,
                                         &key_size, (void**)&value, in_member, &out_member)) {
            if (ORCM_SUCCESS !=
                (erri = orcm_logical_group_all_members_exist_one_tag(value, members))) {
                return erri;
            }
            in_member = out_member;
            out_member = NULL;
    }

    return erri;
}

static int orcm_logical_group_hash_table_remove_members(char *tag,
                                                        opal_list_t *value,
                                                        char **members,
                                                        opal_hash_table_t *io_groups)
{
    int index = -1;
    orcm_logical_group_member_t *member_item = NULL;
    orcm_logical_group_member_t *next_member_item = NULL;
    int count = opal_argv_count(members);

    for (index = 0; index < count; index++) {
        OPAL_LIST_FOREACH_SAFE(member_item, next_member_item, value, orcm_logical_group_member_t) {
            if (0 == strncmp(members[index], member_item->member, strlen(members[index]) + 1)) {
                opal_list_remove_item(value, &member_item->super);
                OBJ_RELEASE(member_item);
                break;
            }
        }
    }

    if (opal_list_is_empty(value)) {
        return opal_hash_table_remove_value_ptr(io_groups, tag, strlen(tag) + 1);
    }

    return opal_hash_table_set_value_ptr(io_groups, tag, strlen(tag) + 1, value);
}

static int orcm_logical_group_remove_all_tags(char **members, int do_all_member,
                                              opal_hash_table_t *io_groups)
{
    int erri = ORCM_SUCCESS;
    char *key = NULL;
    size_t key_size = 0;
    opal_list_t *value = NULL;
    void *in_member = NULL;
    void *out_member = NULL;

    if (do_all_member) {
        return opal_hash_table_remove_all(io_groups);
    }

    if (ORCM_SUCCESS !=
        (erri = orcm_logical_group_all_members_exist_all_tags(io_groups, members))) {
        return erri;
    }

    while (OPAL_SUCCESS == opal_hash_table_get_next_key_ptr(io_groups, (void**)&key,
                                         &key_size, (void**)&value, in_member, &out_member)) {
            erri = orcm_logical_group_hash_table_remove_members(key, value, members, io_groups);
            if (ORCM_SUCCESS != erri) {
                return erri;
            }
            in_member = out_member;
            out_member = NULL;
    }

    return erri;
}

static int orcm_logical_group_remove_a_tag(char *tag, char **members, int do_all_member,
                                           opal_hash_table_t *io_groups)
{
    int erri = ORCM_SUCCESS;
    opal_list_t *value = NULL;

    if (OPAL_SUCCESS == (erri = opal_hash_table_get_value_ptr(io_groups, tag,
                                                      strlen(tag) + 1, (void**)&value))) {
        if (do_all_member) {
            return opal_hash_table_remove_value_ptr(io_groups, tag, strlen(tag) + 1);
        }
        if (ORCM_SUCCESS != (erri = orcm_logical_group_all_members_exist_one_tag(value, members))) {
            return erri;
        }
        return orcm_logical_group_hash_table_remove_members(tag, value, members, io_groups);
    }

    if (OPAL_ERR_NOT_FOUND == erri) {
        return ORCM_ERR_GROUP_NOT_EXIST;
    }

    return erri;
}

static int
orcm_logical_group_remove_internal(char *tag, int do_all_tag, char **members,
                                   int do_all_member, opal_hash_table_t *io_groups)
{
    if (do_all_tag) {
        return orcm_logical_group_remove_all_tags(members, do_all_member, io_groups);
    }

    return orcm_logical_group_remove_a_tag(tag, members, do_all_member, io_groups);
}

int orcm_logical_group_remove(char *tag, char *regex, opal_hash_table_t *io_groups)
{
    int erri = ORCM_SUCCESS;
    int do_all_tag = 0;
    int do_all_member = 0;
    char **members = NULL;

    if (NULL == io_groups) {
        return ORCM_ERR_BAD_PARAM;
    }

    if (0 == opal_hash_table_get_size(io_groups)) {
        return ORCM_ERR_NO_ANY_GROUP;
    }

    do_all_tag = is_do_all_wildcard(tag);
    if (0 == (do_all_member = is_do_all_wildcard(regex))) {
        erri = orte_regex_extract_node_names(regex, &members);
        if (ORTE_SUCCESS != erri) {
            goto cleanup;
        }
    }

    erri = orcm_logical_group_remove_internal(tag, do_all_tag, members, do_all_member, io_groups);

cleanup:
    opal_argv_free(members);

    return erri;
}

static opal_list_t *
orcm_logical_group_list_specific_members(opal_list_t *value, char **members)
{
    int index = -1;
    orcm_logical_group_member_t *member_item = NULL;
    opal_list_t *new_value = NULL;
    int count = opal_argv_count(members);

    if (0 == count) {
        return NULL;
    }

    new_value = OBJ_NEW(opal_list_t);
    if (NULL != new_value) {
        for (index = 0; index < count; index++) {
            OPAL_LIST_FOREACH(member_item, value, orcm_logical_group_member_t) {
                if (NULL == member_item) {
                    return NULL;
                }
                if (0 == strncmp(members[index], member_item->member, strlen(members[index]) + 1)) {
                    orcm_logical_group_list_append(member_item->member, new_value);
                    break;
                }
            }
        }

        if (opal_list_is_empty(new_value)) {
            OBJ_RELEASE(new_value);
        }
    }

    return new_value;
}

static opal_hash_table_t*
orcm_logical_group_list_all_tags(char **members, int do_all_member, opal_hash_table_t *groups)
{
    opal_hash_table_t *o_groups = NULL;
    char *key = NULL;
    size_t key_size = 0;
    opal_list_t *value = NULL;
    opal_list_t *new_value = NULL;
    void *in_member = NULL;
    void *o_member = NULL;

    if (do_all_member) {
        return groups;
    }

    o_groups = OBJ_NEW(opal_hash_table_t);
    if (NULL != o_groups) {
        opal_hash_table_init(o_groups, HASH_SIZE);
        while (OPAL_SUCCESS == opal_hash_table_get_next_key_ptr(groups, (void**)&key,
                                           &key_size, (void**)&value, in_member, &o_member)) {
            new_value = orcm_logical_group_list_specific_members(value, members);
            if (NULL != new_value) {
                opal_hash_table_set_value_ptr(o_groups, key, key_size, new_value);
            }
            in_member = o_member;
            o_member = NULL;

        }
    }

    return o_groups;
}

static opal_hash_table_t *orcm_logical_group_list_a_tag(char *tag, char **members,
                                                        int do_all_member,
                                                        opal_hash_table_t *groups)
{
    opal_hash_table_t *o_groups = NULL;
    opal_list_t *value = NULL;
    opal_list_t *new_value = NULL;

    if (OPAL_SUCCESS == opal_hash_table_get_value_ptr(groups, tag,
                                                      strlen(tag) + 1, (void**)&value)) {
        o_groups = OBJ_NEW(opal_hash_table_t);
        if (NULL != o_groups) {
            opal_hash_table_init(o_groups, HASH_SIZE);
            if (do_all_member) {
                opal_hash_table_set_value_ptr(o_groups, tag, strlen(tag) + 1, value);
            } else {
                new_value = orcm_logical_group_list_specific_members(value, members);
                if (NULL != new_value) {
                    opal_hash_table_set_value_ptr(o_groups, tag, strlen(tag) + 1, new_value);
                }
            }
        }
    }

    return o_groups;
}

static opal_hash_table_t *
orcm_logical_group_list_internal(char *tag, int do_all_tag, char **members,
                                 int do_all_member, opal_hash_table_t *groups)
{
    if (do_all_tag) {
        return orcm_logical_group_list_all_tags(members, do_all_member, groups);
    }

    return orcm_logical_group_list_a_tag(tag, members, do_all_member, groups);
}

opal_hash_table_t *orcm_logical_group_list(char *tag, char *regex,
                                           opal_hash_table_t *groups)
{
    int do_all_tag = 0;
    int do_all_member = 0;
    char **members = NULL;
    opal_hash_table_t *o_groups = NULL;

    if (NULL == groups) {
        return NULL;
    }

    do_all_tag = is_do_all_wildcard(tag);
    if (0 == (do_all_member = is_do_all_wildcard(regex))) {
        if (ORTE_SUCCESS != orte_regex_extract_node_names(regex, &members)) {
            goto cleanup;
        }
    }

    o_groups = orcm_logical_group_list_internal(tag, do_all_tag,
                                                members, do_all_member, groups);

cleanup:
    opal_argv_free(members);
    return o_groups;
}

static int orcm_logical_group_process_tag_line(char *tag)
{
    if (NULL == current_tag || (0 != strncmp(current_tag, tag, strlen(current_tag) + 1))) {
        SAFEFREE(current_tag);
        current_tag = strdup(tag);
    }

    return ORCM_SUCCESS;
}

static int orcm_logical_group_parsing(opal_list_t *result_list, opal_hash_table_t *groups)
{
    int erri = ORCM_SUCCESS;
    orcm_value_t *list_item = NULL;
    int rc = ORCM_SUCCESS;

    OPAL_LIST_FOREACH(list_item, result_list, orcm_value_t) {

        if (OPAL_STRING == list_item->value.type) {
            if (0 == strcmp("name", list_item->value.key)) {
                erri = orcm_logical_group_process_tag_line(list_item->value.data.string);
            } else if (0 == strcmp("members", list_item->value.key)) {
                erri = orcm_logical_group_add(current_tag, list_item->value.data.string, groups);
            } else {
                ORCM_UTIL_ERROR_MSG_WITH_ARG("Not recognize the current key %s", list_item->value.key);
                erri = ORCM_ERR_BAD_PARAM;
            }

        } else if (list_item->value.type == OPAL_PTR) {
            if (0 == strcmp(list_item->value.key, "group")) {
                rc = orcm_logical_group_parsing((opal_list_t*)list_item->value.data.ptr, groups);
                if (ORCM_SUCCESS != rc) {
                    return rc;
                }
            } else {
                ORCM_UTIL_ERROR_MSG_WITH_ARG("Unexpected key from config file %s", list_item->value.key);
                return ORCM_ERR_BAD_PARAM;
            }
        } else {
            ORCM_UTIL_ERROR_MSG_WITH_ARG("Unexpected data type %c from config file", list_item->value.type);
            return ORCM_ERR_BAD_PARAM;
        }
    }
    return erri;
}

static void orcm_logical_group_trim_string(char *string, char **o_string)
{
    char *in_string_travesal = NULL;

    if (NULL == string || NULL == o_string) {
        return;
    }

    /* trim the beginning */
    while(' ' == *string || '\t' == *string) {
        string++;
    }
    *o_string = string;

    /* trim the end */
    if ('\0' != *string) {
        in_string_travesal = string + strlen(string) - 1;
        while (in_string_travesal > string &&
               (' ' == *in_string_travesal || '\t' == *in_string_travesal)) {
            *in_string_travesal = '\0';
            in_string_travesal--;
        }
    }
}

static int orcm_logical_group_parse_from_file(opal_hash_table_t *io_groups)
{
    opal_list_t *result_list = NULL;
    int rc;
    orcm_value_t *list_item = NULL;

    result_list = orcm_parser.retrieve_section(logical_group_file_lock.parser_fd, "logicalgroup", "");
    if (NULL == result_list) {
        return ORCM_ERR_NOT_FOUND;
    }

    OPAL_LIST_FOREACH(list_item, result_list, orcm_value_t) {

        if (list_item->value.type == OPAL_PTR) {
            if (0 == strcmp(list_item->value.key, "logicalgroup")) {
                rc = orcm_logical_group_parsing((opal_list_t*)list_item->value.data.ptr, io_groups);
                if (ORCM_SUCCESS != rc) {
                    SAFE_RELEASE_NESTED_LIST(result_list);
                    return rc;
                }
            }
        }
        else {
            ORCM_UTIL_ERROR_MSG_WITH_ARG("Unexpected data type %c from config file", list_item->value.type);
            SAFE_RELEASE_NESTED_LIST(result_list);
            return ORCM_ERROR;
        }
    }
    SAFE_RELEASE_NESTED_LIST(result_list);
    return ORCM_SUCCESS;
}

static int orcm_logical_group_open_file(char *storage_filename)
{

    if (NULL == storage_filename || '\0' == storage_filename[0]) {
        ORCM_UTIL_ERROR_MSG("Bad setup for parsing logical groupings.");
        return ORCM_ERR_FILE_OPEN_FAILURE;
    }

    if (0 > (logical_group_file_lock.parser_fd = orcm_parser.open(storage_filename))) {
        return logical_group_file_lock.parser_fd;
    }

    return ORCM_SUCCESS;
}

static int orcm_logical_group_crate_xml_file(char *storage_filename)
{
    FILE *storage_fp = NULL;

    if (NULL == storage_filename || '\0' == storage_filename[0]) {
        ORCM_UTIL_ERROR_MSG("Bad setup for parsing logical groupings.");
        return ORCM_ERR_FILE_OPEN_FAILURE;
    }

    storage_fp = fopen(storage_filename, "a");
    if (NULL == storage_fp) {
        ORCM_UTIL_ERROR_MSG("Failed to create file for logical groupings.");
        return ORCM_ERR_FILE_OPEN_FAILURE;
    }
    else {
        if (0 > fprintf(storage_fp, "<configuration />")) {
            fclose(storage_fp);
            return ORCM_ERR_FILE_WRITE_FAILURE;
        }
    }
    fflush(storage_fp);
    fclose(storage_fp);
    return ORCM_SUCCESS;
}

static int orcm_logical_group_open_file_write_mode(char *storage_filename)
{
    int ret = ORCM_SUCCESS;

    ret = orcm_logical_group_open_file(storage_filename);
    if (ORCM_SUCCESS != ret) {
        ret = orcm_logical_group_crate_xml_file(storage_filename);
        if (ORCM_SUCCESS == ret) {
            ret = orcm_logical_group_open_file(storage_filename);
        }
    }
    return ret;
}

static int orcm_logical_group_open_file_with_logicalgroup_tag(char *storage_filename)
{
    int ret = ORCM_SUCCESS;
    opal_list_t *result_list = NULL;

    ret = orcm_logical_group_open_file(storage_filename);
    if (ORCM_SUCCESS != ret) {
        ret = orcm_logical_group_open_file_write_mode(storage_filename);
        if (ORCM_SUCCESS != ret) {
            return ret;
        }
    }

    result_list = orcm_parser.retrieve_section(logical_group_file_lock.parser_fd, "logicalgroup", "");
    if (NULL == result_list) {
        ret = orcm_parser.close(logical_group_file_lock.parser_fd);
        if (ORCM_SUCCESS == ret) {
            ret = orcm_logical_group_open_file_write_mode(storage_filename);
        }
    }
    SAFE_RELEASE_NESTED_LIST(result_list);
    return ret;
}

static int orcm_logical_group_load_from_file(char *storage_filename,
                                             opal_hash_table_t *io_groups)
{
    int erri = ORCM_SUCCESS;
    int ret = ORCM_SUCCESS;

    if (NULL == io_groups) {
        ORCM_UTIL_ERROR_MSG("Missing logical group.");
        return ORCM_ERR_BAD_PARAM;
    }

    ret = orcm_logical_group_open_file(storage_filename);
    if (ORCM_SUCCESS != ret) {
        return ORCM_SUCCESS;
    }

    erri = orcm_logical_group_parse_from_file(io_groups);
    ret = orcm_parser.close(logical_group_file_lock.parser_fd);

    if (ORCM_SUCCESS == erri) {
        return ret;
    } else if (ORCM_ERR_NOT_FOUND == erri) {
        return ORCM_SUCCESS;
    }
    return erri;
}

static opal_list_t *orcm_logical_group_do_convertion(opal_list_t *members_list,
                                                     char *memberlist,
                                                     unsigned int reserved_size)
{
    orcm_logical_group_member_t *member_item = NULL;
    unsigned int current_size = 0;
    int index = 0;
    int count = opal_list_get_size(members_list);
    opal_list_t *new_members_list = OBJ_NEW(opal_list_t);
    if (NULL == new_members_list) {
        return NULL;
    }

    OPAL_LIST_FOREACH(member_item, members_list, orcm_logical_group_member_t) {
        index++;
        current_size = strlen(memberlist);
        if (reserved_size < current_size + strlen(member_item->member) + 1) {
            if (ORCM_SUCCESS != orcm_logical_group_list_append(memberlist, new_members_list)) {
                goto clean;
            }
            memset(memberlist, '\0', strlen(memberlist));
        }
        if (0 < strlen(memberlist)) {
            strncat(memberlist, ",", sizeof(char));
        }
        strncat(memberlist, member_item->member, strlen(member_item->member));

        if (index == count && 0 < strlen(memberlist)) {
            if (ORCM_SUCCESS != orcm_logical_group_list_append(memberlist, new_members_list)) {
                goto clean;
            }
        }
    }

    return new_members_list;

clean:
    OBJ_RELEASE(new_members_list);
    return NULL;
}

opal_list_t *orcm_logical_group_convert_members_list(opal_list_t *members_list,
                                                     unsigned int max_size)
{
    char *memberlist = NULL;
    opal_list_t *o_members_list = NULL;

    if (NULL == members_list || opal_list_is_empty(members_list) || 0 >= max_size) {
        return NULL;
    }

    memberlist = (char*)calloc(max_size, sizeof(char));
    if (NULL == memberlist) {
        return NULL;
    }

    o_members_list = orcm_logical_group_do_convertion(members_list, memberlist, max_size);
    SAFEFREE(memberlist);
    return o_members_list;
}

static int orcm_logical_group_save_to_file_concat(opal_list_t *parser_input, char *tag, opal_list_t *members_list)
{
    int erri = ORCM_SUCCESS;
    orcm_logical_group_member_t *regex = NULL;
    opal_list_t *new_members_list = orcm_logical_group_convert_members_list(members_list,
                                                MAX_LINE_LENGTH - strlen("member list="));
    orcm_value_t* logicalgroup=NULL;
    orcm_value_t* group_name=NULL;
    orcm_value_t* members=NULL;
    opal_list_t *inner_list = NULL;


    if (NULL == tag || NULL == new_members_list) {
        return erri;
    }

    inner_list = OBJ_NEW(opal_list_t);
    if (NULL == inner_list) {
        erri = ORCM_ERR_FILE_WRITE_FAILURE;
        goto cleanup;
    }

    group_name = orcm_util_load_orcm_value ("name", tag, OPAL_STRING, NULL);
    if (NULL == group_name) {
        erri = ORCM_ERR_FILE_WRITE_FAILURE;
        SAFE_RELEASE(inner_list);
        goto cleanup;
    }
    opal_list_append(inner_list, (opal_list_item_t *)group_name);

    OPAL_LIST_FOREACH(regex, new_members_list, orcm_logical_group_member_t) {
        if (NULL == regex) {
            erri = ORCM_ERR_BAD_PARAM;
            SAFE_RELEASE(inner_list);
            goto cleanup;
        }
        members = orcm_util_load_orcm_value ("members", regex->member, OPAL_STRING, NULL);
        if (NULL == members) {
            erri = ORCM_ERR_FILE_WRITE_FAILURE;
            SAFE_RELEASE(inner_list);
            goto cleanup;
        }
        opal_list_append(inner_list, (opal_list_item_t *)members);
    }

    logicalgroup = orcm_util_load_orcm_value ("group", inner_list, OPAL_PTR, NULL);
    if (NULL == logicalgroup) {
        erri = ORCM_ERR_FILE_WRITE_FAILURE;
        SAFE_RELEASE(inner_list);
        goto cleanup;
    }
    opal_list_append(parser_input, (opal_list_item_t *)logicalgroup);


cleanup:
    if (NULL != new_members_list) {
        OBJ_RELEASE(new_members_list);
    }
    return erri;
}

static int orcm_logical_group_save_to_file_internal(opal_hash_table_t *groups)
{
    int erri = ORCM_SUCCESS;
    char *key = NULL;
    size_t key_size = 0;
    opal_list_t *value = NULL;
    void *in_member = NULL;
    void *out_member = NULL;
    opal_list_t *parser_input = NULL;

    parser_input = OBJ_NEW(opal_list_t);
    if (NULL == parser_input) {
        return ORCM_ERR_FILE_WRITE_FAILURE;
    }

    if (0 == opal_hash_table_get_size(groups)) {
        erri = orcm_parser.write_section(logical_group_file_lock.parser_fd, NULL, "logicalgroup", "", true);
        SAFE_RELEASE(parser_input);
        return erri;
    } else {
        while (ORCM_SUCCESS == opal_hash_table_get_next_key_ptr(groups, (void**)&key,
                                        &key_size, (void**)&value, in_member, &out_member)) {
            erri = orcm_logical_group_save_to_file_concat(parser_input, key, value);
            if (ORCM_SUCCESS != erri) {
                break;
            }
            in_member = out_member;
            out_member = NULL;
        }
    }

    if (ORCM_SUCCESS == erri) {
        erri = orcm_parser.write_section(logical_group_file_lock.parser_fd, parser_input, "logicalgroup", "", true);
    }

    return erri;
}

int orcm_logical_group_save_to_file(char *storage_filename, opal_hash_table_t *groups)
{
    int erri = ORCM_SUCCESS;
    int ret = ORCM_SUCCESS;

    if (NULL == groups) {
        ORCM_UTIL_ERROR_MSG("Missing logical group.");
        return ORCM_ERR_BAD_PARAM;
    }

    ret = orcm_logical_group_open_file_with_logicalgroup_tag(storage_filename);
    if (ORCM_SUCCESS != ret) {
        return ret;
    }

    erri = orcm_logical_group_save_to_file_internal(groups);
    ret = orcm_parser.close(logical_group_file_lock.parser_fd);
    if (ORCM_SUCCESS == erri) {
        return ret;
    }

    return erri;
}

static int orcm_logical_group_tag_to_members_nested(char ***tag_array, char ***o_array_string)
{
    int count = -1;
    int erri = ORCM_SUCCESS;
    orcm_logical_group_member_t *tag_member = NULL;
    char *tag = NULL;
    opal_list_t *value = NULL;
    opal_list_t *nested_value = NULL;

    count = opal_argv_count(*tag_array);
    while (0 < count) {
        tag = (*tag_array)[0];
        if (ORCM_SUCCESS == (opal_hash_table_get_value_ptr(LOGICAL_GROUP.groups,
                             (void*)tag, strlen(tag) + 1, (void**)&value))) {
            if (NULL != value) {
                OPAL_LIST_FOREACH(tag_member, value, orcm_logical_group_member_t) {
                    if (NULL == tag_member) {
                        break;
                    }
                    if (ORCM_SUCCESS == (opal_hash_table_get_value_ptr(LOGICAL_GROUP.groups,
                        (void*)tag_member->member, strlen(tag_member->member) + 1,
                        (void**)&nested_value))) {
                        erri = opal_argv_append_unique_nosize(tag_array,
                                                              tag_member->member, false);
                        if (OPAL_SUCCESS != erri) {
                            return erri;
                        }
                        count++;
                    } else {
                        erri = opal_argv_append_unique_nosize(o_array_string,
                                                              tag_member->member, false);
                        if (OPAL_SUCCESS != erri) {
                            return erri;
                        }
                    }
                }
            }
        }
        if (OPAL_SUCCESS != (erri = opal_argv_delete(&count, tag_array, 0, 1))) {
            return erri;
        }
    }

    return erri;
}

int orcm_logical_group_parse_array_string(char *regex, char ***o_array_string)
{
    int erri = ORCM_SUCCESS;
    int size = 0;
    int index = 0;
    char *o_regex = NULL;
    char **regex_array = NULL;
    char **tag_array = NULL;

    orcm_logical_group_trim_string(regex, &o_regex);
    if (NULL == o_regex || '\0' == *o_regex) {
        return erri;
    }

    if (ORCM_SUCCESS != (erri = (orte_regex_extract_node_names(o_regex, &regex_array)))) {
        goto cleanup;
    }
    size = opal_argv_count(regex_array);
    for (index = 0; index < size; index++) {
        o_regex = regex_array[index];
        if ('$' == *(regex_array[index])) {
            o_regex++;
            erri = opal_argv_append_unique_nosize(&tag_array, o_regex, false);
            if (OPAL_SUCCESS != erri) {
                goto cleanup;
            }
        } else {
            erri = opal_argv_append_unique_nosize(o_array_string, o_regex, false);
            if (OPAL_SUCCESS != erri) {
                goto cleanup;
            }
        }
    }
    erri = orcm_logical_group_tag_to_members_nested(&tag_array, o_array_string);

cleanup:
    opal_argv_free(regex_array);
    opal_argv_free(tag_array);
    if (ORCM_SUCCESS != erri) {
        opal_argv_free(*o_array_string);
    }
    return erri;
}

static unsigned int orcm_logical_group_argv_addup_size(char **argv, int *count)
{
    int index = 0;
    unsigned int size = 0;

    *count = opal_argv_count(argv);
    for (; index < *count; index++) {
        size += (strlen(argv[index]) + 1);
    }

    return size;
}

static int orcm_logical_group_arraystring_to_string(char **array_string, char **string)
{
    unsigned int size = 0;
    int erri = ORCM_SUCCESS;
    int index = 0;
    int count = 0;

    if (0 == (size = orcm_logical_group_argv_addup_size(array_string, &count))) {
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == (*string = (char*)calloc(size, sizeof(char)))) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    for (; index < count; index++) {
        strncat(*string, array_string[index], strlen(array_string[index]));
        if (index != (count - 1)) {
            strncat(*string, ",", sizeof(char));
        }
    }

    return erri;
}

int orcm_logical_group_parse_string(char *regex, char **o_string)
{
    int erri = ORCM_SUCCESS;
    char **o_string_array = NULL;

    erri = orcm_logical_group_parse_array_string(regex, &o_string_array);
    if (ORCM_SUCCESS != erri) {
        return erri;
    }

    erri = orcm_logical_group_arraystring_to_string(o_string_array, o_string);
    opal_argv_free(o_string_array);

    return erri;
}

int orcm_logical_group_load_to_memory(char *config_file)
{
    int erri = ORCM_SUCCESS;
    if (ORCM_SUCCESS != (erri = orcm_logical_group_init(config_file))) {
        return erri;
    }

    return orcm_logical_group_load_from_file(LOGICAL_GROUP.storage_filename,
                                             LOGICAL_GROUP.groups);
}
