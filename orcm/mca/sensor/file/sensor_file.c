/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2011-2012 Los Alamos National Security, LLC.
 *                         All rights reserved.
 * Copyright (c) 2013-2016 Intel, Inc. All rights reserved.
 *
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
#include <stddef.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>

#include "opal_stdint.h"
#include "opal/util/output.h"
#include "opal/runtime/opal_progress_threads.h"

#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/state/state.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "sensor_file.h"

#define ON_NULL_REPORT_ERROR_AND_RETURN(p,e) if(NULL==p){ORTE_ERROR_LOG(e);return;}
#define BREAK_ON_NULL(x) if(NULL==x){break;}

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void perthread_file_sample(int fd, short args, void *cbdata);
static void file_activate_job_state(orte_job_t *jdata, int flag);
void collect_file_sample(orcm_sensor_sampler_t *sampler);
int file_enable_sampling(const char* sensor_specification);
int file_disable_sampling(const char* sensor_specification);
int file_reset_sampling(const char* sensor_specification);


/* This typedef and function pointer is required to keep UT tests from causing
 * a segault.  This redirection allows the tests to subclass this private
 * function "file_activate_job_state".
 */
static void file_sample(orcm_sensor_sampler_t *sampler);
typedef void (*test_file_activate_job_state_fn_t)(orte_job_t*, int);
test_file_activate_job_state_fn_t test_file_activate_job_state = file_activate_job_state;


/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_file_module = {
    init,
    finalize,
    start,
    stop,
    file_sample,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    file_enable_sampling,
    file_disable_sampling,
    file_reset_sampling
};

/* define a tracking object */
typedef struct {
    opal_list_item_t super;
    orte_jobid_t jobid;
    orte_vpid_t vpid;
    char *file;
    int tick;
    bool check_size;
    bool check_access;
    bool check_mod;
    int32_t file_size;
    time_t last_access;
    time_t last_mod;
    int limit;
} file_tracker_t;
static void ft_constructor(file_tracker_t *ft)
{
    ft->file = NULL;
    ft->tick = 0;
    ft->file_size = 0;
    ft->last_access = 0;
    ft->last_mod = 0;
    ft->limit = 0;
}
static void ft_destructor(file_tracker_t *ft)
{
    SAFEFREE(ft->file);
}
OBJ_CLASS_INSTANCE(file_tracker_t,
                   opal_list_item_t,
                   ft_constructor, ft_destructor);

/* local globals */
static opal_list_t jobs;
static orcm_sensor_sampler_t *file_sampler = NULL;
static orcm_sensor_file_t orcm_sensor_file;

static int init(void)
{
    mca_sensor_file_component.diagnostics = 0;
    mca_sensor_file_component.runtime_metrics =
        orcm_sensor_base_runtime_metrics_create("file", orcm_sensor_base.collect_metrics,
                                                mca_sensor_file_component.collect_metrics);

    OBJ_CONSTRUCT(&jobs, opal_list_t);
    return ORCM_SUCCESS;
}

static void finalize(void)
{


    while (true) {
        opal_list_item_t *item = opal_list_remove_first(&jobs);
        BREAK_ON_NULL(item);
        OBJ_RELEASE(item);
    }
    OBJ_DESTRUCT(&jobs);

    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_file_component.runtime_metrics);
    mca_sensor_file_component.runtime_metrics = NULL;

    return;
}

static bool find_value(orte_app_context_t *app,
                       char *pattern, char **value)
{
    for (int i=0; NULL != app->env[i]; i++) {
        if (0 == strncmp(app->env[i], pattern, strlen(pattern))) {
            char* ptr = strchr(app->env[i], '=');
            ORCM_ON_NULL_RETURN_ERROR(ptr, true);
            ptr++;
            ORCM_ON_NULL_RETURN_ERROR(value, true);
            *value = strdup(ptr);
            return true;
        }
    }
    return false;
}

/*
 * Start monitoring of local processes
 */
static void start(orte_jobid_t jobid)
{
    orte_job_t *jobdat;
    orte_app_context_t *app, *aptr;
    int i;
    char *filename;
    file_tracker_t *ft = NULL;
    char *ptr;

    /* cannot monitor my own job */
    if (jobid == ORTE_PROC_MY_NAME->jobid && ORTE_JOBID_WILDCARD != jobid) {
        return;
    }
    OPAL_OUTPUT_VERBOSE((1, orcm_sensor_base_framework.framework_output,
                         "%s starting file monitoring for job %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_JOBID_PRINT(jobid)));

    /* get the local jobdat for this job */
    jobdat = orte_get_job_data_object(jobid);
    ON_NULL_REPORT_ERROR_AND_RETURN(jobdat, ORTE_ERR_NOT_FOUND);

    /* must be at least one app_context, so use the first one found */
    app = NULL;
    for (i=0; i < jobdat->apps->size; i++) {
        if (NULL != (aptr = (orte_app_context_t*)opal_pointer_array_get_item(jobdat->apps, i))) {
            app = aptr;
            break;
        }
    }
    if (NULL == app) {
        /* got a problem */
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        return;
    }
    /* search the environ to get the filename */
    if (!find_value(app, OPAL_MCA_PREFIX"sensor_file_filename", &filename)) {
        /* was a default file given */
        if (NULL == mca_sensor_file_component.file) {
            /* can't do anything without a file */
            OPAL_OUTPUT_VERBOSE((1, orcm_sensor_base_framework.framework_output,
                                 "%s sensor:file no file for job %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 ORTE_JOBID_PRINT(jobid)));
            return;
        }
        filename = strdup(mca_sensor_file_component.file);
    }

    ORCM_ON_NULL_RETURN(filename);

    /* create the tracking object */
    ft = OBJ_NEW(file_tracker_t);
    ft->jobid = jobid;
    ft->file = strdup(filename);
    SAFEFREE(filename);

    /* search the environ to see what we are checking */
    if (!find_value(app, OPAL_MCA_PREFIX"sensor_file_check_size", &ptr)) {
        /* was a default value given */
        if (0 < mca_sensor_file_component.check_size) {
            ft->check_size = OPAL_INT_TO_BOOL(mca_sensor_file_component.check_size);
        }
    } else {
        ORCM_ON_NULL_GOTO(ptr, cleanup);
        ft->check_size = OPAL_INT_TO_BOOL(strtol(ptr, NULL, 10));
        free(ptr);
    }


    if (!find_value(app, OPAL_MCA_PREFIX"sensor_file_check_access", &ptr)) {
        /* was a default value given */
        if (0 < mca_sensor_file_component.check_access) {
            ft->check_access = OPAL_INT_TO_BOOL(mca_sensor_file_component.check_access);
        }
    } else {
        ORCM_ON_NULL_GOTO(ptr, cleanup);
        ft->check_access = OPAL_INT_TO_BOOL(strtol(ptr, NULL, 10));
        free(ptr);
    }

    if (!find_value(app, OPAL_MCA_PREFIX"sensor_file_check_mod", &ptr)) {
        /* was a default value given */
        if (0 < mca_sensor_file_component.check_mod) {
            ft->check_mod = OPAL_INT_TO_BOOL(mca_sensor_file_component.check_mod);
        }
    } else {
        ORCM_ON_NULL_GOTO(ptr, cleanup);
        ft->check_mod = OPAL_INT_TO_BOOL(strtol(ptr, NULL, 10));
        free(ptr);
    }

    if (!find_value(app, OPAL_MCA_PREFIX"sensor_file_limit", &ptr)) {
        ft->limit = mca_sensor_file_component.limit;
    } else {
        ORCM_ON_NULL_GOTO(ptr, cleanup);
        ft->limit = strtol(ptr, NULL, 10);
        free(ptr);
    }
    OPAL_OUTPUT_VERBOSE((1, orcm_sensor_base_framework.framework_output,
                         "%s file %s monitored for %s%s%s with limit %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ft->file, ft->check_size ? "SIZE:" : " ",
                         ft->check_access ? "ACCESS TIME:" : " ",
                         ft->check_mod ? "MOD TIME" : " ", ft->limit));
    opal_list_append(&jobs, &ft->super);
    ft = NULL; /* Now owned by the list */

    /* start a separate file progress thread for sampling */
    if (mca_sensor_file_component.use_progress_thread) {
        if (!orcm_sensor_file.ev_active) {
            orcm_sensor_file.ev_active = true;
            if (NULL == (orcm_sensor_file.ev_base = opal_progress_thread_init("file"))) {
                orcm_sensor_file.ev_active = false;
                goto cleanup;
            }
        }

        /* setup file sampler */
        file_sampler = OBJ_NEW(orcm_sensor_sampler_t);
        ORCM_ON_NULL_GOTO(file_sampler, cleanup);

        /* check if file sample rate is provided for this*/
        if (mca_sensor_file_component.sample_rate) {
            file_sampler->rate.tv_sec = mca_sensor_file_component.sample_rate;
        } else {
            file_sampler->rate.tv_sec = orcm_sensor_base.sample_rate;
        }
        file_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(orcm_sensor_file.ev_base, &file_sampler->ev,
                               perthread_file_sample, file_sampler);
        opal_event_evtimer_add(&file_sampler->ev, &file_sampler->rate);
    }

cleanup:
    ORCM_RELEASE(ft);
    return;
}


static void stop(orte_jobid_t jobid)
{
    opal_list_item_t *item;
    file_tracker_t *ft;

    /* cannot monitor my own job */
    if (jobid == ORTE_PROC_MY_NAME->jobid && ORTE_JOBID_WILDCARD != jobid) {
        return;
    }

    for (item = opal_list_get_first(&jobs);
        ((item != opal_list_get_end(&jobs)) && (NULL != item));
        item = opal_list_get_next(item)) {
        ft = (file_tracker_t*)item;
        if (jobid == ft->jobid || ORTE_JOBID_WILDCARD == jobid) {
            opal_list_remove_item(&jobs, item);
            OBJ_RELEASE(item);
        }
    }

    if (orcm_sensor_file.ev_active) {
        orcm_sensor_file.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_progress_thread_pause("file");
    }
    return;
}

static void file_sample(orcm_sensor_sampler_t *sampler)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor file : file_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    if (!mca_sensor_file_component.use_progress_thread) {
       collect_file_sample(sampler);
    }

}

static void perthread_file_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor file : perthread_file_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* this has fired in the sampler thread, so we are okay to
     * just go ahead and sample since we do NOT allow both the
     * base thread and the component thread to both be actively
     * calling this component */
    collect_file_sample(sampler);
    /* we now need to push the results into the base event thread
     * so it can add the data to the base bucket */
    ORCM_SENSOR_XFER(&sampler->bucket);
    /* clear the bucket */
    OBJ_DESTRUCT(&sampler->bucket);
    OBJ_CONSTRUCT(&sampler->bucket, opal_buffer_t);
    /* set ourselves to sample again */
    opal_event_evtimer_add(&sampler->ev, &sampler->rate);
}

void collect_file_sample(orcm_sensor_sampler_t *sampler)
{
    struct stat buf;
    opal_list_item_t *item;
    file_tracker_t *ft;
    orte_job_t *jdata;
    void* metrics_obj = mca_sensor_file_component.runtime_metrics;
    if(!orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, NULL)) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor file : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    mca_sensor_file_component.diagnostics |= 0x1;

    OPAL_OUTPUT_VERBOSE((1, orcm_sensor_base_framework.framework_output,
                         "%s sampling files",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    for (item = opal_list_get_first(&jobs);
        ((item != opal_list_get_end(&jobs)) && (NULL != item));
        item = opal_list_get_next(item)) {
        ft = (file_tracker_t*)item;
        /* stat the file and get its size */
        if (0 > stat(ft->file, &buf)) {
            /* cannot stat file */
            OPAL_OUTPUT_VERBOSE((1, orcm_sensor_base_framework.framework_output,
                                 "%s could not stat %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 ft->file));
            continue;
        }

        OPAL_OUTPUT_VERBOSE((1, orcm_sensor_base_framework.framework_output,
                             "%s size %lu access %s\tmod %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             (unsigned long)buf.st_size, ctime(&buf.st_atime), ctime(&buf.st_mtime)));

        if (ft->check_size) {
            if (buf.st_size == ft->file_size) {
                ft->tick++;
                goto CHECK;
            } else {
                ft->tick = 0;
                ft->file_size = buf.st_size;
            }
        }
        if (ft->check_access) {
            if (buf.st_atime == ft->last_access) {
                ft->tick++;
                goto CHECK;
            } else {
                ft->tick = 0;
                ft->last_access = buf.st_atime;
            }
        }
        if (ft->check_mod) {
            if (buf.st_mtime == ft->last_mod) {
                ft->tick++;
                goto CHECK;
            } else {
                ft->tick = 0;
                ft->last_mod = buf.st_mtime;
            }
        }

    CHECK:
        OPAL_OUTPUT_VERBOSE((1, orcm_sensor_base_framework.framework_output,
                             "%s sampled file %s tick %d",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ft->file, ft->tick));

        if (ft->tick == ft->limit) {
            orte_show_help("help-orcm-sensor-file.txt", "file-stalled", true,
                           ft->file, ft->file_size, ctime(&ft->last_access), ctime(&ft->last_mod));
            jdata = orte_get_job_data_object(ft->jobid);
            /* Under UT testing this function ALWAYS segfaults so this is a
               function pointer so the UT can "subclass" the
               "file_activate_job_state" function to remove the segfault. */
            test_file_activate_job_state(jdata, ORTE_JOB_STATE_SENSOR_BOUND_EXCEEDED);
        }
    }
}

static void file_activate_job_state(orte_job_t *jdata, int flag)
{
printf("*** DEBUG: activate_job_state called!\n");
    ORCM_ON_NULL_RETURN(jdata);
    ORTE_ACTIVATE_JOB_STATE(jdata, flag);
}

int file_enable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_file_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, true, sensor_specification);
}

int file_disable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_file_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, false, sensor_specification);
}

int file_reset_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_file_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_reset(metrics, sensor_specification);
}
