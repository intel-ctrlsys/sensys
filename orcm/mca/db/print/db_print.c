/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <time.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "opal/class/opal_pointer_array.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal_stdint.h"

#include "orcm/mca/db/base/base.h"
#include "db_print.h"

static int init(struct orcm_db_base_module_t *imod);
static void finalize(struct orcm_db_base_module_t *imod);
static int store(struct orcm_db_base_module_t *imod,
                 const char *primary_key,
                 opal_list_t *kvs);

mca_db_print_module_t mca_db_print_module = {
    {
        init,
        finalize,
        store,
        NULL,
        NULL,
        NULL
    },
};

static int init(struct orcm_db_base_module_t *imod)
{
    mca_db_print_module_t *mod = (mca_db_print_module_t*)imod;

    if (0 == strcmp(mod->file, "-")) {
        mod->fp = stdout;
    } else if (0 == strcmp(mod->file, "+")) {
        mod->fp = stderr;
    } else if (NULL == (mod->fp = fopen(mod->file, "w"))) {
        opal_output(0, "ERROR: cannot open log file %s", mod->file);
        return ORCM_ERROR;
    }

    return ORCM_SUCCESS;
}

static void finalize(struct orcm_db_base_module_t *imod)
{
    mca_db_print_module_t *mod = (mca_db_print_module_t*)imod;

    if (NULL != mod->fp &&
        stdout != mod->fp &&
        stderr != mod->fp) {
        fclose(mod->fp);
    }
}

static int store(struct orcm_db_base_module_t *imod,
                 const char *primary_key,
                 opal_list_t *kvs)
{
    mca_db_print_module_t *mod = (mca_db_print_module_t*)imod;
    char **cmdargs=NULL, *vstr;
    time_t nowtime;
    struct tm nowtm;
    char tbuf[1024];
    opal_value_t *kv;
    char **key_argv;
    int argv_count;
    int len;

    /* cycle through the provided values and print them */
    /* print the data in the following format: <key>=<value>:<units> */
    OPAL_LIST_FOREACH(kv, kvs, opal_value_t) {
        /* the key could include the units (<key>:<units>) */
        key_argv = opal_argv_split(kv->key, ':');
        argv_count = opal_argv_count(key_argv);
        if (0 != argv_count) {
            snprintf(tbuf, sizeof(tbuf), "%s=", key_argv[0]);
            len = strlen(tbuf);
        } else {
            /* no key :o */
            len = 0;
        }

        switch (kv->type) {
        case OPAL_STRING:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%s",
                     kv->data.string);
            break;
        case OPAL_SIZE:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%" PRIsize_t "",
                     kv->data.size);
            break;
        case OPAL_INT:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%d",
                     kv->data.integer);
            break;
        case OPAL_INT8:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%" PRIi8 "",
                     kv->data.int8);
            break;
        case OPAL_INT16:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%" PRIi16 "",
                     kv->data.int16);
            break;
        case OPAL_INT32:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%" PRIi32 "",
                     kv->data.int32);
            break;
        case OPAL_INT64:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%" PRIi64 "",
                     kv->data.int64);
            break;
        case OPAL_UINT:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%u",
                     kv->data.uint);
            break;
        case OPAL_UINT8:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%" PRIu8 "",
                     kv->data.uint8);
            break;
        case OPAL_UINT16:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%" PRIu16 "",
                     kv->data.uint16);
            break;
        case OPAL_UINT32:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%" PRIu32 "",
                     kv->data.uint32);
            break;
        case OPAL_UINT64:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%" PRIu64 "",
                     kv->data.uint64);
            break;
        case OPAL_PID:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%lu",
                     (unsigned long)kv->data.pid);
            break;
        case OPAL_FLOAT:
            snprintf(tbuf + len, sizeof(tbuf) - len, "%f",
                     kv->data.fval);
            break;
        case OPAL_TIMEVAL:
            /* we only care about seconds */
            nowtime = kv->data.tv.tv_sec;
            (void)localtime_r(&nowtime, &nowtm);
            strftime(tbuf + len, sizeof(tbuf) - len,
                     "%Y-%m-%d %H:%M:%S", &nowtm);
            break;
        default:
            snprintf(tbuf + len, sizeof(tbuf) - len,
                     "Unsupported type: %s",
                     opal_dss.lookup_data_type(kv->type));
            break;
        }

        if (argv_count > 1) {
            /* units were included, so add them to the buffer */
            len = strlen(tbuf);
            snprintf(tbuf + len, sizeof(tbuf) - len, ":%s", key_argv[1]);
        }

        opal_argv_append_nosize(&cmdargs, tbuf);

        opal_argv_free(key_argv);
    }

    /* assemble the value string */
    vstr = opal_argv_join(cmdargs, ',');

    /* print it */
    fprintf(mod->fp, "Store request: %s; values:\n%s\n",
            primary_key, vstr);
    free(vstr);
    opal_argv_free(cmdargs);

    return ORCM_SUCCESS;
}
