/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

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

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_freq.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void freq_sample(orcm_sensor_sampler_t *sampler);
static void perthread_freq_sample(int fd, short args, void *cbdata);
static void collect_sample(orcm_sensor_sampler_t *sampler);
static void freq_log(opal_buffer_t *buf);
static void freq_set_sample_rate(int sample_rate);
static void freq_get_sample_rate(int *sample_rate);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_freq_module = {
    init,
    finalize,
    start,
    stop,
    freq_sample,
    freq_log,
    NULL,
    NULL,
    freq_set_sample_rate,
    freq_get_sample_rate
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
static void hst_con(corefreq_history_t *hst)
{
    hst->hostname = NULL;
    hst->core_no  = 0;
    hst->hi_thres = true;
    hst->severity = ORTE_NOTIFIER_INFO;
    hst->count    = 0;
    hst->tstamp   = 0;
}
static void hst_des(corefreq_history_t *hst)
{
    if (NULL != hst->hostname) {
        free(hst->hostname);
    }

}
OBJ_CLASS_INSTANCE(corefreq_history_t,
                   opal_list_item_t,
                   hst_con, hst_des);

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
    if (NULL != trk->file) {
        free(trk->file);
    }
}
OBJ_CLASS_INSTANCE(corefreq_tracker_t,
                   opal_list_item_t,
                   ctr_con, ctr_des);

typedef struct {
    opal_list_item_t super;
    char *file;     /* sysfs entry file location */
    char *sysname;  /* sysfs entry name */
    unsigned int value;
} pstate_tracker_t;
static void ptrk_con(pstate_tracker_t *trk)
{
    trk->file = NULL;
}
static void ptrk_des(pstate_tracker_t *trk)
{
    if (NULL != trk->file) {
        free(trk->file);
    }
    if(NULL != trk->sysname) {
        free(trk->sysname);
    }
}
OBJ_CLASS_INSTANCE(pstate_tracker_t,
                   opal_list_item_t,
                   ptrk_con, ptrk_des);

static bool log_enabled = true;
static bool intel_pstate_avail = false;
static opal_list_t tracking;
static opal_list_t pstate_list;
static opal_list_t event_history;
static orcm_sensor_sampler_t *freq_sampler = NULL;
static orcm_sensor_freq_t orcm_sensor_freq;

char **corefreq_policy_list; /* store corefreq policies from MCA parameter */

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

static int corefreq_load_policy(char *policy)
{
    char **tokens = NULL;
    int array_length = 0;
    orcm_sensor_policy_t *plc, *newplc;
    bool found_me;
    char *sensor_name = NULL;
    char *action = NULL;
    float threshold;
    bool hi_thres;
    int max_count, time_window;
    orte_notifier_severity_t sev;
    int ret;

    ret = ORCM_ERR_BAD_PARAM;
    tokens = opal_argv_split(policy, ':');
    array_length = opal_argv_count(tokens);

    sensor_name = strdup("corefreq");
    if ( 6 == array_length ) {
        threshold = (float)strtof(tokens[0], NULL);
        if ( 0 == strcmp(tokens[1], "hi") ) {
            hi_thres = true;
        } else if ( 0 == strcmp(tokens[1], "lo") ) {
            hi_thres = false;
        } else {
            goto done;
        }

        if (isdigit(tokens[2][strlen(tokens[2]) - 1])) {
            max_count = (int)strtol(tokens[2], NULL, 10);
        } else {
            goto done;
        }

        if (isdigit(tokens[3][strlen(tokens[3]) - 1])) {
            time_window = (int)strtol(tokens[3], NULL, 10);
        } else {
            goto done;
        }

        if ( 0 == strcmp(tokens[4], "emerg") ) {
            sev = ORTE_NOTIFIER_EMERG;
        } else if ( 0 == strcmp(tokens[4], "alert") ) {
            sev = ORTE_NOTIFIER_ALERT;
        } else if ( 0 == strcmp(tokens[4], "crit") ) {
            sev = ORTE_NOTIFIER_CRIT;
        } else if ( 0 == strcmp(tokens[4], "error") ) {
            sev = ORTE_NOTIFIER_ERROR;
        } else if ( 0 == strcmp(tokens[4], "warn") ) {
            sev = ORTE_NOTIFIER_WARN;
        } else if ( 0 == strcmp(tokens[4], "notice") ) {
            sev = ORTE_NOTIFIER_NOTICE;
        } else if ( 0 == strcmp(tokens[4], "info") ) {
            sev = ORTE_NOTIFIER_INFO;
        } else if ( 0 == strcmp(tokens[4], "debug") ) {
            sev = ORTE_NOTIFIER_DEBUG;
        } else {
            goto done;
        }

        action = strdup(tokens[5]);

        /* look for sensor event policy; update with new setting or create new policy if not existing */
        found_me = false;
        OPAL_LIST_FOREACH(plc, &orcm_sensor_base.policy, orcm_sensor_policy_t) {
            if ( (0 == strcmp(sensor_name, plc->sensor_name)) &&
                 (hi_thres == plc->hi_thres ) &&
                 (sev == plc->severity) ) {
                found_me = true;
                /* update existing policy */
                plc->threshold = threshold;
                plc->max_count = max_count;
                plc->time_window = time_window;
                plc->action = strdup(action);
                break;
            }
        }

        if ( !found_me ) {
            /* matched policy not found, insert into policy list */
            newplc = OBJ_NEW(orcm_sensor_policy_t);
            newplc->sensor_name = strdup(sensor_name);
            newplc->threshold = threshold;
            newplc->hi_thres  = hi_thres;
            newplc->max_count = max_count;
            newplc->time_window = time_window;
            newplc->severity  = sev;
            newplc->action = strdup(action);

            opal_list_append(&orcm_sensor_base.policy, &newplc->super);
            opal_output(0, "Add policy: %s %.2f %s %d %d %d %s!",
                                newplc->sensor_name, newplc->threshold, newplc->hi_thres ? "higher" : "lower",
                                newplc->max_count, newplc->time_window, newplc->severity, newplc->action);
        }
        ret = ORCM_SUCCESS;

    } else {
        goto done;
    }

done:
    if ( NULL != sensor_name ) {
        free(sensor_name);
    }

    if ( NULL != action ) {
        free(action);
    }

    opal_argv_free(tokens);
    return ret;
}

static int corefreq_policy_filter(char *hostname, int core_no, float cf, time_t ts)
{
    orcm_sensor_policy_t *plc;
    corefreq_history_t *hst, *newhst;
    bool found_me;
    char *sev;
    char *msg;

    /* Check if this sample may be filtered
     * We have to check for all policies, one single sample might trigger
     * multiple events with different severity levels
     */
    OPAL_LIST_FOREACH(plc, &orcm_sensor_base.policy, orcm_sensor_policy_t) {
        /* check for corefreq sensor type */
        if ( 0 != strcmp(plc->sensor_name, "corefreq") ) {
            continue;
        }

        if ( (plc->hi_thres && (cf < plc->threshold) ) ||
             (!plc->hi_thres && (cf > plc->threshold) ) ) {
            continue;
        }

        switch ( plc->severity ) {
        case ORTE_NOTIFIER_EMERG:
            sev = strdup("EMERG");
            break;
        case ORTE_NOTIFIER_ALERT:
            sev = strdup("ALERT");
            break;
        case ORTE_NOTIFIER_CRIT:
            sev = strdup("CRIT");
            break;
        case ORTE_NOTIFIER_ERROR:
            sev = strdup("ERROR");
            break;
        case ORTE_NOTIFIER_WARN:
            sev = strdup("WARN");
            break;
        case ORTE_NOTIFIER_NOTICE:
            sev = strdup("NOTICE");
            break;
        case ORTE_NOTIFIER_INFO:
            sev = strdup("INFO");
            break;
        case ORTE_NOTIFIER_DEBUG:
            sev = strdup("DEBUG");
            break;
        default:
            sev = strdup("UNKNOWN");
            break;
        }

        /* this sample should be accounted for this policy
         * we have a candidate, let's check there is similar sample accounted in history or not
         */
        found_me = false;
        OPAL_LIST_FOREACH(hst, &event_history, corefreq_history_t) {
            /* check for any matching record in history list */
            /* match by host name and core number */
            if (   hst->tstamp &&
                 ( 0 == strcmp(hostname, hst->hostname) ) &&
                 ( core_no == hst->core_no ) &&
                 ( plc->hi_thres == hst->hi_thres ) &&
                 ( plc->severity == hst->severity) ) {
                found_me = true;
                /* this sample pattern has been captured in history
                 * let's test the time windows expiration
                 */
                if ( (hst->tstamp + plc->time_window) < ts ) {
                    /* Matching history record had expired, just overwrite it */
                    hst->count = 1;
                    hst->tstamp = ts;
                } else {
                    hst->count++;
                }

                /* filter policy threshold reached */
                if ( hst->count >= plc->max_count ) {
                    /* fire an event */
                    asprintf(&msg, "host: %s core %d freq %f GHz, %s than threshold %f GHz for %d times in %d seconds",
                                    hostname, core_no, cf, plc->hi_thres ? "higher" : "lower",
                                    plc->threshold, plc->max_count, plc->time_window);
                    ORTE_NOTIFIER_SYSTEM_EVENT(plc->severity, msg, plc->action);
                    opal_output(0, "host: %s core %d freq %f GHz, %s than threshold %f GHz for %d times in %d seconds, trigger %s event!",
                                    hostname, core_no, cf, plc->hi_thres ? "higher" : "lower",
                                    plc->threshold, plc->max_count, plc->time_window, sev);
                    /* stop watching for this history record, remove from list */
                    opal_list_remove_item(&event_history, &hst->super);
                }

                break;
            }
        }

        if ( !found_me ) {
            /* matched sample not seen before, insert into history list */
            if ( 1 == plc->max_count ) {
                /* fire an event right away, no need to store in history list */
                asprintf(&msg, "host: %s core %d freq %f GHz, %s than threshold %f GHz for %d times in %d seconds",
                                    hostname, core_no, cf, plc->hi_thres ? "higher" : "lower",
                                    plc->threshold, plc->max_count, plc->time_window);
                ORTE_NOTIFIER_SYSTEM_EVENT(plc->severity, msg, plc->action);
                opal_output(0, "host: %s Core %d freq %f GHz, %s than threshold %f GHz for %d times in %d seconds, trigger %s event!",
                                    hostname, core_no, cf, plc->hi_thres ? "higher" : "lower",
                                    plc->threshold, plc->max_count, plc->time_window, sev);
            } else {
                newhst = OBJ_NEW(corefreq_history_t);
                newhst->hostname = strdup(hostname);
                newhst->core_no = core_no;
                newhst->hi_thres = plc->hi_thres;
                newhst->severity = plc->severity;
                newhst->count = 1;
                newhst->tstamp = ts;
                opal_list_append(&event_history, &newhst->super);
            }
        }
        free(sev);
    }


  return 1;
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
    FILE *fp;
    corefreq_tracker_t *trk;
    pstate_tracker_t *ptrk;
    int i = 0;
    int ret = 0;

    /* always construct this so we don't segfault in finalize */
    OBJ_CONSTRUCT(&tracking, opal_list_t);
    OBJ_CONSTRUCT(&pstate_list, opal_list_t);
    OBJ_CONSTRUCT(&event_history, opal_list_t);

    /* get policy from MCA parameters */
    if( NULL != mca_sensor_freq_component.policy ) {
        corefreq_policy_list = opal_argv_split(mca_sensor_freq_component.policy, ',');
    }

    /* load policies */
    for(i =0; i <opal_argv_count(corefreq_policy_list); i++) {
        ret = corefreq_load_policy(corefreq_policy_list[i]);
        if ( ORCM_SUCCESS != ret ) {
            opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                "%s failed loading corefreq policy - %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                corefreq_policy_list[i]);
        }
    }
    opal_argv_free(corefreq_policy_list);

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
        if (0 == strncmp(entry->d_name, ".", strlen(".")) ||
            0 == strncmp(entry->d_name, "..", strlen(".."))) {
            continue;
        }

        /* look for cpu directories */
        if (0 != strncmp(entry->d_name, "cpu", strlen("cpu"))) {
            /* cannot be a cpu directory */
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
        if (0 == strncmp(entry->d_name, ".", strlen(".")) ||
            0 == strncmp(entry->d_name, "..", strlen(".."))) {
            continue;
        }

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
            if (NULL == (orcm_sensor_freq.ev_base = opal_start_progress_thread("freq", true))) {
                orcm_sensor_freq.ev_active = false;
                return;
            }
        }

        /* setup freq sampler */
        freq_sampler = OBJ_NEW(orcm_sensor_sampler_t);

        /* check if freq sample rate is provided for this*/
        if (mca_sensor_freq_component.sample_rate) {
            freq_sampler->rate.tv_sec = mca_sensor_freq_component.sample_rate;
        } else {
            freq_sampler->rate.tv_sec = orcm_sensor_base.sample_rate;
        }
        freq_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(orcm_sensor_freq.ev_base, &freq_sampler->ev,
                               perthread_freq_sample, freq_sampler);
        opal_event_evtimer_add(&freq_sampler->ev, &freq_sampler->rate);
    }
    return;
}


static void stop(orte_jobid_t jobid)
{
    if (orcm_sensor_freq.ev_active) {
        orcm_sensor_freq.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_stop_progress_thread("freq", false);
    }
    return;
}

static void freq_sample(orcm_sensor_sampler_t *sampler)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor freq : freq_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    if (!mca_sensor_freq_component.use_progress_thread) {
       collect_sample(sampler);
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
    collect_sample(sampler);
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

static void collect_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    corefreq_tracker_t *trk, *nxt;
    pstate_tracker_t *ptrk, *pnxt;

    FILE *fp;
    char *freq;
    float ghz;
    opal_buffer_t data, *bptr;
    int32_t ncores;
    time_t now;
    char time_str[40];
    char *timestamp_str;
    bool packed;
    struct tm *sample_time;
    unsigned int item_count = 0;

    if (0 == opal_list_get_size(&tracking)) {
        return;
    }

    opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                        "%s sampling freq",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* prep to store the results */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    packed = false;

    /* pack our name */
    freq = strdup("freq");
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &freq, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }
    free(freq);

    /* store our hostname */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* store the number of cores */
    ncores = (int32_t)opal_list_get_size(&tracking);
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &ncores, 1, OPAL_INT32))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* get the sample time */
    now = time(NULL);
    /* pass the time along as a simple string */
    sample_time = localtime(&now);
    if (NULL == sample_time) {
        ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
        return;
    }
    strftime(time_str, sizeof(time_str), "%F %T%z", sample_time);
    asprintf(&timestamp_str, "%s", time_str);
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &timestamp_str, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        free(timestamp_str);
        return;
    }
    free(timestamp_str);

    OPAL_LIST_FOREACH_SAFE(trk, nxt, &tracking, corefreq_tracker_t) {
        opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                            "%s processing freq file %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            trk->file);
        /* read the freq */
        if (NULL == (fp = fopen(trk->file, "r"))) {
            /* we can't be read, so remove it from the list */
            opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                "%s access denied to freq file %s - removing it",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                trk->file);
            opal_list_remove_item(&tracking, &trk->super);
            OBJ_RELEASE(trk);
            continue;
        }
        while (NULL != (freq = orte_getline(fp))) {
            ghz = strtoul(freq, NULL, 10) / 1000000.0;
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "%s sensor:freq: Core %d freq %f max %f min %f",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                trk->core, ghz, trk->max_freq, trk->min_freq);
            if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &ghz, 1, OPAL_FLOAT))) {
                ORTE_ERROR_LOG(ret);
                free(freq);
                fclose(fp);
                OBJ_DESTRUCT(&data);
                return;
            }
            packed = true;
            free(freq);
        }
        fclose(fp);
    }

    if(true == intel_pstate_avail) {
        item_count = opal_list_get_size(&pstate_list);
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &item_count, 1, OPAL_UINT))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
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
            if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &ptrk->sysname, 1, OPAL_STRING))) {
                    ORTE_ERROR_LOG(ret);
                    free(freq);
                    fclose(fp);
                    OBJ_DESTRUCT(&data);
                    return;
            }
            while (NULL != (freq = orte_getline(fp))) {
                ptrk->value = strtoul(freq, NULL, 10);
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                    "%s sensor:pstate: file %s : %d",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                    ptrk->file, ptrk->value);
                if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &ptrk->value, 1, OPAL_UINT))) {
                    ORTE_ERROR_LOG(ret);
                    free(freq);
                    fclose(fp);
                    OBJ_DESTRUCT(&data);
                    return;
                }
                packed = true;
                free(freq);
            }
            fclose(fp);
        }
    } else {
        /* Pack 0 pstate values available */
        item_count = 0;
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &item_count, 1, OPAL_UINT))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }
    }

    /* xfer the data for transmission */
    if (packed) {
        bptr = &data;
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }
    }
    OBJ_DESTRUCT(&data);
}

static void mycleanup(int dbhandle, int status,
                      opal_list_t *kvs, void *cbdata)
{
    OPAL_LIST_RELEASE(kvs);
    if (ORTE_SUCCESS != status) {
        log_enabled = false;
    }
}

static void freq_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    char *sampletime;
    struct tm time_info;
    time_t ts;
    int rc;
    int32_t n, ncores;
    opal_list_t *vals, *pstate_vals=NULL;
    opal_value_t *kv;
    float fval;
    int i;
    unsigned int pstate_count = 0, pstate_value = 0;
    char *pstate_name;

    if (!log_enabled) {
        return;
    }
    /* unpack the host this came from */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    /* and the number of cores on that host */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &ncores, &n, OPAL_INT32))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    /* sample time */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "%s Received freq log from host %s with %d cores",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname, ncores);

    /* xfr to storage */
    vals = OBJ_NEW(opal_list_t);

    /* load the sample time at the start */
    if (NULL == sampletime) {
        ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
        return;
    }
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("ctime");
    kv->type = OPAL_STRING;
    kv->data.string = strdup(sampletime);
    opal_list_append(vals, &kv->super);

    /* load the hostname */
    if (NULL == hostname) {
        ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
        return;
    }
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("hostname");
    kv->type = OPAL_STRING;
    kv->data.string = strdup(hostname);
    opal_list_append(vals, &kv->super);

    for (i=0; i < ncores; i++) {
        kv = OBJ_NEW(opal_value_t);
        asprintf(&kv->key, "core%d:GHz", i);
        kv->type = OPAL_FLOAT;
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &fval, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }

        /* check corefreq event policy */
        strptime(sampletime, "%F %T%z", &time_info);
        ts = mktime(&time_info);
        corefreq_policy_filter(hostname, i, fval, ts);

        kv->data.fval = fval;
        opal_list_append(vals, &kv->super);
    }

    /* store it */
    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store(orcm_sensor_base.dbhandle, "freq", vals, mycleanup, NULL);
    } else {
        OPAL_LIST_RELEASE(vals);
    }

    /* unpack the pstate entry count */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &pstate_count, &n, OPAL_UINT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                "Total pstate count: %d", pstate_count);

    if (pstate_count > 0) {
        /* xfr to storage */
        pstate_vals = OBJ_NEW(opal_list_t);

        /* load the sample time at the start */
        if (NULL == sampletime) {
            ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("ctime");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(sampletime);
        opal_list_append(pstate_vals, &kv->super);

        /* load the hostname */
        if (NULL == hostname) {
            ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("hostname");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(hostname);
        opal_list_append(pstate_vals, &kv->super);
    }

    while(pstate_count > 0)
    {
        /* unpack the pstate entry name */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &pstate_name, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        /* unpack the pstate entry value */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &pstate_value, &n, OPAL_UINT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "%s : %d",pstate_name, pstate_value);

        kv = OBJ_NEW(opal_value_t);
        if (0 != strcmp(pstate_name,"no_turbo")) {
            kv->key = strdup(pstate_name);
            kv->type = OPAL_UINT;
            kv->data.uint = pstate_value;
        } else {
            kv->key = strdup("allow_turbo");
            kv->type = OPAL_BOOL;
            kv->data.flag = ((0 == pstate_value) ? true: false);
        }
        opal_list_append(pstate_vals, &kv->super);
        pstate_count--;
    }

    if (pstate_vals != NULL)
    {
        /* store it */
        if (0 <= orcm_sensor_base.dbhandle) {
            orcm_db.store(orcm_sensor_base.dbhandle, "pstate", pstate_vals, mycleanup, NULL);
        } else {
            OPAL_LIST_RELEASE(pstate_vals);
        }
    }

 cleanup:
    if (NULL != hostname) {
        free(hostname);
    }

}

static void freq_set_sample_rate(int sample_rate)
{
    /* set the freq sample rate if seperate thread is enabled */
    if (mca_sensor_freq_component.use_progress_thread) {
        mca_sensor_freq_component.sample_rate = sample_rate;
    }
    return;
}

static void freq_get_sample_rate(int *sample_rate)
{
    if (NULL != sample_rate) {
    /* check if freq sample rate is provided for this*/
        if (mca_sensor_freq_component.use_progress_thread) {
            *sample_rate = mca_sensor_freq_component.sample_rate;
        }
    }
    return;
}
