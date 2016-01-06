/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * Additional copyrights may follow
 *
 * $HEADER$
 */

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
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/un.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"
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
#include "orcm/mca/db/db.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/analytics/analytics.h"
#include "orcm/util/utils.h"
#include "sensor_syslog.h"

#define MAXLEN 1024
#define INPUT_SOCKET "/dev/orcm_log"

/* declare the API functions */
static int  init(void);
static int  syslog_socket(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void syslog_sample(orcm_sensor_sampler_t *sampler);
static void perthread_syslog_sample(int fd, short args, void *cbdata);
static void collect_sample(orcm_sensor_sampler_t *sampler);
static void syslog_log(opal_buffer_t *buf);
static void syslog_set_sample_rate(int sample_rate);
static void syslog_get_sample_rate(int *sample_rate);
static void *syslog_listener(void *arg);

static opal_list_t msgQueue;

typedef struct{
    opal_list_item_t super;
    char *log;
}syslog_msg;

static void msg_con(syslog_msg *msg){
    msg->log = NULL;
}

static void msg_des(syslog_msg *msg){
    if (NULL != msg->log) {
        free(msg->log);
    }
}

OBJ_CLASS_INSTANCE(syslog_msg,
        opal_list_item_t,
        msg_con, msg_des);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_syslog_module = {
    init,
    finalize,
    start,
    stop,
    syslog_sample,
    syslog_log,
    NULL,
    NULL,
    syslog_set_sample_rate,
    syslog_get_sample_rate
};

pthread_t listener;

static __time_val _tv;
static orcm_sensor_sampler_t *syslog_sampler;
static orcm_sensor_syslog_t orcm_sensor_syslog;

static int init(void)
{
    /* we must be root to run */
    if (0 != geteuid()) {
        return ORTE_ERROR;
    }
    return ORCM_SUCCESS;
}

static void finalize(void)
{
    return;
}

/*
 start syslog plug-in
 */
static void start(orte_jobid_t jobid)
{
    OBJ_CONSTRUCT(&msgQueue, opal_list_t);

    gettimeofday(&(_tv.tv_curr), NULL);
    _tv.tv_prev=_tv.tv_curr;
    _tv.interval=0;

    /* Create a thread to catch all messages addressed by rsyslog */
    pthread_create(&listener, NULL, syslog_listener, NULL);

    /* start a separate syslog progress thread for sampling */
    if (mca_sensor_syslog_component.use_progress_thread) {
        if (!orcm_sensor_syslog.ev_active) {
            orcm_sensor_syslog.ev_active = true;
            if (NULL == (orcm_sensor_syslog.ev_base = opal_progress_thread_init("syslog"))) {
                orcm_sensor_syslog.ev_active = false;
                return;
            }
        }

        /* setup syslog sampler */
        syslog_sampler = OBJ_NEW(orcm_sensor_sampler_t);
        if (NULL == syslog_sampler) {
            ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
            return;
        }

        /* check if syslog sample rate is provided for this*/
        if (!mca_sensor_syslog_component.sample_rate) {
            mca_sensor_syslog_component.sample_rate = orcm_sensor_base.sample_rate;
        }

        syslog_sampler->rate.tv_sec = mca_sensor_syslog_component.sample_rate;
        syslog_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(orcm_sensor_syslog.ev_base, &syslog_sampler->ev,
                               perthread_syslog_sample, syslog_sampler);
        opal_event_evtimer_add(&syslog_sampler->ev, &syslog_sampler->rate);
    }
    return;
}

/*
 stop syslog plug-in
 */
static void stop(orte_jobid_t jobid)
{
    if (orcm_sensor_syslog.ev_active) {
        orcm_sensor_syslog.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_progress_thread_pause("syslog");
        if (NULL != syslog_sampler) {
            OBJ_RELEASE(syslog_sampler);
        }
    }
    OBJ_DESTRUCT(&msgQueue);
    return;
}

/*
 sampler syslog every X seconds
 */
static void syslog_sample(orcm_sensor_sampler_t *sampler)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor syslog : syslog_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    if (!mca_sensor_syslog_component.use_progress_thread) {
       collect_sample(sampler);
    }
}

static void perthread_syslog_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor syslog : perthread_syslog_sample: called",
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
    /* check if syslog sample rate is provided for this*/
    if (mca_sensor_syslog_component.sample_rate != sampler->rate.tv_sec) {
        sampler->rate.tv_sec = mca_sensor_syslog_component.sample_rate;
    }
    /* set ourselves to sample again */
    opal_event_evtimer_add(&sampler->ev, &sampler->rate);
}

static void collect_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    int nmsg;
    char *name;
    bool packed;
    syslog_msg *trk,*nxt;
    opal_buffer_t data, *bptr;
    struct timeval current_time;

    nmsg = opal_list_get_size(&msgQueue);
    if (0 == nmsg) {
        return;
    }

    /* prepare to store messages */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    packed = false;

    /* pack our name */
    name = strdup("syslog");
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &name, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        free(name);
        return;
    }
    free(name);

    /* store our hostname */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
     return;
    }

    /* store the number of messages */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &nmsg, 1, OPAL_INT32))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* get the sample time */
    gettimeofday(&current_time, NULL);

    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &current_time, 1, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }

    OPAL_LIST_FOREACH_SAFE(trk, nxt, &msgQueue, syslog_msg) {
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &trk->log, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
     }
     packed=true;
     opal_list_remove_item(&msgQueue, &trk->super);
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

static void syslog_log(opal_buffer_t *sample)
{
    int rc,i,n;
    int32_t nmsg;
    char tmp_log[32];
    char *log=NULL;
    char *hostname=NULL;
    bool error = false;
    struct timeval sampletime;
    orcm_value_t *sensor_metric = NULL;
    orcm_analytics_value_t *analytics_vals = NULL;
    opal_list_t *key = NULL;
    opal_list_t *non_compute_data = NULL;

    /* unpack the host this came from */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }

    /* and the number of messages recieved on that host */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &nmsg, &n, OPAL_INT32))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }

    /* sample time */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }

    key = OBJ_NEW(opal_list_t);
    if (NULL == key) {
        goto cleanup;
    }

    non_compute_data = OBJ_NEW(opal_list_t);
    if (NULL == non_compute_data) {
        goto cleanup;
    }

    /* load sample timestamp */
    sensor_metric = orcm_util_load_orcm_value("ctime", &sampletime, OPAL_TIMEVAL, NULL);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }
    opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

    /* load the hostname */
    if (NULL == hostname) {
        ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
        goto cleanup;
    }
    sensor_metric = orcm_util_load_orcm_value("hostname", hostname, OPAL_STRING, NULL);

    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    /* load the data_group */
    sensor_metric = orcm_util_load_orcm_value("data_group", "syslog", OPAL_STRING, NULL);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    /* load syslog message */
    for(i=0; i < nmsg; i++) {
        analytics_vals = orcm_util_load_orcm_analytics_value(key,non_compute_data, NULL);
        if ((NULL == analytics_vals) || (NULL == analytics_vals->key) ||
            (NULL == analytics_vals->non_compute_data) ||(NULL == analytics_vals->compute_data)) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            error = true;
            goto cleanup;
        }

        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &log, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            error = true;
            goto cleanup;
        } 

        if (0 > snprintf(tmp_log, sizeof(tmp_log), "log_message_%d", i)) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            error = true;
            goto cleanup;
        }

        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s syslog_log: %s = %s\n",tmp_log,log,
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

        sensor_metric = orcm_util_load_orcm_value(tmp_log, log, OPAL_STRING, "log");
        if (NULL == sensor_metric) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            error = true;
            goto cleanup;
        }

        opal_list_append(analytics_vals->compute_data, (opal_list_item_t *)sensor_metric);
        orcm_analytics.send_data(analytics_vals);
        OBJ_RELEASE(analytics_vals);
    }

cleanup:
    if (true == error) {
        if (NULL != analytics_vals) {
            OBJ_RELEASE(analytics_vals);
        }
    }
    SAFEFREE(hostname);
    SAFEFREE(log);
    if ( NULL != key) {
        OBJ_RELEASE(key);
    }
    if ( NULL != non_compute_data) {
       OBJ_RELEASE(non_compute_data);
    }
}

/**
 * Function for handling server side socket
 *
 * @returns FD of the socket to syslog, -1 in case of error
 */
static int syslog_socket(void)
{
    int len;
    static int s = -1;
    struct sockaddr_un sInfo;

    if (s > 0) {
        return s;
    }

    if (-1 == (s = socket(AF_UNIX, SOCK_DGRAM, 0))) {
        s = -1;
        return s;
    }

    sInfo.sun_family = AF_UNIX;
    strcpy(sInfo.sun_path, INPUT_SOCKET);
    unlink(sInfo.sun_path);

    len = strlen(sInfo.sun_path) + sizeof(sInfo.sun_family);
    if (-1 == bind(s, (struct sockaddr *)&sInfo, len)) {
        close(s);
        s = -1;
        return s;
    }
    return s;
}

/**
 * Dedicated thread to catch any syslog event
 *
 * @returns insert any gotten message into a queue
 */
static void *syslog_listener(void *arg)
{
    int n;
    syslog_msg *log;
    char msg[MAXLEN];

    while(1) {
        if (syslog_socket() < 0) {
            opal_output(0, "SYSLOG ERROR: Unable to open socket, sensor won't collect data\n");
            return (void*) -1;
     }

     n = recv(syslog_socket(), msg, MAXLEN, 0);
     if (n > 0) {
         log = OBJ_NEW(syslog_msg);
         log->log = strdup(msg);
         opal_list_append(&msgQueue,&log->super);
         memset(msg, 0, MAXLEN);
     }
    }
    return 0;
}

static void syslog_set_sample_rate(int sample_rate)
{
    /* set the syslog sample rate if seperate thread is enabled */
    if (mca_sensor_syslog_component.use_progress_thread) {
        mca_sensor_syslog_component.sample_rate = sample_rate;
    }
    return;
}

static void syslog_get_sample_rate(int *sample_rate)
{
    if (NULL != sample_rate) {
      /* check if syslog sample rate is provided for this*/
      if (mca_sensor_syslog_component.use_progress_thread) {
          *sample_rate = mca_sensor_syslog_component.sample_rate;
      }
    }
    return;
}
