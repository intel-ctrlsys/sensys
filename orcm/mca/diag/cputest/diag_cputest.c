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
#ifdef HAVE_SYS_SYSINFO_H
#include <sys/sysinfo.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif  /* HAVE_SYS_WAIT_H */
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

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
#include "diag_cputest.h"

static int init(void);
static void finalize(void);
static int cputest_log(opal_buffer_t *buf);
static void cputest_run(int sd, short args, void *cbdata);


orcm_diag_base_module_t orcm_diag_cputest_module = {
    init,
    finalize,
    NULL,
    cputest_log,
    cputest_run
};

/*
 * Stress Test Timeout
 */
#define CPU_STRESS_TIMEOUT    5
#define BACKOFF_TIME          3000


static int keep_cpu_busy(void);
static int cpu_stress_test(int cpucnt, int timeout);
static int cpu_fp_test(void);
static int cpu_prime_gen_test(void);

static int init(void)
{
    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                         "%s diag:cputest:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    return ORCM_SUCCESS;

}

static void finalize(void)
{
    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                         "%s diag:cputest:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    return;
}

static void mycleanup(int dbhandle, int status, opal_list_t *kvs,
                      opal_list_t *ret, void *cbdata)
{
    OPAL_LIST_RELEASE(kvs);
    if (ORCM_SUCCESS != status){
        ORTE_ERROR_LOG(status);
    }
}


static int cputest_log(opal_buffer_t *buf)
{
    int cnt, rc;
    char *nodename;
    int diag_result;
    opal_list_t *vals[3];
    opal_value_t *kv;
    struct timeval start_time;
    struct timeval end_time;
    int i;

    /* Unpack start Time */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &start_time, &cnt, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* Unpack end time */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &end_time, &cnt, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* Unpack the node name */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &nodename, &cnt, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* Unpack our results */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &diag_result, &cnt, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    /* add results */

    vals[0] = OBJ_NEW(opal_list_t);
    vals[1] = OBJ_NEW(opal_list_t);
    vals[2] = OBJ_NEW(opal_list_t);

    for(i = 0; i < 3; i++){
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("hostname");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(nodename);
        opal_list_append(vals[i], &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("start_time");
        kv->type = OPAL_TIMEVAL;
        kv->data.tv = start_time;
        opal_list_append(vals[i], &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("end_time");
        kv->type = OPAL_TIMEVAL;
        kv->data.tv = end_time;
        opal_list_append(vals[i], &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("diag_type");
        kv->type = OPAL_STRING;
        kv->data.string = strdup("cpu");
        opal_list_append(vals[i], &kv->super);
    }

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("diag_subtype");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("floating point");
    opal_list_append(vals[0], &kv->super);

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("diag_subtype");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("prime number");
    opal_list_append(vals[1], &kv->super);

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("diag_subtype");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("stress test");
    opal_list_append(vals[2], &kv->super);

    /* send diag test result to db */
    if (0 <= orcm_diag_base.dbhandle) {
        if ( DIAG_CPU_FP_TST & diag_result ) {
            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("test_result");
            kv->type = OPAL_STRING;
            kv->data.string = strdup("FAIL");
            opal_list_append(vals[0], &kv->super);

        } else {
            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("test_result");
            kv->type = OPAL_STRING;
            kv->data.string = strdup("PASS");
            opal_list_append(vals[0], &kv->super);
        }
        orcm_db.store_new(orcm_diag_base.dbhandle, ORCM_DB_DIAG_DATA, vals[0], NULL, mycleanup, NULL);

        if ( DIAG_CPU_PRIME_TST & diag_result ) {
            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("test_result");
            kv->type = OPAL_STRING;
            kv->data.string = strdup("FAIL");
            opal_list_append(vals[1], &kv->super);
        } else {
            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("test_result");
            kv->type = OPAL_STRING;
            kv->data.string = strdup("PASS");
            opal_list_append(vals[1], &kv->super);
        }
        orcm_db.store_new(orcm_diag_base.dbhandle, ORCM_DB_DIAG_DATA, vals[1], NULL, mycleanup, NULL);

        if ( DIAG_CPU_STRESS_TST & diag_result ) {
            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("test_result");
            kv->type = OPAL_STRING;
            kv->data.string = strdup("FAIL");
            opal_list_append(vals[2], &kv->super);
        } else {
            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("test_result");
            kv->type = OPAL_STRING;
            kv->data.string = strdup("PASS");
            opal_list_append(vals[2], &kv->super);
        }
        orcm_db.store_new(orcm_diag_base.dbhandle, ORCM_DB_DIAG_DATA, vals[2], NULL, mycleanup, NULL);
    } else {
        OPAL_LIST_RELEASE(vals[0]);
        OPAL_LIST_RELEASE(vals[1]);
        OPAL_LIST_RELEASE(vals[2]);
    }
    return ORCM_SUCCESS;
}

static void cputest_run(int sd, short args, void *cbdata)
{
    orcm_diag_caddy_t *caddy = (orcm_diag_caddy_t*)cbdata;
    int numprocs = 0;
    int retval   = 0;
    int cpu_diag_ret = 0;
    orcm_diag_cmd_flag_t command = ORCM_DIAG_AGG_COMMAND;
    opal_buffer_t *data = NULL;
    struct timeval now;
    struct timeval start_time;
    char *compname;
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
                            "%s diag:cputest: HNP is not defined",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        /* still run diags? */
        /* return; */
    }

    numprocs = get_nprocs();

    retval = cpu_fp_test();
    if ( retval ) {
        cpu_diag_ret |= DIAG_CPU_FP_TST;
        opal_output(0, "%s Diagnostic checking CPU floating point:  \t\t[ FAIL ]\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    } else {
        opal_output(0, "%s Diagnostic checking CPU floating point:  \t\t[ PASS ]\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    }

    retval = cpu_prime_gen_test();
    if ( retval ) {
        cpu_diag_ret |= DIAG_CPU_PRIME_TST;
        opal_output(0, "%s Diagnostic checking CPU prime generation:\t\t[ FAIL ]\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    } else {
        opal_output(0, "%s Diagnostic checking CPU prime generation:\t\t[ PASS ]\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    }

    retval = cpu_stress_test(numprocs, CPU_STRESS_TIMEOUT);
    if ( retval ) {
        cpu_diag_ret |= DIAG_CPU_STRESS_TST;
        opal_output(0, "%s Diagnostic checking CPU stress:          \t\t[ FAIL ]\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    } else {
        opal_output(0, "%s Diagnostic checking CPU stress:          \t\t[ PASS ]\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    }

    data = OBJ_NEW(opal_buffer_t);

    /* pack aggregator command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &command, 1, ORCM_DIAG_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* pack our component name */
    compname = strdup("cputest");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &compname, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }
    free(compname);

    /* Pack start Time */
    gettimeofday(&start_time, NULL);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &start_time, 1, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* Pack the Time */
    gettimeofday(&now, NULL);
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

    /* Pack our results */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &cpu_diag_ret, 1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

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

static int keep_cpu_busy(void)
{
    while (1) {
        sqrt(rand());
    }

    return 0;
}

static int cpu_stress_test(int cpucnt, int timeout)
{
    pid_t pid     = 0;
    int child_num = 0;
    int fork_num  = 0;
    int retval    = 0;
    time_t start_time, stop_time, cur_time, run_time;

    /* Record start time.  */
    if ( -1 == (start_time = time(NULL)) ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:cannot get system time",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORCM_ERROR;
    }

    fork_num = cpucnt;

    /* Dispatch work processes to stress cpu  */
    while ( fork_num ) {
        long long backoff, tst_timeout = 0;

        /* Calculate backoff sleep time to get good throughput */
        backoff = fork_num * BACKOFF_TIME;

        /* Check time out */
        if ( timeout ) {
            /* Get current time */
            cur_time = time(NULL);

            tst_timeout = timeout - (cur_time - start_time);

            if ( tst_timeout <= 0 ) {
                opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "%s diag:time used up before work dispatched",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            }
        }

        switch ( pid = fork() ) {
        case -1:           /* error */
            opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:fork process failed - %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), strerror(errno));
            return ORCM_ERR_SYS_LIMITS_CHILDREN;
            break;
        case 0:
            alarm(timeout);
            usleep(backoff);
            exit( keep_cpu_busy() );
            break;
        default:           /* parent */
            opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "%s diag:stress cpu work process %i [%i] forked",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), fork_num, pid);
            child_num++;
            break;
        }
        fork_num--;
    }

    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                   "%s diag:finish dispatch",
                   ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* Wait for the child process exit */
    while (child_num) {
        int statval, ret;

        if ((pid = wait(&statval)) > 0)
        {
            --child_num;

            if ( WIFEXITED(statval) ) {
                if ( 0 == (ret = WEXITSTATUS(statval)) ) {
                    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "%s diag:stress cpu child process %i returned successfully",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), pid);
                } else {
                    opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:stress cpu child process %i returned error %i",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), pid, ret);

                    retval++;
                    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "%s diag:stress cpu reap child process",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

                    if ( SIG_ERR == signal(SIGUSR1, SIG_IGN) ) {
                        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:stress cpu signal handler error : %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), strerror(errno) );
                    }

                    if ( -1 == kill(-1 * getpid(), SIGUSR1) ) {
                        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:stress cpu kill process error : %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), strerror(errno) );
                    }
                }
            } else if ( WIFSIGNALED(statval) ) {
                if ( SIGALRM == (ret = WTERMSIG(statval)) ) {
                    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "%s diag:stress cpu child process %i signalled successfully",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), pid);
                } else if ( SIGUSR1 == (ret = WTERMSIG (statval)) ) {
                    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "%s diag:stress cpu child process %i reaped",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), pid);
                } else {
                    opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:stress cpu child process %i got signal %i",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), pid, ret);
                    retval++;
                    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "%s diag:stress cpu reap child process",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

                    if ( SIG_ERR == signal(SIGUSR1, SIG_IGN) ) {
                        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:stress cpu signal handler error : %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), strerror(errno) );
                    }

                    if ( -1 == kill(-1 * getpid(), SIGUSR1) ) {
                        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:stress cpu kill process error : %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), strerror(errno) );
                    }
                }
            } else {
                opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                        "%s diag:stress cpu child process %i exit abnormally",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), pid);
                retval++;
            }
        } else {
            opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                    "%s diag:stress cpu wait for child process",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
            break;
        }
    }

    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                   "%s diag:finish waiting for child process",
                   ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* Calculate runtime. */
    stop_time = time(NULL);
    run_time  = stop_time - start_time;

    if ( retval ) {
        opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                "%s diag:stress cpu failed - run completed in %lis",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), run_time );
    } else {
        opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                "%s diag:stress cpu successful run completed in %lis",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), run_time );
    }

    return retval;
}

static int cpu_fp_test(void)
{
    /* Tets the implementations of the double type corresponds to the IEEE-754 double precision and
       the long double type corresponds to x86 extended precision */
    volatile double x = 4294967304.0; /* 2^32 + 8 */
    volatile double y = 4294967288.0; /* 2^32 - 8 */
    double z, zl;
    long double xl, yl;
    int retval = 0;

    xl = x;
    yl = y;
    z  = (long double)x * (long double)y;
    zl = xl * yl;
    if ( z != zl ) {
        opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                "%s diag:cpu FPU cast to long double test failed",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
        retval++;
    } else {
        opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                "%s diag:cpu FPU cast to long double test pass",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    }

    return retval;
}

static int cpu_prime_gen_test(void)
{
    return ORCM_SUCCESS;
}

