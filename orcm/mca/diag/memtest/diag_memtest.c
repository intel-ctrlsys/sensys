/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */
#include <ctype.h>

#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <sys/resource.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "opal/class/opal_list.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal/util/os_dirpath.h"
#include "opal/util/os_path.h"
#include "opal/mca/installdirs/installdirs.h"
#include "opal/mca/hwloc/hwloc.h"

#include "orte/types.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/db/db.h"
#include "orcm/mca/sensor/base/sensor_private.h"

#include "orcm/mca/diag/base/base.h"
#include "diag_memtest.h"

#include "orcm/util/utils.h"

#define	NPAGE_SIZE	(  4UL << 10)

static int init(void);
static void finalize(void);
static int memtest_log(opal_buffer_t *buf);
static void memtest_run(int sd, short args, void *cbdata);

/* prepare input data for the data structure */
static opal_list_t* memtest_prepare_db_input(struct timeval start_time,
                                             struct timeval end_time,
                                             char *nodename, char *diag_type,
                                             char *diag_subtype, char *diag_result);

static void orcm_diag_memtest_db_cleanup(int db_handle, int status, opal_list_t *list,
                                         opal_list_t *ret, void *cbdata);

static int mem_diag_ret = ORCM_SUCCESS;

orcm_diag_base_module_t orcm_diag_memtest_module = {
    init,
    finalize,
    NULL,
    memtest_log,
    memtest_run
};

static struct { unsigned int *addr; size_t size; long num; unsigned long length; } memcheck_t;

static int init(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_diag_base_framework.framework_output,
                         "%s diag:memtest:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    return ORCM_SUCCESS;

}

static void finalize(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_diag_base_framework.framework_output,
                         "%s diag:memtest:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    return;
}

static void *memcheck_thread(void *arg) {

    long pos = (long)arg;
    unsigned int *myaddr;
    size_t mysize, i;
    unsigned int x, y, z, w, t;

    myaddr = memcheck_t.addr + pos * memcheck_t.length;
    mysize = memcheck_t.size - pos * memcheck_t.length;
    if (mysize > memcheck_t.length) mysize = memcheck_t.length;

    /* Xorshift random number generator. */

    x = 123456789;
    y = 362436069;
    z = 521288629;
    w =  88675123;

    for (i = 0; i < mysize; i++) {

        t = (x ^ (x << 11));
        x = y;
        y = z;
        z = w;
        w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
    
        myaddr[i] = w;
    }

    x = 123456789;
    y = 362436069;
    z = 521288629;
    w =  88675123;

    for (i = 0; i < mysize; i++) {

        t = (x ^ (x << 11));
        x = y;
        y = z;
        z = w;
        w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
    
        if (w != myaddr[i]) {
            opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s memdiag: memory error on %p : it should be 0x%08x, but 0x%08x; diffs : 0x%08x",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), (void *)(myaddr + i), w, myaddr[i], w ^ myaddr[i]);
            mem_diag_ret |= DIAG_MEM_STRESS_TST;
        }
    }

    return NULL;
}

static void memcheck(unsigned int *addr, size_t size) {

    long i, numprocs;
    pthread_t *threads;

    numprocs = get_nprocs();

    threads = (pthread_t *)malloc(numprocs * sizeof(pthread_t));

    if (threads) {

        memcheck_t.addr   = addr;
        memcheck_t.size   = size;
        memcheck_t.num    = numprocs;
        memcheck_t.length = (size + numprocs - 1)/ numprocs;
        memcheck_t.length = (memcheck_t.length + 15) & ~15;

        for (i = 1; i < numprocs; i++) 
            pthread_create(&threads[i], NULL, memcheck_thread, (void *)i);

        memcheck_thread((void *)0);

        for (i = 1; i < numprocs; i++) 
            pthread_join(threads[i], NULL);

        free(threads);

    }

}

static opal_list_t* memtest_prepare_db_input(struct timeval start_time,
                                             struct timeval end_time,
                                             char *nodename, char *diag_type,
                                             char *diag_subtype, char *diag_result)
{
    opal_list_t *db_input = NULL;
    opal_value_t *kv = NULL;

    db_input = OBJ_NEW(opal_list_t);
    if (NULL != db_input) {
        /* load the start time */
        kv = orcm_util_load_opal_value("start_time", &start_time, OPAL_TIMEVAL);
        if (NULL == kv) {
            goto memtestcleanup;
        }
        opal_list_append(db_input, &(kv->super));

        /* load the end time */
        kv = orcm_util_load_opal_value("end_time", &end_time, OPAL_TIMEVAL);
        if (NULL == kv) {
            goto memtestcleanup;
        }
        opal_list_append(db_input, &(kv->super));

        /* load the hostname */
        kv = orcm_util_load_opal_value("hostname", nodename, OPAL_STRING);
        if (NULL == kv) {
            goto memtestcleanup;
        }
        opal_list_append(db_input, &(kv->super));

        /* load the diag type */
        kv = orcm_util_load_opal_value("diag_type", diag_type, OPAL_STRING);
        if (NULL == kv) {
            goto memtestcleanup;
        }
        opal_list_append(db_input, &(kv->super));

        /* load the diag subtype */
        kv = orcm_util_load_opal_value("diag_subtype", diag_subtype, OPAL_STRING);
        if (NULL == kv) {
            goto memtestcleanup;
        }
        opal_list_append(db_input, &(kv->super));

        /* load the diag result */
        kv = orcm_util_load_opal_value("test_result", diag_result, OPAL_STRING);
        if (NULL == kv) {
            goto memtestcleanup;
        }
        opal_list_append(db_input, &(kv->super));
    }

    return db_input;

memtestcleanup:
    OPAL_LIST_RELEASE(db_input);
    return NULL;
}

static void orcm_diag_memtest_db_cleanup(int db_handle, int status, opal_list_t *list,
                                         opal_list_t *ret, void *cbdata)
{
    OPAL_LIST_RELEASE(list);

    if (ORTE_SUCCESS != status) {
        ORTE_ERROR_LOG(status);
    }
}

static int memtest_log(opal_buffer_t *buf)
{
    int cnt, rc;
    struct timeval start_time;
    struct timeval end_time;
    char *nodename;
    char *diag_type;
    char *diag_subtype;
    char *diag_result;

    /* Unpack start Time */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &start_time,
                                              &cnt, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* Unpack end time */  
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &end_time,
                                              &cnt, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* Unpack the node name */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &nodename,
                                              &cnt, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* Unpack the diag type */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &diag_type,
                                              &cnt, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* Unpack the diag subtype */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &diag_subtype,
                                              &cnt, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* Unpack our results */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &diag_result,
                                              &cnt, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    opal_list_t *db_input = memtest_prepare_db_input(start_time, end_time, nodename,
                                                     diag_type, diag_subtype, diag_result);
    if (NULL == db_input) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    /* send diag test result to db */
    if (0 <= orcm_diag_base.dbhandle) {
        orcm_db.store_new(orcm_diag_base.dbhandle, ORCM_DB_DIAG_DATA, db_input,
                          NULL, orcm_diag_memtest_db_cleanup, NULL);
        return ORCM_SUCCESS;
    }

    OPAL_LIST_RELEASE(db_input);
    return ORCM_ERR_NO_CONNECTION_ALLOWED;
}

static void memtest_run(int sd, short args, void *cbdata)
{
    orcm_diag_caddy_t *caddy = (orcm_diag_caddy_t*)cbdata;
    struct sysinfo info;
    struct rlimit org_limit, new_limit;
    void *addr;
    char *p;
    size_t size, rest;
    orcm_diag_cmd_flag_t command = ORCM_DIAG_AGG_COMMAND;
    opal_buffer_t *data = NULL;
    struct timeval now;
    struct timeval start_time;
    char *compname;
    char *diag_type;
    char *diag_subtype;
    char *diag_result; 
    orte_process_name_t *tgt;
    int rc;

    if (ORTE_PROC_IS_CM) {
        /* we send to our daemon */
        tgt = ORTE_PROC_MY_DAEMON;
    } else {
        tgt = ORTE_PROC_MY_HNP;
    }
    /* if my target hasn't been defined yet, ignore - nobody listening yet */
    if (ORTE_JOBID_INVALID ==tgt->jobid ||
        ORTE_VPID_INVALID == tgt->vpid) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:memtest: HNP is not defined",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        /* still run diags? */
        /* return; */
    }

    gettimeofday(&start_time, NULL);

    sysinfo(&info);

    OPAL_OUTPUT_VERBOSE((5, orcm_diag_base_framework.framework_output,
                         "%s diag:memtest:run",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    /* use maximum free memory  */
    size = info.freeram - (128UL << 20);

    /* Save ulimit for virtual address space */
    if ( 0 != getrlimit(RLIMIT_AS, &org_limit) ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                        "%s memdiag: get resource limit failed",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        rc = ORCM_ERR_MEM_LIMIT_EXCEEDED;
        ORTE_ERROR_LOG(rc);
        mem_diag_ret |= DIAG_MEM_NOTRUN; 
        opal_output(0, "%s Diagnostic checking memory:\t\t\t[NOTRUN]\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
        goto sendresults;
    }

    /* Increase ulimit for virtual address space and try mapping */
    new_limit.rlim_cur = RLIM_INFINITY;
    new_limit.rlim_max = RLIM_INFINITY;
    if ( 0 != setrlimit(RLIMIT_AS, &new_limit) ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                        "%s memdiag: set resource limit failed",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        rc = ORCM_ERR_BAD_PARAM;
        ORTE_ERROR_LOG(rc);
        mem_diag_ret |= DIAG_MEM_NOTRUN; 
        opal_output(0, "%s Diagnostic checking memory:\t\t\t[NOTRUN]\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
        goto sendresults;
    }

    do {
        /* Mapping */
        addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
        if (addr == (void *)-1) {
            size -= NPAGE_SIZE;
        }
      
        if (size == 0) {
            opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s memdiag: out of memory",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            rc = ORCM_ERR_FAILED_TO_MAP;
            ORTE_ERROR_LOG(rc);
            mem_diag_ret |= DIAG_MEM_NOTRUN; 
            opal_output(0, "%s Diagnostic checking memory:\t\t\t[NOTRUN]\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
            goto sendresults;
        }

    } while (addr == (void *)-1);

    opal_output(0, "%s Diagnostic checking memory: %ld MB is used for test\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), info.freeram >> 20);
    p    = (char *)addr;
    rest = size;

    while ((long)rest > 0) {
        /* do page fault */
        *(double *)p = 0.;
        p    += NPAGE_SIZE;
        rest -= NPAGE_SIZE;
    }

    mem_diag_ret = ORCM_SUCCESS;
    memcheck((unsigned int *)addr, size / sizeof(int));

    munmap(addr, size);
    /* Restore original ulimit for virtual address space */
    if ( 0 != setrlimit(RLIMIT_AS, &org_limit) ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                        "%s memdiag: set resource limit failed",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        rc = ORCM_ERR_BAD_PARAM;
        ORTE_ERROR_LOG(rc);
    }

    if ( ORCM_SUCCESS != mem_diag_ret ) {
        opal_output(0, "%s Diagnostic checking memory:\t\t\t[ FAIL ]\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    } else {
        opal_output(0, "%s Diagnostic checking memory:\t\t\t[ PASS ]\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    }

sendresults:
    gettimeofday(&now, NULL);
    data = OBJ_NEW(opal_buffer_t);

    /* pack aggregator command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &command, 1, ORCM_DIAG_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* pack our component name */
    compname = strdup("memtest");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &compname, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }
    free(compname);

    /* Pack start Time */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &start_time, 1, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* Pack the Time */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &now, 1, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* Pack our node name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* Pack diag type */
    diag_type = strdup("memory");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &diag_type, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }
    free(diag_type);

    /* Pack diag subtype */
    diag_subtype = strdup("stress test");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &diag_subtype, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }
    free(diag_subtype);

    /* Pack our results */
    if ( mem_diag_ret & DIAG_MEM_NOTRUN ) {
        diag_result = strdup("NOTRUN");
    } else if ( ORCM_SUCCESS != mem_diag_ret ) {
        diag_result = strdup("FAIL");
    } else {
        diag_result = strdup("PASS");
    }

    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &diag_result, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }
    free(diag_result);


    /* send results to aggregator/sender */
    if (ORCM_SUCCESS != (rc = orte_rml.send_buffer_nb(tgt, data,
                                                      ORCM_RML_TAG_DIAG,
                                                      orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(data);
    }

    OBJ_RELEASE(caddy);
    return;
}
