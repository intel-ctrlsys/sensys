/*
 * Copyright (c) 2017 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MOCK_MUNGE_FACTORY_TESTS_H
#define MOCK_MUNGE_FACTORY_TESTS_H

#include "gtest/gtest.h"

bool mock_malloc;
bool mock_getgrgid;
bool mock_getpwuid;
bool mock_getgrouplist;
bool mock_munge_encode;
bool mock_munge_decode;

int malloc_cntr;
int malloc_step;
int step;

extern "C" {
    #include <pwd.h>
    #include <grp.h>
    #include <munge.h>
    #include <sys/types.h>
    #include <stdlib.h>
    #include <grp.h>
    #include "opal/mca/sec/sec.h"

    extern void* __real_malloc(size_t size);
    extern struct group* __real_getgrgid(gid_t gid);
    extern struct passwd* __real_getpwuid(uid_t uid);

    extern int __real_getgrouplist(const char* user, gid_t group,
                                   gid_t *groups, int *ngroups);
    extern int __real_munge_encode(char **cred, munge_ctx_t ctx,
                                   const void *buf, int len);
    extern int __real_munge_decode(const char *cred, munge_ctx_t ctx,
                                   void **buf, int *len, uid_t *uid, gid_t *gid);

    void* __wrap_malloc(size_t size)
    {
      ++malloc_cntr;
      if (mock_malloc && malloc_cntr == malloc_step) {
          return NULL;
      }
      return __real_malloc(size);
    }

    int __wrap_munge_encode(char **cred, munge_ctx_t ctx, const void *buf, int len)
    {
        if (mock_munge_encode) {
            return EMUNGE_SUCCESS;
        }
        return EMUNGE_SNAFU;
    }

    int __wrap_munge_decode(const char *cred, munge_ctx_t ctx,
                            void **buf, int *len, uid_t *uid, gid_t *gid)
    {
        if (mock_munge_decode) {
            return EMUNGE_SUCCESS;
        }
        return EMUNGE_SNAFU;
    }

    struct group* __wrap_getgrgid(gid_t gid)
    {
        if (mock_getgrgid) {
            struct group *gid = (struct group*)malloc(sizeof(struct group));
            gid->gr_name = strdup("orcmuser");
            gid->gr_passwd = strdup("123456");
            return gid;
        }
        return __real_getgrgid(gid);
    }

    struct passwd* __wrap_getpwuid(uid_t uid)
    {
        if (mock_getpwuid) {
            struct passwd *uid = (struct passwd*)malloc(sizeof(struct passwd));
            return uid;
        }
        return NULL;
    }

    int __wrap_getgrouplist(const char* user, gid_t group,
                            gid_t* groups, int* ngroups)
    {
      ++step;
      if (mock_getgrouplist && 3 != step)
          return -1;
      return 0;
    }
};

#endif /*MOCK_MUNGE_FACTORY_TESTS_H*/
