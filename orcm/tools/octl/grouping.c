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

bool get_user_confirmation(void);

bool get_user_confirmation()
{
    char ans;
    printf("The default config file will be overwritten.\n"
          "Are you sure you want to overwrite orcm-site.xml?(Y/N)(y/n)");
    scanf("%c", &ans);
    if('\n' == ans) {
        printf("Logical Group not added to file\n"
               "Please answer Y/y to write to default configuration file or\n"
               "Restart octl with -l <filename> option to write to a different file\n");
        return false;
    }
    while('\n' != getchar())
        ;
    if('y' == tolower(ans)){
        return true;
    }
    else {
        printf("Logical Group not added to file\n"
               "Restart octl with -l <filename> option to write to a different file\n");
    }
    return false;
}

int orcm_octl_logical_group_add(int argc, char **argv)
{
    int erri = ORCM_SUCCESS;
    char *tag = NULL;
    char *regex = NULL;
    bool save = true;

    if (4 != argc) {
        orcm_octl_usage("grouping-add", INVALID_USG);
        return ORCM_ERR_BAD_PARAM;
    }
    tag = argv[2];
    regex = argv[3];
    if (0 == strncmp(tag, "*", strlen(tag) + 1) ||
        0 == strncmp(regex, "*", strlen(regex) + 1)) {
        orcm_octl_error("add-wildcard");
        return ORCM_ERR_BAD_PARAM;
    }
    if(NULL == getenv("ORCM_MCA_logical_group_config_file")){
        save = get_user_confirmation();
    }
    if(save){
        if (ORCM_SUCCESS != (erri = orcm_logical_group_add(tag, regex, LOGICAL_GROUP.groups))) {
            return erri;
        }

        if (ORCM_SUCCESS != (erri = orcm_logical_group_save_to_file(LOGICAL_GROUP.storage_filename,
                                                                    LOGICAL_GROUP.groups))) {
            return erri;
        }

        orcm_octl_info("grouping-success", "Add");
    }
    return erri;
}

int orcm_octl_logical_group_remove(int argc, char **argv)
{
    int erri = ORCM_SUCCESS;
    char *tag = NULL;
    char *regex = NULL;
    char *err_str = NULL;

    if (4 != argc) {
        orcm_octl_usage("grouping-remove", INVALID_USG);
        return ORCM_ERR_BAD_PARAM;
    }
    tag = argv[2];
    regex = argv[3];

    if (ORCM_SUCCESS != (erri = orcm_logical_group_remove(tag, regex, LOGICAL_GROUP.groups))) {
        if (ORCM_ERR_NO_ANY_GROUP == erri ||
            ORCM_ERR_GROUP_NOT_EXIST == erri || ORCM_ERR_NODE_NOT_EXIST == erri) {
            orcm_err2str(erri, (const char**)(&err_str));
            orcm_octl_error("grouping-remove", err_str);
            erri = ORCM_ERR_BAD_PARAM;
        }
        return erri;
    }

    if (ORCM_SUCCESS != (erri = orcm_logical_group_save_to_file(LOGICAL_GROUP.storage_filename,
                                                                LOGICAL_GROUP.groups))) {
        return erri;
    }

    orcm_octl_info("grouping-success", "Remove");

    return erri;
}

static int orcm_octl_logical_group_print_list(opal_hash_table_t *groups)
{
    char *key = NULL;
    size_t key_size = 0;
    opal_list_t *value = NULL;
    opal_list_t *new_value = NULL;
    void *in_member = NULL;
    void *o_member = NULL;
    orcm_logical_group_member_t *member_item = NULL;

    while (ORCM_SUCCESS == opal_hash_table_get_next_key_ptr(groups, (void**)&key,
                                         &key_size, (void**)&value, in_member, &o_member)) {
        new_value = orcm_logical_group_convert_members_list(value, MAX_LINE_LENGTH);
        if (NULL != new_value && !opal_list_is_empty(new_value)) {
            orcm_octl_info("group-name", key);
            OPAL_LIST_FOREACH(member_item, new_value, orcm_logical_group_member_t) {
                orcm_octl_info("member-list", member_item->member);
            }
        }
        if (NULL != new_value) {
            OBJ_RELEASE(new_value);
        }
        in_member = o_member;
        o_member = NULL;
    }

    return ORCM_SUCCESS;
}

int orcm_octl_logical_group_list(int argc, char **argv)
{
    int erri = ORCM_SUCCESS;
    opal_hash_table_t *o_groups = NULL;
    char *tag = NULL;
    char *regex = NULL;

    if (4 != argc) {
        orcm_octl_usage("grouping-list", INVALID_USG);
        return ORCM_ERR_BAD_PARAM;
    }
    tag = argv[2];
    regex = argv[3];

    o_groups = orcm_logical_group_list(tag, regex, LOGICAL_GROUP.groups);
    if (NULL == o_groups || 0 == opal_hash_table_get_size(o_groups)) {
        orcm_octl_info("no-record");
        goto cleanup;
    }

    erri = orcm_octl_logical_group_print_list(o_groups);

cleanup:
    if (o_groups != LOGICAL_GROUP.groups && NULL != o_groups) {
        opal_hash_table_remove_all(o_groups);
        OBJ_RELEASE(o_groups);
    }
    return erri;
}
