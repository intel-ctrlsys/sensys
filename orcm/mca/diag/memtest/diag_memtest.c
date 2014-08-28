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

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/db/db.h"
#include "orcm/mca/sensor/base/sensor_private.h"

#include "orcm/mca/diag/base/base.h"
#include "diag_memtest.h"

#define	NPAGE_SIZE	(  4UL << 10)

static int init(void);
static void finalize(void);
static int diag_read(opal_list_t *config);
static int diag_check(opal_list_t *config);

static int mem_diag_ret = ORCM_SUCCESS;

orcm_diag_base_module_t orcm_diag_memtest_module = {
    init,
    finalize,
    NULL,
    diag_read,
    diag_check
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
            mem_diag_ret = ORCM_ERR_COMPARE_FAILURE;
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

static int diag_read(opal_list_t *config)
{
    return ORCM_SUCCESS;
}

static int diag_check(opal_list_t *config)
{
    struct sysinfo info;
    struct rlimit org_limit, new_limit;
    void *addr;
    char *p;
    size_t size, rest;
  
    sysinfo(&info);

    OPAL_OUTPUT_VERBOSE((5, orcm_diag_base_framework.framework_output,
                         "%s diag:memtest:check",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    /* use maximum free memory  */
    size = info.freeram - (128UL << 20);

    /* Save ulimit for virtual address space */
    if ( 0 != getrlimit(RLIMIT_AS, &org_limit) ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                        "%s memdiag: get resource limit failed",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        mem_diag_ret = ORCM_ERR_MEM_LIMIT_EXCEEDED;
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                        "%s Checking memory:                        [NOTRUN]",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
        return mem_diag_ret;
    }

    /* Increase ulimit for virtual address space and try mapping */
    new_limit.rlim_cur = RLIM_INFINITY;
    new_limit.rlim_max = RLIM_INFINITY;
    if ( 0 != setrlimit(RLIMIT_AS, &new_limit) ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                        "%s memdiag: set resource limit failed",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        mem_diag_ret = ORCM_ERR_BAD_PARAM;
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                        "%s Checking memory:                        [NOTRUN]",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
        return mem_diag_ret;
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
            mem_diag_ret = ORCM_ERR_FAILED_TO_MAP;
            opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                                "%s Checking memory:                        [NOTRUN]",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
            return mem_diag_ret;
        }

    } while (addr == (void *)-1);
    
    opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                        "%s memdiag: %ld MB is used for test",
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
        mem_diag_ret = ORCM_ERR_BAD_PARAM;
    }

    if ( ORCM_SUCCESS == mem_diag_ret ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s Checking memory:                        [  OK  ]",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    } else {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s Checking memory:                        [ FAIL ]",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    }

    return mem_diag_ret;
}


