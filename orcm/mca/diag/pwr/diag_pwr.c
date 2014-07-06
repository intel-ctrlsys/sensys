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

#include "opal/class/opal_list.h"
#include "opal/dss/dss.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal/util/os_dirpath.h"
#include "opal/util/os_path.h"
#include "opal/mca/installdirs/installdirs.h"
#include "opal/mca/hwloc/hwloc.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/orte_wait.h"

#include "orcm/mca/db/db.h"
#include "orcm/mca/sensor/base/sensor_private.h"

#include "orcm/mca/diag/base/base.h"
#include "diag_pwr.h"

static int init(void);
static void finalize(void);
static void calibrate(void);

orcm_diag_base_module_t orcm_diag_pwr_module = {
    init,
    finalize,
    calibrate,
    NULL,
    NULL
};

typedef struct {
    opal_list_item_t super;
    int core;
    char *directory;
    /* save the system settings so we can restore them when we die */
    char *system_governor;
    float system_max_freq;
    float system_min_freq;
    /* track the current settings */
    char *current_governor;
    float current_max_freq;
    float current_min_freq;
    /* keep a list of allowed values */
    opal_list_t governors;
    opal_list_t frequencies;
    /* mark if setspeed is supported */
    bool setspeed;
    /* flag if child is running */
    bool child_alive;
    /* pid of power virus child */
    pid_t pid;
} tracker_t;
static void ctr_con(tracker_t *trk)
{
    trk->directory = NULL;
    trk->system_governor = NULL;
    trk->current_governor = NULL;
    OBJ_CONSTRUCT(&trk->governors, opal_list_t);
    OBJ_CONSTRUCT(&trk->frequencies, opal_list_t);
    trk->setspeed = false;
    trk->child_alive = false;
}
static void ctr_des(tracker_t *trk)
{
    if (NULL != trk->directory) {
        free(trk->directory);
    }
    if (NULL != trk->system_governor) {
        free(trk->system_governor);
    }
    if (NULL != trk->current_governor) {
        free(trk->current_governor);
    }
    OPAL_LIST_DESTRUCT(&trk->governors);
    OPAL_LIST_DESTRUCT(&trk->frequencies);
}
OBJ_CLASS_INSTANCE(tracker_t,
                   opal_list_item_t,
                   ctr_con, ctr_des);

typedef struct {
    opal_object_t super;
    volatile bool active;
    int32_t n_active_cores;
    float freq;
} collector_t;
OBJ_CLASS_INSTANCE(collector_t,
                   opal_object_t,
                   NULL, NULL);

static char *orte_getline(FILE *fp)
{
    char *ret, *buff;
    char input[1024];
    int k;

    ret = fgets(input, 1024, fp);
    if (NULL != ret) {
        /* trim the end of the line */
        for (k=strlen(input)-1; 0 < k && isspace(input[k]); k--) {
            input[k] = '\0';
        }
        buff = strdup(input);
        return buff;
    }
    
    return NULL;
}


static opal_list_t tracking;

static int init(void)
{
    int k;
    DIR *cur_dirp = NULL;
    struct dirent *entry;
    char *filename, *tmp, **vals;
    FILE *fp;
    tracker_t *trk;
    opal_value_t *kv;

    OPAL_OUTPUT_VERBOSE((5, orcm_diag_base_framework.framework_output,
                         "%s diag:pwr:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    /* always construct this so we don't segfault in finalize */
    OBJ_CONSTRUCT(&tracking, opal_list_t);

    /*
     * Open up the base directory so we can get a listing
     */
    if (NULL == (cur_dirp = opendir("/sys/devices/system/cpu"))) {
        OBJ_DESTRUCT(&tracking);
        /* disqualify ourselves */
        opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "%s diag:pwr:init required cpu dir not found",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }

    /*
     * For each directory
     */
    while (NULL != (entry = readdir(cur_dirp))) {
        
        /*
         * Skip the obvious
         */
        if (0 == strncmp(entry->d_name, ".", strlen(".")) ||
            0 == strncmp(entry->d_name, "..", strlen(".."))) {
            continue;
        }

        /* look for cpu directories */
        if (0 != strncmp(entry->d_name, "cpu", strlen("cpu"))) {
            /* cannot be a cpu directory */
            continue;
        }
        /* if it ends in other than a digit, then it isn't a cpu directory */
        if (!isdigit(entry->d_name[strlen(entry->d_name)-1])) {
            continue;
        }

        /* track the info for this core */
        trk = OBJ_NEW(tracker_t);
        /* trailing digits are the core id */
        for (k=strlen(entry->d_name)-1; 0 <= k; k--) {
            if (!isdigit(entry->d_name[k])) {
                k++;
                break;
            }
        }
        trk->core = strtoul(&entry->d_name[k], NULL, 10);
        trk->directory = opal_os_path(false, "/sys/devices/system/cpu", entry->d_name, "cpufreq", NULL);
        
        /* read/save the current settings */
        filename = opal_os_path(false, trk->directory, "scaling_governor", NULL);
        fp = fopen(filename, "r");
        trk->system_governor = orte_getline(fp);
        trk->current_governor = strdup(trk->system_governor);
        fclose(fp);
        free(filename);

        filename = opal_os_path(false, trk->directory, "scaling_max_freq", NULL);
        fp = fopen(filename, "r");
        tmp = orte_getline(fp);
        fclose(fp);
        trk->system_max_freq = strtoul(tmp, NULL, 10) / 1000000.0;
        trk->current_max_freq = trk->system_max_freq;
        free(filename);
        free(tmp);

        filename = opal_os_path(false, trk->directory, "scaling_min_freq", NULL);
        fp = fopen(filename, "r");
        tmp = orte_getline(fp);
        fclose(fp);
        trk->system_min_freq = strtoul(tmp, NULL, 10) / 1000000.0;
        trk->current_min_freq = trk->system_min_freq;
        free(filename);
        free(tmp);

        /* get the list of available governors */
        filename = opal_os_path(false, trk->directory, "scaling_available_governors", NULL);
        if (NULL != (fp = fopen(filename, "r"))) {
            tmp = orte_getline(fp);
            fclose(fp);
            free(filename);
            if (NULL != tmp) {
                vals = opal_argv_split(tmp, ' ');
                free(tmp);
                for (k=0; NULL != vals[k]; k++) {
                    kv = OBJ_NEW(opal_value_t);
                    kv->type = OPAL_STRING;
                    kv->data.string = strdup(vals[k]);
                    opal_list_append(&trk->governors, &kv->super);
                }
                opal_argv_free(vals);
            }
        }

        /* get the list of available frequencies - these come in a list from
         * max to min */
        filename = opal_os_path(false, trk->directory, "scaling_available_frequencies", NULL);
        if (NULL != (fp = fopen(filename, "r"))) {
            tmp = orte_getline(fp);
            fclose(fp);
            free(filename);
            if (NULL != tmp) {
                vals = opal_argv_split(tmp, ' ');
                free(tmp);
                for (k=0; NULL != vals[k]; k++) {
                    kv = OBJ_NEW(opal_value_t);
                    kv->type = OPAL_FLOAT;
                    kv->data.fval = strtoul(vals[k], NULL, 10) / 1000000.0;
                    opal_list_append(&trk->frequencies, &kv->super);
                }
                opal_argv_free(vals);
            }
        }

        /* see if setspeed is supported */
        filename = opal_os_path(false, trk->directory, "scaling_setspeed", NULL);
        if (access(filename, W_OK)) {
            trk->setspeed = true;
        }
        free(filename);

        /* add to our list */
        opal_list_append(&tracking, &trk->super);
    }
    closedir(cur_dirp);

    if (0 == opal_list_get_size(&tracking)) {
        /* nothing to calibrate - disqualify ourselves */
        opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "%s diag:pwr:init no cpus found",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }

    /* report out the results, if requested */
    if (9 < opal_output_get_verbosity(orcm_diag_base_framework.framework_output)) {
        OPAL_LIST_FOREACH(trk, &tracking, tracker_t) {
            opal_output(0, "%s\tCore: %d  Governor: %s MaxFreq: %f MinFreq: %f\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), trk->core,
                        trk->system_governor, trk->system_max_freq, trk->system_min_freq);
            OPAL_LIST_FOREACH(kv, &trk->governors, opal_value_t) {
                opal_output(0, "%s\t\tGovernor: %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), kv->data.string);
            }
            OPAL_LIST_FOREACH(kv, &trk->frequencies, opal_value_t) {
                opal_output(0, "%s\t\tFrequency: %f",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), kv->data.fval);
            }
        }
    }
    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_diag_base_framework.framework_output,
                         "%s diag:pwr:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    OPAL_LIST_DESTRUCT(&tracking);
}

/*** REMINDER: THIS CALLBACK IS EXECUTED IN THE
 * DB EVENT BASE ***/
static void mycleanup(int dbhandle, int status,
                      opal_list_t *kvs, void *cbdata)
{
    OPAL_LIST_RELEASE(kvs);
}

/*** REMINDER: THIS CALLBACK IS EXECUTED IN THE SENSOR
 * EVENT BASE ***/
static void sensor_sample(opal_buffer_t *buffer, void *cbdata)
{
    collector_t *coll = (collector_t*)cbdata;
    opal_buffer_t *buf=NULL;
    char *hostname=NULL, *component;
    char *sampletime;
    int rc;
    int32_t n, ncores;
    opal_list_t *vals;
    opal_value_t *kv;
    float fval;
    int i;

    opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                        "%s pwr:calibrate recvd pwr reading",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* unpack the buffer object that is in the incoming buffer */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &buf, &n, OPAL_BUFFER))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }
    /* unpack the pwr sensor component name */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &component, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }
    /* unpack the host this came from */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &hostname, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }
    /* and the number of cores on that host */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &ncores, &n, OPAL_INT32))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }

    /* sample time */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &sampletime, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }

    opal_output_verbose(3, orcm_diag_base_framework.framework_output,
                        "%s Received calib from host %s with %d cores",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname, ncores);

    /* xfr to storage */
    vals = OBJ_NEW(opal_list_t);

    /* start by marking this as calib data */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("CALIB");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("PWR");
    opal_list_append(vals, &kv->super);

    /* load the sample time */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("ctime");
    kv->type = OPAL_STRING;
    kv->data.string = strdup(sampletime);
    free(sampletime);
    opal_list_append(vals, &kv->super);

    /* load the hostname */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("hostname");
    kv->type = OPAL_STRING;
    kv->data.string = strdup(hostname);
    opal_list_append(vals, &kv->super);

    /* load the number of cpus that were running the virus */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("nactive");
    kv->type = OPAL_INT32;
    kv->data.int32 = coll->n_active_cores;
    opal_list_append(vals, &kv->super);

    /* load the frequency */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("nactive");
    kv->type = OPAL_FLOAT;
    kv->data.fval = coll->freq;
    opal_list_append(vals, &kv->super);

    /* load the power reading from each cpu */
    for (i=0; i < ncores; i++) {
        kv = OBJ_NEW(opal_value_t);
        asprintf(&kv->key, "core%d", i);
        kv->type = OPAL_FLOAT;
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &fval, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }
        kv->data.fval = fval;
        opal_list_append(vals, &kv->super);
    }

    /* store it */
    if (0 <= orcm_sensor_base.dbhandle) {
        /* the database framework will release the values */
        orcm_db.store(orcm_sensor_base.dbhandle, "pwr-calib", vals, mycleanup, NULL);
    } else {
        /* cleanup the xfr storage */
        OPAL_LIST_RELEASE(vals);
    }

 cleanup:
    if (NULL != hostname) {
        free(hostname);
    }
    if (NULL != buf) {
        OBJ_RELEASE(buf);
    }

    /* mark that wthe sample was collected */
    coll->active = false;
}

static int set_freq(tracker_t *trk, float freq);
static int spawn_one(int core);
static void reset_freq(tracker_t *trk);

static void calibrate(void)
{
    tracker_t *trk, *nxt;
    char *filename;
    FILE *fp;
    opal_value_t *kv, *nkv;
    collector_t *collector;
    int rc;

    OPAL_OUTPUT_VERBOSE((5, orcm_diag_base_framework.framework_output,
                         "%s diag:pwr:calibrate",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
 
    /* loop over all cpus */
    OPAL_LIST_FOREACH_SAFE(trk, nxt, &tracking, tracker_t) {
        /* ensure the governor is set to userspace so we can control the freq */
        filename = opal_os_path(false, trk->directory, "scaling_governor", NULL);
        if (NULL == (fp = fopen(filename, "w"))) {
            /* not allowed */
            opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                                "%s pwr:calibrate cannot set governor on core %d",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), trk->core);
            free(filename);
            /* remove this entry from the list */
            opal_list_remove_item(&tracking, &trk->super);
            OBJ_RELEASE(trk);
            continue;
        }
        opal_output_verbose(2, orcm_diag_base_framework.framework_output,
                            "%s Setting governor to userspace for core %d",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            trk->core);
        fprintf(fp, "userspace\n");
        fclose(fp);
        free(filename);
        /* set the cpu to its lowest available freq */
        if (ORCM_SUCCESS != set_freq(trk, trk->system_min_freq)) {
            /* remove this entry from the list */
            opal_list_remove_item(&tracking, &trk->super);
            OBJ_RELEASE(trk);
            continue;
        }
    }

    /* read our baseline power - this should be the power minus
     * any executing applications */
    collector = OBJ_NEW(collector_t);
    collector->active = true;
    collector->n_active_cores = 0;
    collector->freq = trk->system_min_freq;
    orcm_sensor.sample("pwr", sensor_sample, collector);
    ORTE_WAIT_FOR_COMPLETION(collector->active);

    /* loop over all cpus */
    OPAL_LIST_FOREACH(trk, &tracking, tracker_t) {
        /* fork/exec our viral program and bind it to this cpu */
        rc = spawn_one(trk->core);
        if (rc < 0) {
            ORTE_ERROR_LOG(rc);
            continue;
        }
        trk->pid = rc;
        trk->child_alive = true;
        collector->n_active_cores++;
        /* for each freq available to this cpu */
        OPAL_LIST_FOREACH_SAFE(kv, nkv, &trk->frequencies, opal_value_t) {
            /* set the frequency of this cpu to this value */
            if (ORCM_SUCCESS != set_freq(trk, kv->data.fval)) {
                /* remove this entry from the list */
                opal_list_remove_item(&trk->frequencies, &kv->super);
                OBJ_RELEASE(kv);
                continue;
            }
            /* sample the power, adding the measurement to the same bucket */
            collector->active = true;
            collector->freq = kv->data.fval;
            orcm_sensor.sample("pwr", sensor_sample, collector);
            ORTE_WAIT_FOR_COMPLETION(collector->active);
        }
    }

    /* reset everything to the system settings and kill all child
     * viral programs */
    OPAL_LIST_FOREACH(trk, &tracking, tracker_t) {
        if (trk->child_alive) {
            opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                                "%s pwr:calibrate terminating power virus pid %lu",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                (unsigned long)trk->pid);            
            kill(trk->pid, SIGKILL);
            trk->child_alive = false;
        }
        filename = opal_os_path(false, trk->directory, "scaling_governor", NULL);
        if (NULL == (fp = fopen(filename, "w"))) {
            /* not allowed */
            opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                                "%s pwr:calibrate cannot reset governor on core %d",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), trk->core);
            free(filename);
            continue;
        }
        opal_output_verbose(2, orcm_diag_base_framework.framework_output,
                            "%s Resetting governor to %s for core %d",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            trk->system_governor, trk->core);
        fprintf(fp, "%s\n", trk->system_governor);
        fclose(fp);
        free(filename);
        /* set the cpu to its initial freqs */
        reset_freq(trk);
    }

    /* now report those readings to our database - for now, just print
     * the results until the db integration is available */
    opal_output_verbose(2, orcm_diag_base_framework.framework_output,
                        "%s pwr:calibration complete",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
}

static int set_freq(tracker_t *trk, float freq)
{
    char *filename;
    FILE *fp;

    if (trk->setspeed) {
        filename = opal_os_path(false, trk->directory, "scaling_setspeed", NULL);
    } else {
        filename = opal_os_path(false, trk->directory, "scaling_min_freq", NULL);
    }
    /* attempt to set the value */
    if (NULL == (fp = fopen(filename, "w"))) {
        /* not allowed */
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s pwr:calibrate cannot set min-freq on core %d",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), trk->core);
        free(filename);
        /* restore the governor */
        filename = opal_os_path(false, trk->directory, "scaling_governor", NULL);
        fp = fopen(filename, "w");
        fprintf(fp, "%s\n", trk->system_governor);
        fclose(fp);
        free(filename);
        return ORCM_ERROR;
    }
    opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                        "%s pwr:calibrate set %s on core %d to %f",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        trk->setspeed ? "freq" : "min-freq",
                        trk->core, freq);
    fprintf(fp, "%ld\n", (unsigned long)(freq * 1000000.0));
    fclose(fp);
    free(filename);
    if (!trk->setspeed) {
        /* need to set the max freq to the same value */
        filename = opal_os_path(false, trk->directory, "scaling_max_freq", NULL);
        /* attempt to set the value */
        if (NULL == (fp = fopen(filename, "w"))) {
            /* not allowed */
            opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                                "%s pwr:calibrate cannot set min-freq on core %d",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), trk->core);
            free(filename);
            /* restore the governor */
            filename = opal_os_path(false, trk->directory, "scaling_governor", NULL);
            fp = fopen(filename, "w");
            fprintf(fp, "%s\n", trk->system_governor);
            fclose(fp);
            free(filename);
            /* restore the min freq */
            filename = opal_os_path(false, trk->directory, "scaling_min_freq", NULL);
            fp = fopen(filename, "w");
            fprintf(fp, "%ld\n", (unsigned long)(trk->system_min_freq * 1000000.0));
            fclose(fp);
            free(filename);
            return ORCM_ERROR;
        }
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s pwr:calibrate set max-freq on core %d to %f",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), trk->core, freq);
        fprintf(fp, "%ld\n", (unsigned long)(freq * 1000000.0));
        fclose(fp);
        free(filename);
    }
    return ORCM_SUCCESS;
}

static int spawn_one(int core)
{
    pid_t pid;
    hwloc_obj_t obj;
    char **argv=NULL;
    char *app;

    pid = fork();
    if (pid < 0) {
        ORTE_ERROR_LOG(ORTE_ERR_SYS_LIMITS_CHILDREN);
        return ORTE_ERR_SYS_LIMITS_CHILDREN;
    }

    if (0 == pid) {
        /* we are the child */
        /* get the topological object of the specified core */
        if (NULL == (obj = hwloc_get_obj_by_type(opal_hwloc_topology, HWLOC_OBJ_PU, core))) {
            opal_output(0, "GOT NULL HWLOC OBJ");
            exit(1);
        }
        /* bind us to the specified PU */
        hwloc_set_cpubind(opal_hwloc_topology, obj->cpuset, 0);

        /* set the path to the virus */
        app = opal_os_path(false, opal_install_dirs.bindir, "opwrvirus", NULL);
        opal_argv_append_nosize(&argv, "opwrvirus");

        /* exec the virus */
        execv(app, argv);

        /* error if we get here */
        opal_output(0, "PWR DIAG FAILED TO EXEC VIRUS");
    }

    /* record the pid */
    return pid;
}

static void reset_freq(tracker_t *trk)
{
    char *filename;
    FILE *fp;

    filename = opal_os_path(false, trk->directory, "scaling_min_freq", NULL);
    if (NULL == (fp = fopen(filename, "w"))) {
        /* not allowed */
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s pwr:calibrate cannot reset min-freq on core %d",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), trk->core);
    } else {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s pwr:calibrate reset min-freq on core %d to %f",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            trk->core, trk->system_min_freq);
        fprintf(fp, "%ld\n", (unsigned long)(trk->system_min_freq * 1000000.0));
        fclose(fp);
    }
    free(filename);

    filename = opal_os_path(false, trk->directory, "scaling_max_freq", NULL);
    if (NULL == (fp = fopen(filename, "w"))) {
        /* not allowed */
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s pwr:calibrate cannot reset max-freq on core %d",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), trk->core);
    } else {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s pwr:calibrate reset max-freq on core %d to %f",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            trk->core, trk->system_max_freq);
        fprintf(fp, "%ld\n", (unsigned long)(trk->system_max_freq * 1000000.0));
        fclose(fp);
    }
    free(filename);
}
