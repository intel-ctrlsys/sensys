/*
 * Copyright (c) 2013-2016  Intel, Inc. All rights reserved.
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
#include "orcm/util/utils.h"
#include "orcm/runtime/orcm_globals.h"

#include "orcm/mca/analytics/analytics.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "sensor_coretemp.h"

#define TEST_CORES (256)

#define ON_FAILURE_RETURN(x) if(ORCM_SUCCESS!=x){ORTE_ERROR_LOG(x);return;}
#define ON_FAILURE_GOTO(x,label) if(ORCM_SUCCESS!=x){ORTE_ERROR_LOG(x);goto label;}
#define ON_NULL_RETURN(x) if(NULL==x){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);return;}
#define ON_NULL_GOTO(x,label) if(NULL==x){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);goto label;}
#define ORCM_RELEASE(x) if(NULL!=x){OBJ_RELEASE(x);x=NULL;}

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void coretemp_sample(orcm_sensor_sampler_t *sampler);
static void perthread_coretemp_sample(int fd, short args, void *cbdata);
void collect_coretemp_sample(orcm_sensor_sampler_t *sampler);
static void coretemp_log(opal_buffer_t *buf);
static void coretemp_set_sample_rate(int sample_rate);
static void coretemp_get_sample_rate(int *sample_rate);
static void coretemp_inventory_collect(opal_buffer_t *inventory_snapshot);
static void coretemp_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);
int coretemp_enable_sampling(const char* sensor_specification);
int coretemp_disable_sampling(const char* sensor_specification);
int coretemp_reset_sampling(const char* sensor_specification);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_coretemp_module = {
    init,
    finalize,
    start,
    stop,
    coretemp_sample,
    coretemp_log,
    coretemp_inventory_collect,
    coretemp_inventory_log,
    coretemp_set_sample_rate,
    coretemp_get_sample_rate,
    coretemp_enable_sampling,
    coretemp_disable_sampling,
    coretemp_reset_sampling
};

/****    CORETEMP EVENT HISTORY TYPE    ****/
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
} coretemp_history_t;
static void hst_con(coretemp_history_t *hst)
{
    hst->hostname = NULL;
    hst->core_no  = 0;
    hst->hi_thres = true;
    hst->severity = ORTE_NOTIFIER_INFO;
    hst->count    = 0;
    hst->tstamp   = 0;
}
static void hst_des(coretemp_history_t *hst)
{
    if (NULL != hst->hostname) {
        free(hst->hostname);
    }

}
OBJ_CLASS_INSTANCE(coretemp_history_t,
                   opal_list_item_t,
                   hst_con, hst_des);

typedef struct {
    opal_list_item_t super;
    char *file;
    int socket;
    int core;
    char *label;
    float critical_temp;
    float max_temp;
} coretemp_tracker_t;
static void ctr_con(coretemp_tracker_t *trk)
{
    trk->file = NULL;
    trk->label = NULL;
    trk->socket = -1;
    trk->core = -1;
}
static void ctr_des(coretemp_tracker_t *trk)
{
    if (NULL != trk->file) {
        free(trk->file);
    }
    if (NULL != trk->label) {
        free(trk->label);
    }
}
OBJ_CLASS_INSTANCE(coretemp_tracker_t,
                   opal_list_item_t,
                   ctr_con, ctr_des);

static opal_list_t tracking;
static opal_list_t event_history;
static orcm_sensor_sampler_t *coretemp_sampler = NULL;
static orcm_sensor_coretemp_t orcm_sensor_coretemp;

static void generate_test_vector(opal_buffer_t *v);
char **coretemp_policy_list; /* store coretemp policies from MCA parameter */

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

static int coretemp_load_policy(char *policy)
{
    char **tokens = NULL;
    int array_length = 0;
    orcm_sensor_policy_t *plc, *newplc;
    bool found_me;
    const char *sensor_name = "coretemp";
    const char *action = NULL;
    float threshold;
    bool hi_thres;
    int max_count, time_window;
    orte_notifier_severity_t sev;
    int ret;

    ret = ORCM_ERR_BAD_PARAM;
    tokens = opal_argv_split(policy, ':');
    array_length = opal_argv_count(tokens);

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

        action = tokens[5];

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
    opal_argv_free(tokens);
    return ret;
}

static int coretemp_policy_filter(char *hostname, int core_no, float ct, time_t ts)
{
    orcm_sensor_policy_t *plc;
    coretemp_history_t *hst, *newhst;
    bool found_me;
    const char *sev = NULL;
    char *msg;

    /* Check if this sample may be filtered
     * We have to check for all policies, one single sample might trigger
     * multiple events with different severity levels
     */
    OPAL_LIST_FOREACH(plc, &orcm_sensor_base.policy, orcm_sensor_policy_t) {
        /* check for coretemp sensor type */
        if ( 0 != strcmp(plc->sensor_name, "coretemp") ) {
            continue;
        }

        if ( (plc->hi_thres && (ct < plc->threshold) ) ||
             (!plc->hi_thres && (ct > plc->threshold) ) ) {
            continue;
        }

        sev = orte_notifier_base_sev2str(plc->severity);

        /* this sample should be accounted for this policy
         * we have a candidate, let's check there is similar sample accounted in history or not
         */
        found_me = false;
        OPAL_LIST_FOREACH(hst, &event_history, coretemp_history_t) {
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
                    asprintf(&msg, "host: %s core %d temperature %f °C, %s than or equal to threshold %f °C for %d times in %d seconds",
                                    hostname, core_no, ct, plc->hi_thres ? "higher" : "lower",
                                    plc->threshold, plc->max_count, plc->time_window);
                    ORTE_NOTIFIER_SYSTEM_EVENT(plc->severity, msg, plc->action);
                    opal_output(0, "host: %s core %d temperature %f °C, %s than or equal to threshold %f °C for %d times in %d seconds, trigger %s event!",
                                    hostname, core_no, ct, plc->hi_thres ? "higher" : "lower",
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
                asprintf(&msg, "host: %s core %d temperature %f °C, %s than or equal to threshold %f °C for %d times in %d seconds",
                                    hostname, core_no, ct, plc->hi_thres ? "higher" : "lower",
                                    plc->threshold, plc->max_count, plc->time_window);
                ORTE_NOTIFIER_SYSTEM_EVENT(plc->severity, msg, plc->action);
                opal_output(0, "host: %s Core %d temperature %f °C, %s than or equal to threshold %f °C for %d times in %d seconds, trigger %s event!",
                                    hostname, core_no, ct, plc->hi_thres ? "higher" : "lower",
                                    plc->threshold, plc->max_count, plc->time_window, sev);
            } else {
                newhst = OBJ_NEW(coretemp_history_t);
                newhst->hostname = strdup(hostname);
                newhst->core_no = core_no;
                newhst->hi_thres = plc->hi_thres;
                newhst->severity = plc->severity;
                newhst->count = 1;
                newhst->tstamp = ts;
                opal_list_append(&event_history, &newhst->super);
            }
        }
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
    DIR *cur_dirp = NULL, *tdir;
    struct dirent *dir_entry, *entry;
    char *dirname = NULL;
    char *filename, *ptr, *tmp;
    size_t tlen = strlen("temp");
    size_t ilen = strlen("_input");
    FILE *fp;
    coretemp_tracker_t *trk, *t2;
    bool inserted;
    opal_list_t foobar;
    int corecount = 0;
    int i = 0;
    int ret = 0;
    char *skt;

    mca_sensor_coretemp_component.diagnostics = 0;
    mca_sensor_coretemp_component.runtime_metrics =
        orcm_sensor_base_runtime_metrics_create("coretemp", orcm_sensor_base.collect_metrics,
                                                mca_sensor_coretemp_component.collect_metrics);

    /* always construct this so we don't segfault in finalize */
    OBJ_CONSTRUCT(&tracking, opal_list_t);
    OBJ_CONSTRUCT(&event_history, opal_list_t);

    /* get policy from MCA parameters */
    if( NULL != mca_sensor_coretemp_component.policy ) {
        coretemp_policy_list = opal_argv_split(mca_sensor_coretemp_component.policy, ',');
    }

    /* load policies */
    for(i =0; i <opal_argv_count(coretemp_policy_list); i++) {
        ret = coretemp_load_policy(coretemp_policy_list[i]);
        if ( ORCM_SUCCESS != ret ) {
            opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                "%s failed loading coretemp policy - %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                coretemp_policy_list[i]);
        }
    }
    opal_argv_free(coretemp_policy_list);

    /*
     * Open up the base directory so we can get a listing
     */
    if (NULL == (cur_dirp = opendir("/sys/bus/platform/devices"))) {
        OBJ_DESTRUCT(&tracking);
        orte_show_help("help-orcm-sensor-coretemp.txt", "req-dir-not-found",
                       true, orte_process_info.nodename,
                       "/sys/bus/platform/devices");
        return ORTE_ERROR;
    }

    /*
     * For each directory
     */
    while (NULL != (dir_entry = readdir(cur_dirp))) {

        /* look for coretemp directories */
        if (0 != strncmp(dir_entry->d_name, "coretemp", strlen("coretemp"))) {
            continue;
        }
        skt = strchr(dir_entry->d_name, '.');
        if (NULL != skt) {
            skt++;
        }

        /* open that directory */
        if (NULL == (dirname = opal_os_path(false, "/sys/bus/platform/devices", dir_entry->d_name, NULL ))) {
            continue;
        }
        if (NULL == (tdir = opendir(dirname))) {
            free(dirname);
            continue;
        }
        OBJ_CONSTRUCT(&foobar, opal_list_t);
        while (NULL != (entry = readdir(tdir))) {
            /*
             * Skip the obvious
             */
            if (NULL == entry->d_name) {
                continue;
            }

            if (0 == strncmp(entry->d_name, ".", strlen(".")) ||
                0 == strncmp(entry->d_name, "..", strlen(".."))) {
                continue;
            }
            if (strlen(entry->d_name) < (tlen+ilen)) {
                /* cannot be a core temp file */
                continue;
            }
            /*
             * See if this is a core temp file
             */
            if (0 != strncmp(entry->d_name, "temp", strlen("temp"))) {
                continue;
            }
            if (0 != strcmp(entry->d_name + strlen(entry->d_name) - ilen, "_input")) {
                continue;
            }
            /* track the info for this core */
            trk = OBJ_NEW(coretemp_tracker_t);
            if (NULL != skt) {
                trk->socket = strtol(skt, NULL, 10);
            }
            trk->file = opal_os_path(false, dirname, entry->d_name, NULL);
            /* take the part up to the first underscore as this will
             * be used as the start of all the related files
             */
            tmp = strdup(entry->d_name);
            if (NULL == (ptr = strchr(tmp, '_'))) {
                /* unrecognized format */
                free(tmp);
                OBJ_RELEASE(trk);
                continue;
            }
            *ptr = '\0';
            /* look for critical, max, and label info */
            asprintf(&filename, "%s/%s_%s", dirname, tmp, "label");
            if (NULL != (fp = fopen(filename, "r")))
            {
                if (NULL != (trk->label = orte_getline(fp)))
                {
                    fclose(fp);
                    free(filename);
                    if(NULL != (ptr = strcasestr(trk->label,"core"))) {
                        trk->core = strtol(trk->label+strlen("core "), NULL, 10); /* This stores the core ID under each processor*/
                    } else if (NULL != (ptr = strcasestr(trk->label,"Physical id "))) {
                        if (mca_sensor_coretemp_component.enable_packagetemp == true) {
                            trk->core = strtol(trk->label+strlen("Physical id "), NULL, 10); /* This stores the Package ID of each processor*/
                        } else {
                            free(tmp);
                            OBJ_RELEASE(trk);
                            continue;
                        }
                    } else {
                        free(tmp);
                        OBJ_RELEASE(trk);
                        continue; /* We only collect core temperatures for now*/
                    }
                } else {
                    ORTE_ERROR_LOG(ORTE_ERR_FILE_READ_FAILURE);
                    fclose(fp);
                    free(filename);
                    free(tmp);
                    OBJ_RELEASE(trk);
                    continue;
                }
            } else {
                ORTE_ERROR_LOG(ORTE_ERR_FILE_OPEN_FAILURE);
                free(filename);
                free(tmp);
                OBJ_RELEASE(trk);
                    continue;
            }
            asprintf(&filename, "%s/%s_%s", dirname, tmp, "crit");
            if (NULL != (fp = fopen(filename, "r")))
            {
                if (NULL != (ptr = orte_getline(fp)))
                {
                    trk->critical_temp = strtol(ptr, NULL, 10)/1000.0;
                    free(ptr);
                    fclose(fp);
                    free(filename);
                } else {
                    ORTE_ERROR_LOG(ORTE_ERR_FILE_READ_FAILURE);
                    fclose(fp);
                    free(filename);
                    free(tmp);
                    OBJ_RELEASE(trk);
                    continue;
                }
            } else {
                ORTE_ERROR_LOG(ORTE_ERR_FILE_OPEN_FAILURE);
                free(filename);
                free(tmp);
                OBJ_RELEASE(trk);
                continue;
            }

            asprintf(&filename, "%s/%s_%s", dirname, tmp, "max");
            if (NULL != (fp = fopen(filename, "r")))
            {
                if (NULL != (ptr = orte_getline(fp)))
                {
                    trk->max_temp = strtol(ptr, NULL, 10)/1000.0;
                    free(ptr);
                    fclose(fp);
                    free(filename);
                } else {
                    ORTE_ERROR_LOG(ORTE_ERR_FILE_READ_FAILURE);
                    fclose(fp);
                    free(filename);
                    free(tmp);
                    OBJ_RELEASE(trk);
                    continue;
                }
            } else {
                ORTE_ERROR_LOG(ORTE_ERR_FILE_OPEN_FAILURE);
                free(filename);
                OBJ_RELEASE(trk);
                free(tmp);
                continue;
            }

            /* add to our list, in core order */
            inserted = false;
            OPAL_LIST_FOREACH(t2, &foobar, coretemp_tracker_t) {
                if (NULL != strcasestr(trk->label,"core")) {
                    if (t2->core > trk->core) {
                        opal_list_insert_pos(&foobar, &t2->super, &trk->super);
                        inserted = true;
                        break;
                    }
                }
            }
            if (!inserted) {
                opal_list_append(&foobar, &trk->super);
            }
            /* cleanup */
            free(tmp);
        }
        free(dirname);
        closedir(tdir);
        /* add the ordered list to our collection */
        while (NULL != (t2 = (coretemp_tracker_t*)opal_list_remove_first(&foobar))) {
            if(NULL != strcasestr(t2->label,"core")) {
                t2->core = corecount++;
                sprintf(t2->label, "core%d", t2->core);
            }
            opal_list_append(&tracking, &t2->super);
        }
        OPAL_LIST_DESTRUCT(&foobar);
    }
    closedir(cur_dirp);

    if (0 == opal_list_get_size(&tracking)) {
        /* nothing to read */
        orte_show_help("help-orcm-sensor-coretemp.txt", "no-cores-found",
                       true, orte_process_info.nodename);
        return ORTE_ERROR;
    }

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_LIST_DESTRUCT(&tracking);
    OPAL_LIST_DESTRUCT(&event_history);

    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_coretemp_component.runtime_metrics);
    mca_sensor_coretemp_component.runtime_metrics = NULL;
}

/*
 * Start monitoring of local temps
 */
static void start(orte_jobid_t jobid)
{
    /* start a separate coretemp progress thread for sampling */
    if (mca_sensor_coretemp_component.use_progress_thread) {
        if (!orcm_sensor_coretemp.ev_active) {
            orcm_sensor_coretemp.ev_active = true;
            if (NULL == (orcm_sensor_coretemp.ev_base = opal_progress_thread_init("coretemp"))) {
                orcm_sensor_coretemp.ev_active = false;
                return;
            }
        }

        /* setup coretemp sampler */
        coretemp_sampler = OBJ_NEW(orcm_sensor_sampler_t);

        /* check if coretemp sample rate is provided for this*/
        if (!mca_sensor_coretemp_component.sample_rate) {
            mca_sensor_coretemp_component.sample_rate = orcm_sensor_base.sample_rate;
        }

        coretemp_sampler->rate.tv_sec = mca_sensor_coretemp_component.sample_rate;
        coretemp_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(orcm_sensor_coretemp.ev_base, &coretemp_sampler->ev,
                               perthread_coretemp_sample, coretemp_sampler);
        opal_event_evtimer_add(&coretemp_sampler->ev, &coretemp_sampler->rate);
    }else{
	 mca_sensor_coretemp_component.sample_rate = orcm_sensor_base.sample_rate;

    }

    return;
}


static void stop(orte_jobid_t jobid)
{
    if (orcm_sensor_coretemp.ev_active) {
        orcm_sensor_coretemp.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_progress_thread_pause("coretemp");
        OBJ_RELEASE(coretemp_sampler);
    }
    return;
}

static void coretemp_sample(orcm_sensor_sampler_t *sampler)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor coretemp : coretemp_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    if (!mca_sensor_coretemp_component.use_progress_thread) {
       collect_coretemp_sample(sampler);
    }

}

static void perthread_coretemp_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor coretemp : perthread_coretemp_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* this has fired in the sampler thread, so we are okay to
     * just go ahead and sample since we do NOT allow both the
     * base thread and the component thread to both be actively
     * calling this component */
    collect_coretemp_sample(sampler);
    /* we now need to push the results into the base event thread
     * so it can add the data to the base bucket */
    ORCM_SENSOR_XFER(&sampler->bucket);
    /* clear the bucket */
    OBJ_DESTRUCT(&sampler->bucket);
    OBJ_CONSTRUCT(&sampler->bucket, opal_buffer_t);
    /* check if coretemp sample rate is provided for this*/
    if (mca_sensor_coretemp_component.sample_rate != sampler->rate.tv_sec) {
        sampler->rate.tv_sec = mca_sensor_coretemp_component.sample_rate;
    }
    /* set ourselves to sample again */
    opal_event_evtimer_add(&sampler->ev, &sampler->rate);
}

void collect_coretemp_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    coretemp_tracker_t *trk, *nxt;
    FILE *fp;
    const char *data_group = "coretemp";
    char *temp = NULL;
    float degc;
    opal_buffer_t data, *bptr;
    int32_t ncores;
    bool packed;
    struct timeval current_time;
    void* metrics_obj = mca_sensor_coretemp_component.runtime_metrics;

    if (mca_sensor_coretemp_component.test) {
        /* generate and send the test vector */
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
                            "%s sensor coretemp : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    mca_sensor_coretemp_component.diagnostics |= 0x1;

    if (0 == opal_list_get_size(&tracking)) {
        return;
    }

    /* prep to store the results */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    packed = false;
    orcm_sensor_base_runtime_metrics_begin(mca_sensor_coretemp_component.runtime_metrics);

    /* pack our name */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &data_group, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        goto ct_sample_exit;
    }

    /* store our hostname */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        goto ct_sample_exit;
    }

    /* get the sample time */
    gettimeofday(&current_time, NULL);

    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &current_time, 1, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(ret);
        goto ct_sample_exit;
    }

    /* Store number of labels to collect */
    if(orcm_sensor_base_runtime_inventory_available(mca_sensor_coretemp_component.runtime_metrics)) {
        ncores = (int32_t)orcm_sensor_base_runtime_metrics_active_label_count(mca_sensor_coretemp_component.runtime_metrics);
    } else {
        ncores = (int32_t)opal_list_get_size(&tracking);
    }
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &ncores, 1, OPAL_INT32))) {
        ORTE_ERROR_LOG(ret);
        goto ct_sample_exit;
    }

    OPAL_LIST_FOREACH_SAFE(trk, nxt, &tracking, coretemp_tracker_t) {
        /* read the temp */
        if (NULL == (fp = fopen(trk->file, "r"))) {
            /* we can't be read, so remove it from the list */
            opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                "%s access denied to coretemp file %s - removing it",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                trk->file);
            opal_list_remove_item(&tracking, &trk->super);
            OBJ_RELEASE(trk);
            continue;
        }
        if (orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_coretemp_component.runtime_metrics, trk->label)) {
            if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &trk->label, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                fclose(fp);
                goto ct_sample_exit;
            }
        }
        while (NULL != (temp = orte_getline(fp))) {
            degc = strtoul(temp, NULL, 10) / 1000.0;
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "%s sensor:coretemp: Core %d in Socket %d temp %f max %f critical %f",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                trk->core, trk->socket, degc, trk->max_temp, trk->critical_temp);
            if (orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_coretemp_component.runtime_metrics, trk->label)) {
                if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &degc, 1, OPAL_FLOAT))) {
                    ORTE_ERROR_LOG(ret);
                    free(temp);
                    fclose(fp);
                    goto ct_sample_exit;
                }
                packed = true;
            }
            free(temp);
            /* check for exceed critical temp */
            if (trk->critical_temp < degc) {
                /* alert the errmgr - this is a critical problem */
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                    "%s sensor:coretemp: Core %d (socket %d) CRITICAL",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                    trk->core, trk->socket);
             } else if (trk->max_temp < degc) {
                /* alert the errmgr */
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                    "%s sensor:coretemp: Core %d (socket %d) MAX",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                    trk->core, trk->socket);
             }
        }
        fclose(fp);
    }

    /* xfer the data for transmission */
    if (packed) {
        bptr = &data;
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER))) {
            ORTE_ERROR_LOG(ret);
        }
    }

ct_sample_exit:
    orcm_sensor_base_runtime_metrics_end(mca_sensor_coretemp_component.runtime_metrics);
    OBJ_DESTRUCT(&data);
}

static void coretemp_log_cleanup(char *core_label, char *hostname, opal_list_t *key,
                                 opal_list_t *non_compute_data,
                                 orcm_analytics_value_t *analytics_vals)
{
    SAFEFREE(core_label);
    SAFEFREE(hostname);
    SAFE_RELEASE(key);
    SAFE_RELEASE(non_compute_data);
    SAFE_RELEASE(analytics_vals);
}

static void coretemp_log(opal_buffer_t *sample)
{
    char *hostname = NULL;
    struct timeval sampletime;
    int rc = ORCM_SUCCESS;
    int32_t n = 0;
    int32_t ncores = 0;
    orcm_analytics_value_t *analytics_vals = NULL;
    opal_list_t *key = NULL;
    opal_list_t *non_compute_data = NULL;
    float fval = 0.0;
    int idx = 0;
    char *core_label = NULL;

    /* unpack the hostname from where the data sample came */
    n = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
        return;
    }

    /* unpack the sample time when the data was sampled */
    n = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
        return;
    }

    /* unpack the number of cores on that host */
    n = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &ncores, &n, OPAL_INT32))) {
        ORTE_ERROR_LOG(rc);
        coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
        return;
    }

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "%s Received log from host %s with %d cores",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname, ncores);

    /* fill the key list with hostname and data_group */
    if (NULL == (key = OBJ_NEW(opal_list_t))) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
        return;
    }
    rc = orcm_util_append_orcm_value(key, "hostname", hostname, OPAL_STRING, NULL);
    if (ORCM_SUCCESS != rc) {
        ORTE_ERROR_LOG(rc);
        coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
        return;
    }
    rc = orcm_util_append_orcm_value(key, "data_group", "coretemp", OPAL_STRING, NULL);
    if (ORCM_SUCCESS != rc) {
        ORTE_ERROR_LOG(rc);
        coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
        return;
    }

    /* fill non compute data list with time stamp */
    if (NULL == (non_compute_data = OBJ_NEW(opal_list_t))) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
        return;
    }
    rc = orcm_util_append_orcm_value(non_compute_data, "ctime", &sampletime, OPAL_TIMEVAL, NULL);
    if (ORCM_SUCCESS != rc) {
        ORTE_ERROR_LOG(rc);
        coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
        return;
    }

    /* fill the compute data with the coretemp of all cores */
    analytics_vals = orcm_util_load_orcm_analytics_value(key, non_compute_data, NULL);
    if (NULL == analytics_vals || NULL == analytics_vals->key ||
        NULL == analytics_vals->non_compute_data || NULL == analytics_vals->compute_data) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
        return;
    }
    for (idx = 0; idx < ncores; idx++) {
        n = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &core_label, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
            return;
        }
        n = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &fval, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
            return;
        }
        if(ORCM_SUCCESS != (rc = orcm_util_append_orcm_value(analytics_vals->compute_data,
                            core_label, &fval, OPAL_FLOAT, "degrees C"))) {
            ORTE_ERROR_LOG(rc);
            coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
            return;
        }
        SAFEFREE(core_label);
        /* check coretemp event policy */
        coretemp_policy_filter(hostname, idx, fval, sampletime.tv_sec);
    }

    orcm_analytics.send_data(analytics_vals);
    coretemp_log_cleanup(core_label, hostname, key, non_compute_data, analytics_vals);
}

static void coretemp_set_sample_rate(int sample_rate)
{
    /* set the coretemp sample rate if seperate thread is enabled */
    if (mca_sensor_coretemp_component.use_progress_thread) {
        mca_sensor_coretemp_component.sample_rate = sample_rate;
    }
    return;
}

static void coretemp_get_sample_rate(int *sample_rate)
{
    if (NULL != sample_rate) {
    /* check if coretemp sample rate is provided for this*/
            *sample_rate = mca_sensor_coretemp_component.sample_rate;
    }
    return;
}

static void generate_test_vector(opal_buffer_t *v)
{
    int ret;
    const char *ctmp = "coretemp";
    char *corelabel = NULL;
    int32_t ncores = TEST_CORES + 2;
    int32_t i;
    float degc = 23.0;
    struct timeval sample_time;

/* pack the plugin name */
    ret = opal_dss.pack(v, &ctmp, 1, OPAL_STRING);
    ON_FAILURE_RETURN(ret);

/* pack the hostname */
    ret = opal_dss.pack(v, &orte_process_info.nodename, 1, OPAL_STRING);
    ON_FAILURE_RETURN(ret);

/* get the sample time */
    gettimeofday(&sample_time, NULL);
    ret = opal_dss.pack(v, &sample_time, 1, OPAL_TIMEVAL);
    ON_FAILURE_RETURN(ret);

/* pack then number of cores */
    ret = opal_dss.pack(v, &ncores, 1, OPAL_INT32);
    ON_FAILURE_RETURN(ret);

/* Pack test core readings */
    for (i = 0; i < ncores; ++i) {
        if(TEST_CORES <= i) {
            asprintf(&corelabel,"package%d", i - TEST_CORES);
        } else {
            asprintf(&corelabel,"core%d", i);
        }
        ON_NULL_RETURN(corelabel);
        ret = opal_dss.pack(v, &corelabel, 1, OPAL_STRING);
        SAFEFREE(corelabel);
        ON_FAILURE_RETURN(ret);

        ret = opal_dss.pack(v, &degc, 1, OPAL_FLOAT);
        ON_FAILURE_RETURN(ret);
        degc += 0.05;
    }

    opal_output_verbose(5,orcm_sensor_base_framework.framework_output,
        "%s sensor:coretemp: Size of test vector is %d",
        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ncores);
}

static void generate_test_inv_data(opal_buffer_t *inventory_snapshot)
{ /* 2 Packages; 2048 cores */
    struct timeval sample_time;
    unsigned int tot_items = TEST_CORES + 2;
    const char *ctemp = "coretemp";
    char *comp = NULL;
    int rc = OPAL_SUCCESS;
    unsigned int i = 0;

    rc = opal_dss.pack(inventory_snapshot, &ctemp, 1, OPAL_STRING);
    ON_FAILURE_RETURN(rc);
    rc = opal_dss.pack(inventory_snapshot, &orte_process_info.nodename, 1, OPAL_STRING);
    ON_FAILURE_RETURN(rc);
    gettimeofday(&sample_time, NULL);
    rc = opal_dss.pack(inventory_snapshot, &sample_time, 1, OPAL_TIMEVAL);
    ON_FAILURE_RETURN(rc);
    rc = opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT);
    ON_FAILURE_RETURN(rc);
    for(i = 0; tot_items > i; ++i) {
        asprintf(&comp, "sensor_coretemp_%d", i + 1);
        ON_NULL_RETURN(comp);
        rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
        SAFEFREE(comp);
        ON_FAILURE_RETURN(rc);
        if(TEST_CORES <= i) {
            asprintf(&comp, "package%d", i - TEST_CORES);
        } else {
            asprintf(&comp, "core%d", i);
        }
        ON_NULL_RETURN(comp);
        rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
        SAFEFREE(comp);
        ON_FAILURE_RETURN(rc);
    }
}

static void coretemp_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    if (mca_sensor_coretemp_component.test) {
        /* generate test vector */
        generate_test_inv_data(inventory_snapshot);
    } else {
        struct timeval sample_time;
        unsigned int tot_items = 0;
        const char *ctemp = "coretemp";
        char *comp = NULL;
        coretemp_tracker_t* trk = NULL;
        int sensor_number = 1;
        int rc = OPAL_SUCCESS;

        rc = opal_dss.pack(inventory_snapshot, &ctemp, 1, OPAL_STRING);
        ON_FAILURE_RETURN(rc);

        rc = opal_dss.pack(inventory_snapshot, &orte_process_info.nodename, 1, OPAL_STRING);
        ON_FAILURE_RETURN(rc);

        gettimeofday(&sample_time, NULL);
        rc = opal_dss.pack(inventory_snapshot, &sample_time, 1, OPAL_TIMEVAL);
        ON_FAILURE_RETURN(rc);

        tot_items = (unsigned int)opal_list_get_size(&tracking);
        rc = opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT);
        ON_FAILURE_RETURN(rc);

        OPAL_LIST_FOREACH(trk, &tracking, coretemp_tracker_t) {
            asprintf(&comp, "sensor_coretemp_%d", sensor_number);
            ON_NULL_RETURN(comp);
            rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
            SAFEFREE(comp);
            ON_FAILURE_RETURN(rc);
            orcm_sensor_base_runtime_metrics_track(mca_sensor_coretemp_component.runtime_metrics, trk->label);

            rc = opal_dss.pack(inventory_snapshot, &trk->label, 1, OPAL_STRING);
            ON_FAILURE_RETURN(rc);

            ++sensor_number;
        }
    }
}

static void my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata)
{
    OBJ_RELEASE(kvs);
}

static void coretemp_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
{
    unsigned int tot_items = 0;
    int n = 1;
    opal_list_t *records = NULL;
    int rc = OPAL_SUCCESS;
    orcm_value_t *time_stamp = NULL;
    orcm_value_t *host_name=NULL;
    struct timeval current_time;
    char *tmp = NULL;
    orcm_value_t *mkv = NULL;
    char *inv = NULL;
    char *inv_val = NULL;

    n=1;
    rc = opal_dss.unpack(inventory_snapshot, &tmp, &n, OPAL_STRING);
    ON_FAILURE_RETURN(rc);

    n=1;
    rc = opal_dss.unpack(inventory_snapshot, &current_time, &n, OPAL_TIMEVAL);
    ON_FAILURE_RETURN(rc);

    n=1;
    rc = opal_dss.unpack(inventory_snapshot, &tot_items, &n, OPAL_UINT);
    ON_FAILURE_RETURN(rc);

    time_stamp = orcm_util_load_orcm_value("ctime", &current_time, OPAL_TIMEVAL, NULL);
    ON_NULL_RETURN(time_stamp);

    host_name = orcm_util_load_orcm_value("hostname", hostname, OPAL_STRING, NULL);
    ON_NULL_RETURN(host_name);

    records = OBJ_NEW(opal_list_t);
    ON_NULL_GOTO(records, cleanup);

    opal_list_append(records, (opal_list_item_t*)time_stamp);
    opal_list_append(records, (opal_list_item_t*)host_name);
    time_stamp = NULL;
    host_name=NULL;
    for(; tot_items > 0; --tot_items) {
        n=1;
        rc = opal_dss.unpack(inventory_snapshot, &inv, &n, OPAL_STRING);
        ON_FAILURE_GOTO(rc, cleanup);

        n=1;
        rc = opal_dss.unpack(inventory_snapshot, &inv_val, &n, OPAL_STRING);
        ON_FAILURE_GOTO(rc, cleanup);

        mkv = orcm_util_load_orcm_value(inv, inv_val, OPAL_STRING, NULL);
        ON_NULL_GOTO(mkv, cleanup);
        inv = NULL;
        inv_val = NULL;
        opal_list_append(records, (opal_list_item_t*)mkv);
        mkv = NULL;
    }
    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA, records, NULL, my_inventory_log_cleanup, NULL);
    } else {
        my_inventory_log_cleanup(-1, -1, records, NULL, NULL);
    }
    records = NULL;

cleanup:
    ORCM_RELEASE(mkv);
    ORCM_RELEASE(time_stamp);
    ORCM_RELEASE(records);
    SAFEFREE(inv);
    SAFEFREE(inv_val);
}

int coretemp_enable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_coretemp_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, true, sensor_specification);
}

int coretemp_disable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_coretemp_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, false, sensor_specification);
}

int coretemp_reset_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_coretemp_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_reset(metrics, sensor_specification);
}
