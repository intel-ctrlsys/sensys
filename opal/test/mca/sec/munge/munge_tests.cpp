/*
 * Copyright (c) 2017 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "munge_tests.hpp"
#include "opal/mca/sec/munge/sec_munge.c"

void ut_munge_tests::SetUpTestCase()
{
    step = 0;
    initialized = true;
    mock_getgrgid = true;
    my_cred.credential = strdup("OLAKEASEabc");
    return;
}

void ut_munge_tests::TearDownTestCase()
{
    step = 0;
    initialized = false;
    mock_getgrgid = false;
    free(my_cred.credential);
    return;
}

TEST_F(ut_munge_tests, init_finalize_explicit)
{
    mock_munge_encode = true;
    initialized = true;
    my_cred.credential = strdup("OLAKEASEabc");
    int rc =  init();
    EXPECT_EQ(OPAL_SUCCESS, rc);
    finalize();
}

TEST_F(ut_munge_tests, init_finalize_none_action)
{
    mock_munge_encode = true;
    int rc =  init();
    EXPECT_EQ(OPAL_SUCCESS, rc);
    initialized = false;
    finalize();
}

TEST_F(ut_munge_tests, init_munge_failure)
{
    mock_munge_encode = false;
    int rc =  init();
    EXPECT_EQ(OPAL_ERR_SERVER_NOT_AVAIL, rc);
}

TEST_F(ut_munge_tests, get_my_credential_not_init)
{
    int dstorehandle;
    opal_process_name_t *my_id;
    opal_sec_cred_t cred;
    initialized = false;
    int rc = get_my_cred(dstorehandle, my_id, &cred);
    EXPECT_EQ(OPAL_SUCCESS, rc);
}

TEST_F(ut_munge_tests, get_my_credential_not_refresh)
{
    int dstorehandle;
    opal_process_name_t *my_id;
    opal_sec_cred_t cred;
    initialized = true;
    refresh = false;
    int rc = get_my_cred(dstorehandle, my_id, &cred);
    EXPECT_EQ(OPAL_SUCCESS, rc);
}

TEST_F(ut_munge_tests, get_my_credential_successful)
{
    int dstorehandle;
    opal_process_name_t *my_id;
    opal_sec_cred_t cred;
    refresh = true;
    mock_munge_encode = true;
    int rc = get_my_cred(dstorehandle, my_id, &cred);
    EXPECT_EQ(strcmp(cred.method, "munge"), 0);
    EXPECT_EQ(cred.size, 12);
    EXPECT_EQ(OPAL_SUCCESS, rc);
}

TEST_F(ut_munge_tests, get_my_credential_failure)
{
    int dstorehandle;
    opal_process_name_t *my_id;
    opal_sec_cred_t cred;
    refresh = true;
    mock_munge_encode = false;
    int rc = get_my_cred(dstorehandle, my_id, &cred);
    EXPECT_EQ(OPAL_ERR_SERVER_NOT_AVAIL, rc);
}

TEST_F(ut_munge_tests, get_group_authorize)
{
    uid_t uid_remote;
    authorize_grp = NULL;
    int rc = group_authorize(uid_remote);
    EXPECT_EQ(OPAL_ERR_AUTHENTICATION_FAILED, rc);
}

TEST_F(ut_munge_tests, get_group_authorize_mca_auth_missing)
{
    uid_t uid_remote;
    authorize_grp = NULL;
    int rc = group_authorize(uid_remote);
    EXPECT_EQ(OPAL_ERR_AUTHENTICATION_FAILED, rc);
}

TEST_F(ut_munge_tests, get_group_authorize_mca_auth_mistmatch_length)
{
    uid_t uid_remote;
    authorize_grp = strdup("not_orcmuser");
    int rc = group_authorize(uid_remote);
    EXPECT_EQ(OPAL_ERR_AUTHENTICATION_FAILED, rc);
    free(authorize_grp);
}

TEST_F(ut_munge_tests, get_group_authorize_success)
{
    uid_t uid_remote;
    authorize_grp = strdup("orcmuser");
    int rc = group_authorize(uid_remote);
    EXPECT_EQ(OPAL_SUCCESS, rc);
    free(authorize_grp);
}

TEST_F(ut_munge_tests, get_group_authorize_mca_auth_mistmatch_name)
{
    uid_t uid_remote;
    authorize_grp = strdup("frcmuser");
    int rc = group_authorize(uid_remote);
    EXPECT_EQ(OPAL_ERROR, rc);
    free(authorize_grp);
}

TEST_F(ut_munge_tests, get_grouplist_pwd_NULL)
{
    uid_t uid_remote;
    gid_t* groups = NULL;
    int ngroups = 1;
    mock_getpwuid = false;
    int rc = get_grouplist(uid_remote, &groups, &ngroups);
    EXPECT_EQ(OPAL_ERR_AUTHENTICATION_FAILED, rc);
}

TEST_F(ut_munge_tests, get_grouplist_malloc_failure)
{
    uid_t uid_remote;
    gid_t* groups = NULL;
    int ngroups = 1;
    mock_getpwuid = true;
    mock_malloc = true;
    malloc_cntr = 0;
    malloc_step = 2;
    int rc = get_grouplist(uid_remote, &groups, &ngroups);
    EXPECT_EQ(OPAL_ERR_OUT_OF_RESOURCE, rc);
    mock_malloc = false;
}

TEST_F(ut_munge_tests, get_grouplist_user_member_of_multiple_groups)
{
    uid_t uid_remote;
    gid_t* groups = NULL;
    int ngroups = 1;
    mock_getpwuid = true;
    mock_getgrouplist = true;
    step = 0;
    int rc = get_grouplist(uid_remote, &groups, &ngroups);
    EXPECT_EQ(OPAL_ERR_OUT_OF_RESOURCE, rc);
    step = 0;
    mock_getgrouplist = false;
}

TEST_F(ut_munge_tests, get_grouplist_user_member_multiple_groups_resize)
{
    uid_t uid_remote;
    gid_t* groups = NULL;
    int ngroups = 1;
    mock_getpwuid = true;
    mock_getgrouplist = true;
    step = 1;
    int rc = get_grouplist(uid_remote, &groups, &ngroups);
    EXPECT_EQ(OPAL_SUCCESS, rc);
    step = 0;
    mock_getgrouplist = false;
}

TEST_F(ut_munge_tests, get_grouplist_getgrouplist_found)
{
    uid_t uid_remote;
    gid_t* groups = NULL;
    int ngroups = 1;
    mock_getpwuid = true;
    int rc = get_grouplist(uid_remote, &groups, &ngroups);
    step = 0;
    EXPECT_EQ(OPAL_SUCCESS, rc);
}

TEST_F(ut_munge_tests, grouplist_authorize_no_mca_group)
{
    gid_t* groups = NULL;
    int ngroups = 1;
    authorize_grp = NULL;
    int rc = grouplist_authorize(ngroups, groups);
    EXPECT_EQ(OPAL_ERR_AUTHENTICATION_FAILED, rc);
}

TEST_F(ut_munge_tests, grouplist_authorize_no_groups)
{
    gid_t* groups = NULL;
    int ngroups = 0;
    authorize_grp = NULL;
    int rc = grouplist_authorize(ngroups, groups);
    EXPECT_EQ(OPAL_ERR_AUTHENTICATION_FAILED, rc);
}

TEST_F(ut_munge_tests, grouplist_authorize_groups_success)
{
    gid_t* groups = NULL;
    int ngroups = 1;
    authorize_grp = strdup("orcmuser");
    groups = (gid_t*)malloc(sizeof(gid_t) * ngroups);
    int rc = grouplist_authorize(ngroups, groups);
    EXPECT_EQ(OPAL_SUCCESS, rc);
    free(authorize_grp);
}

TEST_F(ut_munge_tests, authenticate_group_authorize_success)
{
    /* authorize */
    opal_sec_cred_t cred;
    cred.credential = strdup("OLAKEASEabc");
    mock_munge_decode = true;
    /* group authorize */
    uid_t uid_remote;
    authorize_grp = strdup("orcmuser");

    mock_getgrouplist = true;
    int rc = authenticate(&cred);
    EXPECT_EQ(OPAL_SUCCESS, rc);
    free(authorize_grp);
}

TEST_F(ut_munge_tests, authenticate_get_grouplist_failure)
{
    // authorize
    opal_sec_cred_t cred;
    cred.credential = strdup("OLAKEASEabc");
    mock_munge_decode = true;
    // group authorize
    uid_t uid_remote;
    authorize_grp = NULL;

    mock_getpwuid = false;
    int rc = authenticate(&cred);
    EXPECT_EQ(OPAL_ERR_AUTHENTICATION_FAILED, rc);
}


TEST_F(ut_munge_tests, authenticate_munge_decode_failure)
{
    opal_sec_cred_t cred;
    cred.credential = strdup("OLAKEASEabc");
    mock_munge_decode = false;
    int rc = authenticate(&cred);
    EXPECT_EQ(OPAL_ERR_AUTHENTICATION_FAILED, rc);
}

TEST_F(ut_munge_tests, authenticate_grouplist_authorize_failure)
{
    /* authorize */
    opal_sec_cred_t cred;
    cred.credential = strdup("OLAKEASEabc");
    mock_munge_decode = true;
    /* group authorize */
    authorize_grp = NULL;
    /* group list */
    mock_getpwuid = true;
    mock_getgrouplist = false;
    /* group list authorize */
    int rc = authenticate(&cred);
    EXPECT_EQ(OPAL_ERR_AUTHENTICATION_FAILED, rc);
}
