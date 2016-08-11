/*
 * Copyright (c) 2013-2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include <stdio.h>

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#include <stdio.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */
#include <ctype.h>

#include "opal_stdint.h"
#include "opal/class/opal_list.h"
#include "opal/dss/dss.h"
#include "opal/util/os_path.h"
#include "opal/util/output.h"
#include "opal/util/os_dirpath.h"
#include "opal/runtime/opal_progress_threads.h"

#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/notifier/notifier.h"
#include "orte/mca/notifier/base/base.h"

#include "orcm/mca/db/db.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"

#include "orcm/mca/analytics/analytics.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "sensor_freq.h"

#define TEST_CORES (8192)

#define ON_NULL_RETURN(x) if(NULL==x){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);return;}
#define F_CLOSE(x) if(NULL!=x){fclose(x); x=NULL;}
/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void freq_sample(orcm_sensor_sampler_t *sampler);
static void perthread_freq_sample(int fd, short args, void *cbdata);
void collect_freq_sample(orcm_sensor_sampler_t *sampler);
static void freq_log(opal_buffer_t *buf);
void freq_set_sample_rate(int sample_rate);
void freq_get_sample_rate(int *sample_rate);
static void freq_inventory_collect(opal_buffer_t *inventory_snapshot);
static void freq_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);
int freq_enable_sampling(const char* sensor_specification);
int freq_disable_sampling(const char* sensor_specification);
int freq_reset_sampling(const char* sensor_specification);
void freq_get_units(char* label, char** units);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_freq_module = {
    init,
    finalize,
    start,
    stop,
    freq_sample,
    freq_log,
    freq_inventory_collect,
    freq_inventory_log,
    freq_set_sample_rate,
    freq_get_sample_rate,
    freq_enable_sampling,
    freq_disable_sampling,
    freq_reset_sampling
};

/****    COREFREQ EVENT HISTORY TYPE    ****/
/* To reduce memory footprint, we don't store the count for all nodes and cores.
 * we only keep the count of the specific core of specific node that ever cross
 * threshold.
 * An event history consists of:
 * hostname: host name of the compute node that we see coretemp cross threshold
 * core_no: the core number of the compute node that we see coretemp cross threshold
 * hi_thres: high or low threshold
 * severity: severity level assigned to this event
 * count: the number of samples that we see coretemp cross threshold
 * tstamp: timestamp of the cross threshold sample last seen
 */
typedef struct {
    opal_list_item_t super;
    char   *hostname;
    int    core_no;
    bool   hi_thres;
    orte_notifier_severity_t severity;
    int    count;
    time_t tstamp;
} corefreq_history_t;

typedef struct {
    opal_list_item_t super;
    char *file;
    int core;
    float max_freq;
    float min_freq;
} corefreq_tracker_t;
static void ctr_con(corefreq_tracker_t *trk)
{
    trk->file = NULL;
}
static void ctr_des(corefreq_tracker_t *trk)
{
    SAFEFREE(trk->file);
}
OBJ_CLASS_INSTANCE(corefreq_tracker_t,
                   opal_list_item_t,
                   ctr_con, ctr_des);

void freq_ptrk_con(pstate_tracker_t *trk)
{
    trk->file = NULL;
}
void freq_ptrk_des(pstate_tracker_t *trk)
{
    SAFEFREE(trk->file);
    SAFEFREE(trk->sysname);
}
OBJ_CLASS_INSTANCE(pstate_tracker_t,
                   opal_list_item_t,
                   freq_ptrk_con, freq_ptrk_des);

static bool intel_pstate_avail = false;
static opal_list_t tracking;
static opal_list_t pstate_list;
static opal_list_t event_history;
static orcm_sensor_sampler_t *freq_sampler = NULL;
static orcm_sensor_freq_t orcm_sensor_freq;

static void generate_test_vector(opal_buffer_t *v);

static char *orte_getline(FILE *fp)
{
    char *ret, *buff;
    char input[1024];

    ret = fgets(input, 1024, fp);
    if (NULL != ret) {
        input[strlen(input)-1] = '\0';  /* remove newline */
        buff = strdup(input);
        return buff;
    }

    return NULL;
}

/* FOR FUTURE: extend to read cooling device speeds in
 *     current speed: /sys/class/thermal/cooling_deviceN/cur_state
 *     max speed: /sys/class/thermal/cooling_deviceN/max_state
 *     type: /sys/class/thermal/cooling_deviceN/type
 */
static int init(void)
{
    int k;
    DIR *cur_dirp = NULL;
    struct dirent *entry;
    char *filename, *tmp;
    FILE *fp = NULL;
    corefreq_tracker_t *trk;
    pstate_tracker_t *ptrk;

    mca_sensor_freq_component.diagnostics = 0;
    mca_sensor_freq_component.runtime_metrics =
        orcm_sensor_base_runtime_metrics_create("freq", orcm_sensor_base.collect_metrics,
                                                mca_sensor_freq_component.collect_metrics);

    /* always construct this so we don't segfault in finalize */
    OBJ_CONSTRUCT(&tracking, opal_list_t);
    OBJ_CONSTRUCT(&pstate_list, opal_list_t);
    OBJ_CONSTRUCT(&event_history, opal_list_t);

    /*
     * Open up the base directory so we can get a listing
     */
    if (NULL == (cur_dirp = opendir("/sys/devices/system/cpu"))) {
        orte_show_help("help-orcm-sensor-freq.txt", "req-dir-not-found",
                       true, orte_process_info.nodename,
                       "/sys/devices/system/cpu");
        return ORTE_ERROR;
    }
    /*
     * For each directory
     */
    while (NULL != (entry = readdir(cur_dirp))) {

        /*
         * Skip the obvious
         */
        if(0 == strncmp(entry->d_name, ".", strlen("."))){continue;}
        if(0 == strncmp(entry->d_name, "..", strlen(".."))){continue;}


        /* look for cpu directories */
        if (0 != strncmp(entry->d_name, "cpu", strlen("cpu"))) {
             /*cannot be a cpu directory*/
            continue;
        }
        /* empty directory entry, probably not even possible? */
        if ( strlen(entry->d_name) < 1 ) {
            continue;
        }

        /* if it ends in other than a digit, then it isn't a cpu directory */
        if (0 == strlen(entry->d_name) || !isdigit(entry->d_name[strlen(entry->d_name)-1])) {
            continue;
        }

        /* track the info for this core */
        trk = OBJ_NEW(corefreq_tracker_t);
        /* trailing digits are the core id */
        for (k=strlen(entry->d_name)-1; 0 <= k; k--) {
            if (!isdigit(entry->d_name[k])) {
                break;
            }
        }
        trk->core = strtoul(&entry->d_name[k+1], NULL, 10);
        trk->file = opal_os_path(false, "/sys/devices/system/cpu", entry->d_name, "cpufreq", "cpuinfo_cur_freq", NULL);

        /* read the static info */
        if(NULL == (filename = opal_os_path(false, "/sys/devices/system/cpu", entry->d_name, "cpufreq", "cpuinfo_max_freq", NULL))) {
            continue;
        }
        if(NULL != (fp = fopen(filename, "r")))
        {
            if(NULL!=(tmp = orte_getline(fp))) {
                trk->max_freq = strtoul(tmp, NULL, 10) / 1000000.0;
                free(tmp);
                fclose(fp);
                free(filename);
            } else {
                ORTE_ERROR_LOG(ORTE_ERR_FILE_READ_FAILURE);
                fclose(fp);
                free(filename);
                OBJ_RELEASE(trk);
                continue;
            }
        } else {
            ORTE_ERROR_LOG(ORTE_ERR_FILE_OPEN_FAILURE);
            free(filename);
            OBJ_RELEASE(trk);
            continue;
        }

        if(NULL == (filename = opal_os_path(false, "/sys/devices/system/cpu", entry->d_name, "cpufreq", "cpuinfo_min_freq", NULL))) {
            continue;
        }
        if(NULL != (fp = fopen(filename, "r")))
        {
            if(NULL!=(tmp = orte_getline(fp))) {
                trk->min_freq = strtoul(tmp, NULL, 10) / 1000000.0;
                free(tmp);
                fclose(fp);
                free(filename);
            } else {
                ORTE_ERROR_LOG(ORTE_ERR_FILE_READ_FAILURE);
                fclose(fp);
                free(filename);
                OBJ_RELEASE(trk);
                continue;
            }
        } else {
            ORTE_ERROR_LOG(ORTE_ERR_FILE_OPEN_FAILURE);
            free(filename);
            OBJ_RELEASE(trk);
            continue;
        }

        /* add to our list */
        opal_list_append(&tracking, &trk->super);
    }
    closedir(cur_dirp);

    if (0 == opal_list_get_size(&tracking)) {
        /* nothing to read */
        orte_show_help("help-orcm-sensor-freq.txt", "no-cores-found",
                       true, orte_process_info.nodename);
        return ORTE_ERROR;
    }

    if (true == mca_sensor_freq_component.pstate) {
        /* 'intel_pstate' configuration settings.
         * Open up the intel_pstate base directory so we can get a listing
         */
         if (NULL == (cur_dirp = opendir("/sys/devices/system/cpu/intel_pstate"))) {
             opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                    "pstate information requested but module not in use, freq collection will occur");
             intel_pstate_avail = false;
             return ORCM_SUCCESS;
        } else {
            intel_pstate_avail = true;
        }
    } else {
        intel_pstate_avail = false;
        return ORCM_SUCCESS;
    }

    /*
     * For each directory
     */
    while (NULL != (entry = readdir(cur_dirp))) {

        /*
         * Skip the obvious
         */
        if(0 == strncmp(entry->d_name, ".", strlen("."))){continue;}
        if(0 == strncmp(entry->d_name, "..", strlen(".."))){continue;}

        /* track the info for this core */
        ptrk = OBJ_NEW(pstate_tracker_t);
        ptrk->file = opal_os_path(false, "/sys/devices/system/cpu/intel_pstate", entry->d_name, NULL);
        ptrk->sysname = strdup(entry->d_name);

        /* read the static info */
        if(NULL == ptrk->file) {
            OBJ_RELEASE(ptrk);
            continue;
        }
        if(NULL != (fp = fopen(ptrk->file, "r")))
        {
            if(NULL!=(tmp = orte_getline(fp))) {
                ptrk->value = strtoul(tmp, NULL, 10);
                free(tmp);
                fclose(fp);
            } else {
                ORTE_ERROR_LOG(ORTE_ERR_FILE_READ_FAILURE);
                fclose(fp);
                OBJ_RELEASE(ptrk);
                continue;
            }
        } else {
            ORTE_ERROR_LOG(ORTE_ERR_FILE_OPEN_FAILURE);
            OBJ_RELEASE(ptrk);
            continue;
        }

        /* add to our list */
        opal_list_append(&pstate_list, &ptrk->super);
    }
    closedir(cur_dirp);

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_LIST_DESTRUCT(&tracking);
    OPAL_LIST_DESTRUCT(&pstate_list);
    OPAL_LIST_DESTRUCT(&event_history);

    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_freq_component.runtime_metrics);
    mca_sensor_freq_component.runtime_metrics = NULL;
}

/*
 * Start monitoring of local temps
 */
static void start(orte_jobid_t jobid)
{
    /* start a separate freq progress thread for sampling */
    if (mca_sensor_freq_component.use_progress_thread) {
        if (!orcm_sensor_freq.ev_active) {
            orcm_sensor_freq.ev_active = true;
            if (NULL == (orcm_sensor_freq.ev_base = opal_progress_thread_init("freq"))) {
                orcm_sensor_freq.ev_active = false;
                return;
            }
        }

        /* setup freq sampler */
        freq_sampler = OBJ_NEW(orcm_sensor_sampler_t);

        /* check if freq sample rate is provided for this*/
        if (!mca_sensor_freq_component.sample_rate) {
            mca_sensor_freq_component.sample_rate = orcm_sensor_base.sample_rate;
        }
        freq_sampler->rate.tv_sec = mca_sensor_freq_component.sample_rate;
        freq_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(orcm_sensor_freq.ev_base, &freq_sampler->ev,
                               perthread_freq_sample, freq_sampler);
        opal_event_evtimer_add(&freq_sampler->ev, &freq_sampler->rate);
    }else{
        mca_sensor_freq_component.sample_rate = orcm_sensor_base.sample_rate;

    }

    return;
}


static void stop(orte_jobid_t jobid)
{
    if (orcm_sensor_freq.ev_active) {
        orcm_sensor_freq.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_progress_thread_pause("freq");
        OBJ_RELEASE(freq_sampler);
    }
    return;
}

static void freq_sample(orcm_sensor_sampler_t *sampler)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor freq : freq_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    if (!mca_sensor_freq_component.use_progress_thread) {
       collect_freq_sample(sampler);
    }

}

static void perthread_freq_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor freq : perthread_freq_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* this has fired in the sampler thread, so we are okay to
     * just go ahead and sample since we do NOT allow both the
     * base thread and the component thread to both be actively
     * calling this component */
    collect_freq_sample(sampler);
    /* we now need to push the results into the base event thread
     * so it can add the data to the base bucket */
    ORCM_SENSOR_XFER(&sampler->bucket);
    /* clear the bucket */
    OBJ_DESTRUCT(&sampler->bucket);
    OBJ_CONSTRUCT(&sampler->bucket, opal_buffer_t);
    /* check if freq sample rate is provided for this*/
    if (mca_sensor_freq_component.sample_rate != sampler->rate.tv_sec) {
        sampler->rate.tv_sec = mca_sensor_freq_component.sample_rate;
    }
    /* set ourselves to sample again */
    opal_event_evtimer_add(&sampler->ev, &sampler->rate);
}

void collect_freq_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    corefreq_tracker_t *trk, *nxt;
    pstate_tracker_t *ptrk, *pnxt;
    FILE *fp = NULL;
    const char *cfreq = "freq";
    char* freq_data = NULL;
    float ghz;
    opal_buffer_t data, *bptr;
    int32_t ncores;
    bool packed;
    struct timeval current_time;
    void* metrics_obj = mca_sensor_freq_component.runtime_metrics;

    if (mca_sensor_freq_component.test){
        /* generate and send a test vector */
        OBJ_CONSTRUCT(&data, opal_buffer_t);
        generate_test_vector(&data);
        bptr = &data;
        opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
        OBJ_DESTRUCT(&data);
        return;
    }

    if(0 == orcm_sensor_base_runtime_metrics_active_label_count(metrics_obj) &&
       !orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, NULL)) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor freq : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    mca_sensor_freq_component.diagnostics |= 0x1;

    if (0 == opal_list_get_size(&tracking)) {
        return;
    }

    opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                        "%s sampling freq",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* prep to store the results */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    packed = false;
    orcm_sensor_base_runtime_metrics_begin(mca_sensor_freq_component.runtime_metrics);

    /* pack our name */
    ret = opal_dss.pack(&data, &cfreq, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(ret,cleanup);

    /* store our hostname */
    ret = opal_dss.pack(&data, &orte_process_info.nodename, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(ret,cleanup);

    /* Store number of labels to collect */
    if(orcm_sensor_base_runtime_inventory_available(mca_sensor_freq_component.runtime_metrics)) {
        ncores = (int32_t)orcm_sensor_base_runtime_metrics_active_label_count(mca_sensor_freq_component.runtime_metrics);
    } else {
        ncores = (int32_t)opal_list_get_size(&tracking) + (int32_t)opal_list_get_size(&pstate_list);
    }
    ret = opal_dss.pack(&data, &ncores, 1, OPAL_INT32);
    ORCM_ON_FAILURE_GOTO(ret,cleanup);

    /* get the sample time */
    gettimeofday(&current_time, NULL);
    ret = opal_dss.pack(&data, &current_time, 1, OPAL_TIMEVAL);
    ORCM_ON_FAILURE_GOTO(ret,cleanup);

    int core = 0;
    OPAL_LIST_FOREACH_SAFE(trk, nxt, &tracking, corefreq_tracker_t) {
        char* label = NULL;
        asprintf(&label, "core%d", core++);
        opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                            "%s processing freq file %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            trk->file);
        /* read the freq */
        if (NULL == (fp = fopen(trk->file, "r"))) {
            ORTE_ERROR_LOG(ORCM_ERR_PERM);
            SAFEFREE(label);
            goto cleanup;
        }
        while (NULL != (freq_data = orte_getline(fp))) {
            ghz = strtoul(freq_data, NULL, 10) / 1000000.0;
            if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_freq_component.runtime_metrics, label)) {
                uint8_t type = OPAL_FLOAT;
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                    "%s sensor:freq: Core %d freq %f max %f min %f",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                    trk->core, ghz, trk->max_freq, trk->min_freq);
                ret = opal_dss.pack(&data, &label, 1, OPAL_STRING);
                SAFEFREE(label);
                ORCM_ON_FAILURE_GOTO(ret,cleanup);

                ret = opal_dss.pack(&data, &type, 1, OPAL_UINT8);
                ORCM_ON_FAILURE_GOTO(ret,cleanup);

                ret = opal_dss.pack(&data, &ghz, 1, OPAL_FLOAT);
                ORCM_ON_FAILURE_GOTO(ret,cleanup);
                packed = true;
            }
            SAFEFREE(freq_data);
        }
        F_CLOSE(fp);
        SAFEFREE(label);
    }

    OPAL_LIST_FOREACH_SAFE(ptrk, pnxt, &pstate_list, pstate_tracker_t) {
        opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                            "%s processing freq file %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            ptrk->file);
        /* read the value */
        if (NULL == (fp = fopen(ptrk->file, "r"))) {
            /* we can't be read, so remove it from the list */
            opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                "%s access denied to freq file %s - removing it",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                ptrk->file);
            opal_list_remove_item(&pstate_list, &ptrk->super);
            OBJ_RELEASE(ptrk);
            continue;
        }
        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_freq_component.runtime_metrics, ptrk->sysname)) {
            uint8_t type = OPAL_UINT;
            ret = opal_dss.pack(&data, &ptrk->sysname, 1, OPAL_STRING);
            ORCM_ON_FAILURE_GOTO(ret,cleanup);

            ret = opal_dss.pack(&data, &type, 1, OPAL_UINT8);
            ORCM_ON_FAILURE_GOTO(ret,cleanup);

            while (NULL != (freq_data = orte_getline(fp))) {
                ptrk->value = strtoul(freq_data, NULL, 10);
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                    "%s sensor:pstate: file %s : %d",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                    ptrk->file, ptrk->value);
                ret = opal_dss.pack(&data, &ptrk->value, 1, OPAL_UINT);
                ORCM_ON_FAILURE_GOTO(ret,cleanup);
                packed = true;
                SAFEFREE(freq_data);
            }
        }
        F_CLOSE(fp);
    }

    /* xfer the data for transmission */
    if (packed) {
        bptr = &data;
        ret = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
        ORCM_ON_FAILURE_GOTO(ret,cleanup);
    }

cleanup:
    orcm_sensor_base_runtime_metrics_end(mca_sensor_freq_component.runtime_metrics);
    OBJ_DESTRUCT(&data);
    F_CLOSE(fp);
    SAFEFREE(freq_data);
}

static void freq_log_cleanup(char *label, char *hostname, opal_list_t *key,
                             opal_list_t *non_compute_data, orcm_analytics_value_t *analytics_vals)
{
    SAFEFREE(label);
    SAFEFREE(hostname);
    SAFE_RELEASE(key);
    SAFE_RELEASE(non_compute_data);
    SAFE_RELEASE(analytics_vals);
}

static void freq_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    struct timeval sampletime;
    int rc = ORCM_SUCCESS;
    int32_t n = 0;
    int32_t ncores = 0;
    orcm_analytics_value_t *analytics_vals = NULL;
    opal_list_t *key = NULL;
    opal_list_t *non_compute_data = NULL;
    float fval = 0;
    int idx = 0;

    /* unpack the host this came from */
    n = 1;
    rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING);
    ORCM_ON_FAILURE_RETURN(rc);

    /* and the number of samples on that host */
    n = 1;
    rc = opal_dss.unpack(sample, &ncores, &n, OPAL_INT32);
    ORCM_ON_FAILURE_GOTO(rc,cleanup);

     /* sample time */
    n = 1;
    rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_TIMEVAL);
    ORCM_ON_FAILURE_GOTO(rc,cleanup);
    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "%s Received freq log from host %s with %d cores",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname, ncores);

    /* fill the key list with hostname and data_group */
    key = OBJ_NEW(opal_list_t);
    ORCM_ON_NULL_GOTO(key,cleanup);
    rc = orcm_util_append_orcm_value(key, "hostname", hostname, OPAL_STRING, NULL);
    ORCM_ON_FAILURE_GOTO(rc,cleanup);
    SAFEFREE(hostname);
    rc = orcm_util_append_orcm_value(key, "data_group", "freq", OPAL_STRING, NULL);
    ORCM_ON_FAILURE_GOTO(rc,cleanup);

    /* fill the non compute data list with time stamp */
    non_compute_data = OBJ_NEW(opal_list_t);
    ORCM_ON_NULL_GOTO(non_compute_data,cleanup);
    rc = orcm_util_append_orcm_value(non_compute_data, "ctime", &sampletime, OPAL_TIMEVAL, NULL);
    ORCM_ON_FAILURE_GOTO(rc,cleanup);

    /* fill the compute data with the coretemp of all cores */
    analytics_vals = orcm_util_load_orcm_analytics_value(key, non_compute_data, NULL);
    ORCM_ON_NULL_GOTO(analytics_vals,cleanup);
    ORCM_ON_NULL_GOTO(analytics_vals->key,cleanup);
    ORCM_ON_NULL_GOTO(analytics_vals->non_compute_data,cleanup);
    ORCM_ON_NULL_GOTO(analytics_vals->compute_data,cleanup);

    for (idx = 0; idx < ncores; idx++) {
        char* label = NULL;
        uint8_t type;
        n = 1;
        rc = opal_dss.unpack(sample, &label, &n, OPAL_STRING);
        ORCM_ON_FAILURE_GOTO(rc,cleanup);
        n = 1;
        rc = opal_dss.unpack(sample, &type, &n, OPAL_UINT8);
        ORCM_ON_FAILURE_GOTO(rc,cleanup);
        n = 1;
        if(OPAL_FLOAT == type) {
            rc = opal_dss.unpack(sample, &fval, &n, OPAL_FLOAT);
            ORCM_ON_FAILURE_GOTO(rc,cleanup);
            rc = orcm_util_append_orcm_value(analytics_vals->compute_data,
            label, &fval, OPAL_FLOAT, "GHz");
            ORCM_ON_FAILURE_GOTO(rc,cleanup);
        } else if(OPAL_UINT == type) {
            unsigned int uival;
            rc = opal_dss.unpack(sample, &uival, &n, OPAL_UINT);
            ORCM_ON_FAILURE_GOTO(rc,cleanup);
            char* units = "%";
            freq_get_units(label,&units);
            rc = orcm_util_append_orcm_value(analytics_vals->compute_data,
                                             label, &uival, OPAL_UINT, units);
            ORCM_ON_FAILURE_GOTO(rc,cleanup);
        }
        SAFEFREE(label);
    }

    orcm_analytics.send_data(analytics_vals);
cleanup:
    freq_log_cleanup(NULL, hostname, key, non_compute_data, analytics_vals);

    /* unpack the pstate entry count */
}
void freq_get_units(char* label, char** units)
{
    *units="%";
    if ((11 == strlen(label) && 0 == strncmp(label, "num_pstates", 11)) ||
       (8 == strlen(label) && 0 == strncmp(label, "no_turbo", 8)) ||
       (11 == strlen(label) && 0 == strncmp(label, "allow_turbo", 11))) {
        *units = "";
    }
}
void freq_set_sample_rate(int sample_rate)
{
    /* set the freq sample rate if seperate thread is enabled */
    if (mca_sensor_freq_component.use_progress_thread) {
        mca_sensor_freq_component.sample_rate = sample_rate;
    }
    return;
}

void freq_get_sample_rate(int *sample_rate)
{
    if (NULL != sample_rate) {
    /* check if freq sample rate is provided for this*/
        *sample_rate = mca_sensor_freq_component.sample_rate;
    }
    return;
}

static void generate_test_vector(opal_buffer_t *v)
{
    int ret;
    const char *ctmp  = "freq";
    struct timeval sample_time;
    int32_t ncores;
    int i;
    /* Units are GHz */
    float test_freq;
    float max_freq;
    float min_freq;

    ncores = TEST_CORES;
    min_freq = 1.0;
    max_freq = 5.0;
    test_freq = min_freq;

/* pack the plugin name */
    ret = opal_dss.pack(v, &ctmp, 1, OPAL_STRING);
    ORCM_ON_FAILURE_RETURN(ret);
/* pack the hostname */
    ret = opal_dss.pack(v, &orte_process_info.nodename, 1, OPAL_STRING);
    ORCM_ON_FAILURE_RETURN(ret);
/* pack then number of cores */
    ret = opal_dss.pack(v, &ncores, 1, OPAL_INT32);
    ORCM_ON_FAILURE_RETURN(ret);
/* get the sample time */
    gettimeofday(&sample_time, NULL);
    ret = opal_dss.pack(v, &sample_time, 1, OPAL_TIMEVAL);
    ORCM_ON_FAILURE_RETURN(ret);

/* Pack test core freqs */
    for (i=0; i < ncores; i++) {
        char* l = NULL;
        asprintf(&l, "core%d", i);
        ON_NULL_RETURN(l);

        ret = opal_dss.pack(v, &l, 1, OPAL_STRING);
        SAFEFREE(l);
        ORCM_ON_FAILURE_RETURN(ret);
        uint8_t t = OPAL_FLOAT;
        ret = opal_dss.pack(v, &t, 1, OPAL_UINT8);
        ORCM_ON_FAILURE_RETURN(ret);
        ret = opal_dss.pack(v, &test_freq, 1, OPAL_FLOAT);
        ORCM_ON_FAILURE_RETURN(ret);
        test_freq += 0.01;
        if (test_freq >= max_freq){
            test_freq = min_freq;
        }
    }

    opal_output_verbose(5,orcm_sensor_base_framework.framework_output,
    "%s sensor:freq: Size of test vector is %d",
    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),ncores);
}

static void generate_test_inv_data(opal_buffer_t *inventory_snapshot)
{
    unsigned int tot_items = TEST_CORES;
    unsigned int i = 0;
    const char *key = "freq";
    char *comp = NULL;
    int rc = OPAL_SUCCESS;
    struct timeval time_stamp;
    rc = opal_dss.pack(inventory_snapshot, &key, 1, OPAL_STRING);
    ORCM_ON_FAILURE_RETURN(rc);
    gettimeofday(&time_stamp, NULL);
    rc = opal_dss.pack(inventory_snapshot, &time_stamp, 1, OPAL_TIMEVAL);
    ORCM_ON_FAILURE_RETURN(rc);
    rc = opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT);
    SAFEFREE(comp);
    ORCM_ON_FAILURE_RETURN(rc);
    for(i = 0; i < tot_items; ++i)
    {
        asprintf(&comp, "sensor_freq_%d", i+1);
        ON_NULL_RETURN(comp);
        rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
        SAFEFREE(comp);
        ORCM_ON_FAILURE_RETURN(rc);
        asprintf(&comp, "core%d", i);
        ORCM_ON_NULL_RETURN(comp);
        rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
        ORCM_ON_FAILURE_RETURN(rc);
    }
}

static void freq_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    if(mca_sensor_freq_component.test) {
        generate_test_inv_data(inventory_snapshot);
    } else {
        unsigned int tot_items = (unsigned int)opal_list_get_size(&tracking);
        unsigned int offset = 0;
        unsigned int i = 0;
        const char *ccomp = "freq";
        char *comp = NULL;
        int rc = OPAL_SUCCESS;
        pstate_tracker_t* ptrk = NULL;
        struct timeval time_stamp;

        tot_items += (unsigned int)opal_list_get_size(&pstate_list);
        rc = opal_dss.pack(inventory_snapshot, &ccomp, 1, OPAL_STRING);
        ORCM_ON_FAILURE_RETURN(rc);
        /*store the time_stamp*/
        gettimeofday(&time_stamp, NULL);
        rc = opal_dss.pack(inventory_snapshot, &time_stamp, 1, OPAL_TIMEVAL);
        ORCM_ON_FAILURE_RETURN(rc);
        /*pack the tot_items*/
        rc = opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT);
        ORCM_ON_FAILURE_RETURN(rc);
        /*pack the hostname*/
        //rc = opal_dss.pack(inventory_snapshot, &orte_process_info.nodename, 1, OPAL_STRING);
        //ORCM_ON_FAILURE_RETURN(rc);

        for(i = 0; i < tot_items; ++i) {
            asprintf(&comp, "sensor_freq_%d", i+1);
            rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
            SAFEFREE(comp);
            ORCM_ON_FAILURE_RETURN(rc);
            asprintf(&comp, "core%d", i);
            orcm_sensor_base_runtime_metrics_track(mca_sensor_freq_component.runtime_metrics, comp);
            rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
            SAFEFREE(comp);
            ORCM_ON_FAILURE_RETURN(rc);
        }
        offset = tot_items;
        OPAL_LIST_FOREACH(ptrk, &pstate_list, pstate_tracker_t) {
            asprintf(&comp, "sensor_freq_%d", ++offset);
            rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
            SAFEFREE(comp);
            ORCM_ON_FAILURE_RETURN(rc);
            comp = strdup(ptrk->sysname);
            if (NULL == comp) {
                ORTE_ERROR_LOG(ORTE_ERR_COPY_FAILURE);
                return;
            }
            orcm_sensor_base_runtime_metrics_track(mca_sensor_freq_component.runtime_metrics, comp);
            rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
            SAFEFREE(comp);
            ORCM_ON_FAILURE_RETURN(rc);
        }
    }
}

static void my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata)
{
    ORCM_RELEASE(kvs);
}

static void freq_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
{
    unsigned int tot_items = 0;
    int n = 1;
    opal_list_t *records = NULL;
    int rc = OPAL_SUCCESS;
    orcm_value_t *time_stamp;
    orcm_value_t *host_name = NULL;
    struct timeval current_time;
    char *inv_val = NULL;
    char *inv = NULL;

    rc = opal_dss.unpack(inventory_snapshot, &current_time, &n, OPAL_TIMEVAL);
    ORCM_ON_FAILURE_RETURN(rc);
    n=1;
    rc = opal_dss.unpack(inventory_snapshot, &tot_items, &n, OPAL_UINT);
    ORCM_ON_FAILURE_RETURN(rc);

    host_name = orcm_util_load_orcm_value("hostname", hostname, OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(host_name);

    time_stamp = orcm_util_load_orcm_value("ctime", &current_time, OPAL_TIMEVAL, NULL);
    ORCM_ON_NULL_RETURN(time_stamp);

    records = OBJ_NEW(opal_list_t);
    opal_list_append(records, (opal_list_item_t*)time_stamp);
    opal_list_append(records, (opal_list_item_t*)host_name);
    time_stamp = NULL;
    host_name = NULL;
    while(tot_items > 0) {
        orcm_value_t *mkv = NULL;

        n=1;
        rc = opal_dss.unpack(inventory_snapshot, &inv, &n, OPAL_STRING);
        ORCM_ON_FAILURE_GOTO(rc,cleanup);

        n=1;
        rc = opal_dss.unpack(inventory_snapshot, &inv_val, &n, OPAL_STRING);
        ORCM_ON_FAILURE_GOTO(rc,cleanup);

        mkv = OBJ_NEW(orcm_value_t);
        mkv->value.key = inv;
        inv=NULL;
        mkv->value.type = OPAL_STRING;
        mkv->value.data.string = inv_val;
        inv_val = NULL;
        opal_list_append(records, (opal_list_item_t*)mkv);

        --tot_items;

    }

    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA, records, NULL, my_inventory_log_cleanup, NULL);
    } else {
        my_inventory_log_cleanup(-1, -1, records, NULL, NULL);
    }
    records=NULL;
cleanup:
    ORCM_RELEASE(host_name);
    ORCM_RELEASE(records);
    SAFEFREE(inv_val);
    SAFEFREE(inv);

}

int freq_enable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_freq_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, true, sensor_specification);
}

int freq_disable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_freq_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, false, sensor_specification);
}

int freq_reset_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_freq_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_reset(metrics, sensor_specification);
}
