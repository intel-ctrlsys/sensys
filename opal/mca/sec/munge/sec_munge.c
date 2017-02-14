/*
 * Copyright (c) 2015 - 2016     Intel Corporation. All rights reserved.
 * Copyright (c) 2015      Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * Copyright (c) 2016 - 2017     Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 */

#include "opal_config.h"
#include "opal/constants.h"

#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <munge.h>

#include "opal_stdint.h"
#include "opal/dss/dss_types.h"
#include "opal/util/error.h"
#include "opal/util/output.h"
#include "opal/util/show_help.h"
#include "opal/mca/sec/base/base.h"
#include "opal/mca/sec/munge/sec_munge.h"

static int init(void);
static void finalize(void);
static int get_my_cred(int dstorehandle,
                       opal_process_name_t *my_id,
                       opal_sec_cred_t *cred);
static int authenticate(opal_sec_cred_t *cred);

opal_sec_base_module_t opal_sec_munge_module = {
    init,
    finalize,
    get_my_cred,
    authenticate
};

static opal_sec_cred_t my_cred;
static bool initialized = false;
static bool refresh = false;

static int init(void)
{
    munge_err_t rc = EMUNGE_SNAFU;

    opal_output_verbose(2, opal_sec_base_framework.framework_output,
                        "sec: munge init");

    /* attempt to get a credential as a way of checking that
     * the munge server is available - cache the credential for later use */
    if (EMUNGE_SUCCESS != (rc = munge_encode(&my_cred.credential, NULL, NULL, 0))) {
        opal_output_verbose(2, opal_sec_base_framework.framework_output,
                            "sec: munge failed to create credential: %s",
                            munge_strerror(rc));
        return OPAL_ERR_SERVER_NOT_AVAIL;
    }
    /* include the '\0' termination string character */
    my_cred.size = strlen(my_cred.credential)+1;
    initialized = true;

    return OPAL_SUCCESS;
}

static void finalize(void)
{
    if (initialized) {
        free(my_cred.credential);
    }
}

static int get_my_cred(int dstorehandle,
                       opal_process_name_t *my_id,
                       opal_sec_cred_t *cred)
{
    munge_err_t rc = EMUNGE_SNAFU;

    if (initialized) {
        if (!refresh) {
            refresh = true;
        } else {
            /* get a new credential as munge will not allow us to reuse them */
            if (EMUNGE_SUCCESS != (rc = munge_encode(&my_cred.credential, NULL, NULL, 0))) {
                opal_output_verbose(2, opal_sec_base_framework.framework_output,
                                    "sec: munge failed to create credential: %s",
                                    munge_strerror(rc));
                return OPAL_ERR_SERVER_NOT_AVAIL;
            }
            /* include the '\0' termination string character */
            my_cred.size = strlen(my_cred.credential)+1;
        }
        cred->method = strdup("munge");
        cred->credential = strdup(my_cred.credential);
        cred->size = my_cred.size;
    }

    return OPAL_SUCCESS;
}

static int group_authorize(uid_t uid_remote)
{
    struct group* grp = NULL;

    grp = getgrgid(uid_remote);
    if (NULL == authorize_grp) {
        opal_output_verbose(100, opal_sec_base_framework.framework_output,
                            "sec: munge: set opal_sec_munge_authorize_group"
                            "MCA parameter not defined\n");
        return OPAL_ERR_AUTHENTICATION_FAILED;
    }
    if (strlen(authorize_grp) != strlen(grp->gr_name)){
        opal_output_verbose(100, opal_sec_base_framework.framework_output,
                            "sec: munge: uid remote group mismatch"
                            "name: %s\n",grp->gr_name);
        return OPAL_ERR_AUTHENTICATION_FAILED;
    }
    if (0 == strncmp(authorize_grp,grp->gr_name,strlen(authorize_grp))) {
        opal_output_verbose(100, opal_sec_base_framework.framework_output,
                            "sec: munge: Authorization passed:received group"
                            "name: %s\n",grp->gr_name);
        return OPAL_SUCCESS;
    }

    return OPAL_ERROR;
}

static int get_grouplist(uid_t uid_remote, gid_t** groups, int* ngroups)
{
    struct passwd* pwd = NULL;

    pwd = getpwuid(uid_remote);
    if (NULL == pwd) {
        opal_output_verbose(100, opal_sec_base_framework.framework_output,
                            "sec: munge: getpwuid error\n");
        return OPAL_ERR_AUTHENTICATION_FAILED;
    }

    *groups = (gid_t*)malloc(*ngroups * sizeof(gid_t));
    if (NULL == *groups) {
        opal_output_verbose(100, opal_sec_base_framework.framework_output,
                            "sec: munge: malloc failed to allocate memory");
        return OPAL_ERR_OUT_OF_RESOURCE;
    }

    if (-1 == getgrouplist(pwd->pw_name, pwd->pw_gid, *groups, ngroups)) {
        opal_output_verbose(100, opal_sec_base_framework.framework_output,
                            "sec: munge: getgrouplist() returned -1 for "
                            " first time; ngroups = %d\n",*ngroups);
        *groups = (gid_t*)realloc(*groups, (*ngroups) * sizeof(gid_t));
        if (-1 == getgrouplist(pwd->pw_name, pwd->pw_gid, *groups, ngroups)) {
            opal_output_verbose(100, opal_sec_base_framework.framework_output,
                                "sec: munge: getgrouplist() returned -1 for "
                                "2nd time; ngroups = %d\n",*ngroups);
            if (NULL != *groups) {
                free(*groups);
            }
            return OPAL_ERR_OUT_OF_RESOURCE;
        }
    }

    return OPAL_SUCCESS;
}

static int grouplist_authorize(int ngroups, gid_t* groups)
{
    int indexgrp;
    struct group* grp = NULL;

    opal_output_verbose(100, opal_sec_base_framework.framework_output,
                        "sec: munge: ngroups = %d\n", ngroups);

    if (NULL == authorize_grp) {
        opal_output_verbose(100, opal_sec_base_framework.framework_output,
                            "sec: munge: set opal_sec_munge_authorize_group"
                            "MCA parameter not defined\n");
        return OPAL_ERR_AUTHENTICATION_FAILED;
    }

    for (indexgrp = 0; indexgrp < ngroups; indexgrp++) {
        grp = getgrgid(groups[indexgrp]);
        if (NULL == grp) {
            continue;
        }
        if(strlen(authorize_grp) != strlen(grp->gr_name)){
            continue;
        }
        if (0 == strncmp(authorize_grp,grp->gr_name,strlen(authorize_grp))) {
            opal_output_verbose(100, opal_sec_base_framework.framework_output,
                                "sec: munge: Authorization passed(2nd PASS):"
                                " received group name:%s\n",grp->gr_name);
            free(groups);
            return OPAL_SUCCESS;
        }
    }
    free(groups);

    return OPAL_ERR_AUTHENTICATION_FAILED;
}

static int authenticate(opal_sec_cred_t *cred)
{
    int rc = OPAL_ERR_AUTHENTICATION_FAILED;
    munge_err_t munge_rc = EMUNGE_SNAFU;
    static uid_t uid_remote = -1;
    static gid_t gid_remote = -1;
    gid_t *groups = NULL;
    int ngroups = 1;

    opal_output_verbose(2, opal_sec_base_framework.framework_output,
                        "sec: munge validate_cred %s", cred->credential);

    /* parse the inbound string */
    if (EMUNGE_SUCCESS != (munge_rc = munge_decode(cred->credential, NULL, NULL, NULL,
                                                   &uid_remote, &gid_remote))) {
        opal_output_verbose(2, opal_sec_base_framework.framework_output,
                            "sec: munge Cred decode failed.Received uid:%d and "
                            "gid: %d\n", uid_remote, gid_remote);
        opal_output_verbose(2, opal_sec_base_framework.framework_output,"sec:"
                            "munge cred decode error: %s",munge_strerror(munge_rc));

        return OPAL_ERR_AUTHENTICATION_FAILED;
    }
    if (OPAL_SUCCESS == (rc = group_authorize(uid_remote))) {
        opal_output_verbose(2, opal_sec_base_framework.framework_output,"sec:"
                            "munge group authorize succeeded\n");
        return rc;
    }
    if (OPAL_SUCCESS != (rc = get_grouplist(uid_remote, &groups, &ngroups))) {
        opal_output_verbose(2, opal_sec_base_framework.framework_output,"sec:"
                            "munge get grouplist failed\n");
        return rc;
    }
    if (OPAL_SUCCESS != (rc = grouplist_authorize(ngroups, groups))) {
        opal_output_verbose(2, opal_sec_base_framework.framework_output,"sec:"
                            "munge grouplist authorize failed\n");
    }

    return rc;
}
