/*
 * Copyright (c) 2015-2016  Intel Corporation. All rights reserved.
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
#define _GNU_SOURCE
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
#include <regex.h>
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
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/analytics/analytics.h"
#include "orcm/util/utils.h"
#include "sensor_syslog.h"

#define MAXLEN 1024
#define INPUT_SOCKET "/dev/orcm_log"
#define IF_NULL_ERROR(x) if(NULL == x) { ORTE_ERROR_LOG(ORCM_ERROR); return; }
#define ON_FAILURE_GOTO(code,target) if(OPAL_SUCCESS!=code) { ORTE_ERROR_LOG(code); goto target; }
#define ON_FAILURE_RETURN(code) if(OPAL_SUCCESS!=code) { ORTE_ERROR_LOG(code); return; }
#define ON_NULL_GOTO(pointer,target) if(NULL==pointer) { ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE); goto target; }
#define ON_NULL_RETURN(pointer) if(NULL==pointer) { ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE); return; }
#define ORCM_RELEASE(x) if(NULL!=x){OBJ_RELEASE(x);x=NULL;}
#define ORCM_FREE(x) if(NULL!=x){free(x); x=NULL;}

#define USE_SEVERITY (0x01)
#define USE_LOG      (0x02)

/* declare the API functions */
static int  init(void);
static int  syslog_socket(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void syslog_sample(orcm_sensor_sampler_t *sampler);
void perthread_syslog_sample(int fd, short args, void *cbdata);
void collect_syslog_sample(orcm_sensor_sampler_t *sampler);
static void syslog_log(opal_buffer_t *buf);
static void syslog_set_sample_rate(int sample_rate);
static void syslog_get_sample_rate(int *sample_rate);
void syslog_listener(int fd, short flags, void *arg);
int syslog_enable_sampling(const char* sensor_specification);
int syslog_disable_sampling(const char* sensor_specification);
int syslog_reset_sampling(const char* sensor_specification);
const char *syslog_get_facility(int prival);
const char *syslog_get_severity(int  prival);
static char* make_test_syslog_message(time_t t);
bool orcm_sensor_syslog_generate_test_vector(opal_buffer_t* buffer);
static void syslog_inventory_collect(opal_buffer_t *inventory_snapshot);
static void syslog_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);
static void my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata);

static opal_list_t msgQueue;
static bool stop_thread = true;

typedef struct{
    opal_list_item_t super;
    char *log;
}syslog_msg;

#define NUM_FACILITIES 24
char *facility_msg[NUM_FACILITIES] = {
    "KERNEL MESSAGES",
    "USER-LEVEL MESSAGES",
    "MAIL SYSTEM",
    "SYSTEM DAEMONS",
    "SECURITY/AUTHORIZATION MESSAGES",
    "MESSAGES GENERATED INTERNALLY BY SYSLOGD",
    "LINE PRINTER SUBSYSTEM",
    "NETWORK NEWS SUBSYSTEM",
    "UUCP SUBSYSTEM",
    "CLOCK DAEMON",
    "SECURITY/AUTHORIZATION MESSAGES",
    "FTP DAEMON",
    "NTP SUBSYSTEM",
    "LOG AUDIT",
    "LOG ALERT",
    "CLOCK DAEMON",
    "LOCAL USE 0",
    "LOCAL USE 1",
    "LOCAL USE 2",
    "LOCAL USE 3",
    "LOCAL USE 4",
    "LOCAL USE 5",
    "LOCAL USE 6",
    "LOCAL USE 7"
};

#define NUM_PRIORITIES 8
char *priority_names[NUM_PRIORITIES] = {
    "emerg", "alert", "crit", "error",
    "warn", "notice", "info", "debug"
};

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
    syslog_inventory_collect,
    syslog_inventory_log,
    syslog_set_sample_rate,
    syslog_get_sample_rate,
    syslog_enable_sampling,
    syslog_disable_sampling,
    syslog_reset_sampling
};

pthread_t listener;

static __time_val _tv;
static orcm_sensor_sampler_t *syslog_sampler;
static orcm_sensor_syslog_t orcm_sensor_syslog;


static int init(void)
{
    /* we must be root to run */
    if (0 != geteuid()) {
        return ORCM_ERR_PERM;
    }

    mca_sensor_syslog_component.diagnostics = 0;
    mca_sensor_syslog_component.runtime_metrics =
        orcm_sensor_base_runtime_metrics_create("syslog", orcm_sensor_base.collect_metrics,
                                                mca_sensor_syslog_component.collect_metrics);
    if(NULL == mca_sensor_syslog_component.runtime_metrics) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_syslog_component.runtime_metrics);
    mca_sensor_syslog_component.runtime_metrics = NULL;
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
    if(!mca_sensor_syslog_component.test) {
        syslog_socket_handler = opal_event_new(orte_event_base,
                                               syslog_socket(),
                                               EV_READ|EV_PERSIST,
                                               syslog_listener,
                                               NULL);
        ON_NULL_GOTO(syslog_socket_handler, cleanup);
        opal_event_add(syslog_socket_handler, NULL);
    }

    /* start a separate syslog progress thread for sampling */
    if (mca_sensor_syslog_component.use_progress_thread) {
        if (!orcm_sensor_syslog.ev_active) {
            orcm_sensor_syslog.ev_base = opal_progress_thread_init("syslog");
            ON_NULL_GOTO(orcm_sensor_syslog.ev_base, cleanup);
            orcm_sensor_syslog.ev_active = true;
        }

        /* setup syslog sampler */
        syslog_sampler = OBJ_NEW(orcm_sensor_sampler_t);
        ON_NULL_GOTO(syslog_sampler, cleanup);

        /* check if syslog sample rate is provided for this*/
        if (!mca_sensor_syslog_component.sample_rate) {
            mca_sensor_syslog_component.sample_rate = orcm_sensor_base.sample_rate;
        }

        syslog_sampler->rate.tv_sec = mca_sensor_syslog_component.sample_rate;
        syslog_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(orcm_sensor_syslog.ev_base, &syslog_sampler->ev,
                               perthread_syslog_sample, syslog_sampler);
        opal_event_evtimer_add(&syslog_sampler->ev, &syslog_sampler->rate);
    } else {
        mca_sensor_syslog_component.sample_rate = orcm_sensor_base.sample_rate;
    }

cleanup:
    return;
}

/*
 stop syslog plug-in
 */
static void stop(orte_jobid_t jobid)
{
    stop_thread = true;
    if (orcm_sensor_syslog.ev_active) {
        orcm_sensor_syslog.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_progress_thread_pause("syslog");
        ORCM_RELEASE(syslog_sampler);
    }
    opal_event_del(syslog_socket_handler);
    close(syslog_socket());
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
       collect_syslog_sample(sampler);
    }
}

void perthread_syslog_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor syslog : perthread_syslog_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* this has fired in the sampler thread, so we are okay to
     * just go ahead and sample since we do NOT allow both the
     * base thread and the component thread to both be actively
     * calling this component */
    collect_syslog_sample(sampler);
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

const char *syslog_get_severity(int  prival)
{
    int severity_code;
    const char *severity = NULL;
    if(0 > prival) {
        return severity;
    }
    severity_code = prival & 0x07;
    severity = priority_names[severity_code];
    return severity;
}

const char *syslog_get_facility(int prival)
{
    int facility_code;
    const char *facility = NULL;
    if(0 > prival) {
        return facility;
    }
    facility_code = (prival & 0xF8) >> 3;
    if (NUM_FACILITIES > facility_code) {
        facility = facility_msg[facility_code];
    }
    return facility;
}

void collect_syslog_sample(orcm_sensor_sampler_t *sampler)
{
    int ret = OPAL_SUCCESS;
    int nmsg;
    const char *name = "syslog";
    bool packed;
    syslog_msg *trk,*nxt;
    opal_buffer_t data, *bptr;
    struct timeval current_time;
    const char *severity = NULL;
    const char *facility = NULL;
    char *message = NULL;
    char *log_msg = NULL;
    char *tmp = NULL;
    regex_t regex_comp_log;
    size_t log_parts = 5;
    regmatch_t log_matches[log_parts];
    int prival_int;
    char *prival_str = NULL;
    int regex_res;

    if(mca_sensor_syslog_component.test) {
        opal_buffer_t buffer;
        OBJ_CONSTRUCT(&buffer, opal_buffer_t);
        if(orcm_sensor_syslog_generate_test_vector(&buffer)) {
            bptr = &buffer;
            ret = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
        }
        OBJ_DESTRUCT(&buffer);
        ON_FAILURE_RETURN(ret);
        return;
    }

    /* prepare to store messages */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    packed = false;

    void* metrics_obj = mca_sensor_syslog_component.runtime_metrics;
    ON_NULL_GOTO(metrics_obj, cleanup);
    if(!orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, NULL)) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor syslog : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        goto cleanup;
    }
    mca_sensor_syslog_component.diagnostics |= 0x1;

    nmsg = opal_list_get_size(&msgQueue);
    if (0 == nmsg) {
        goto cleanup;
    }

    /* pack our name */
    ret = opal_dss.pack(&data, &name, 1, OPAL_STRING);
    ON_FAILURE_GOTO(ret, cleanup);

    /* store our hostname */
    ret = opal_dss.pack(&data, &orcm_sensor_base.host_tag_value, 1, OPAL_STRING);
    ON_FAILURE_GOTO(ret, cleanup);

    /* store the number of messages */
    ret = opal_dss.pack(&data, &nmsg, 1, OPAL_INT32);
    ON_FAILURE_GOTO(ret, cleanup);

    /* get the sample time */
    gettimeofday(&current_time, NULL);
    ret = opal_dss.pack(&data, &current_time, 1, OPAL_TIMEVAL);
    ON_FAILURE_GOTO(ret, cleanup);

    uint8_t bits = 0;
    if(orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, "severity")) {
        bits |= USE_SEVERITY;
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, "log_message")) {
        bits |= USE_LOG;
    }
    ret = opal_dss.pack(&data, &bits, 1, OPAL_UINT8);
    ON_FAILURE_GOTO(ret, cleanup);

    /* Split log message into severity, facility and message */
    regcomp(&regex_comp_log, "<([0-9]+)>(.*)", REG_EXTENDED);

    OPAL_LIST_FOREACH_SAFE(trk, nxt, &msgQueue, syslog_msg) {
        regex_res = regexec(&regex_comp_log, trk->log, log_parts, log_matches,0);
        if (!regex_res) {
            prival_str = strndup(&trk->log[log_matches[1].rm_so],
                                 (int)(log_matches[1].rm_eo - log_matches[1].rm_so));
            ON_NULL_GOTO(prival_str, cleanup);

            prival_int = strtol(prival_str, &tmp, 10);
            free(prival_str);
            ON_NULL_GOTO(tmp, cleanup);

            severity = syslog_get_severity(prival_int);
            ON_NULL_GOTO(severity, cleanup);

            facility = syslog_get_facility(prival_int);
            ON_NULL_GOTO(facility, cleanup);

            message = strndup(&trk->log[log_matches[2].rm_so],
                              (int)(log_matches[2].rm_eo - log_matches[2].rm_so));
            ON_NULL_GOTO(message, cleanup);

            asprintf(&log_msg, "%s: %s", facility, message);
            ON_NULL_GOTO(log_msg, cleanup);


            ret = opal_dss.pack(&data, &severity, 1, OPAL_STRING);
            ON_FAILURE_GOTO(ret, cleanup);

            ret = opal_dss.pack(&data, &log_msg, 1, OPAL_STRING);
            ON_FAILURE_GOTO(ret, cleanup);

            packed=true;
            opal_list_remove_item(&msgQueue, &trk->super);

            ORCM_FREE(message);
            ORCM_FREE(log_msg);
        }
    }

    /* xfer the data for transmission */
    if (packed) {
        bptr = &data;
        ret = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
        ON_FAILURE_GOTO(ret, cleanup);
    }

cleanup:
    OBJ_DESTRUCT(&data);
    ORCM_FREE(message);
    ORCM_FREE(log_msg);
}

static void syslog_log(opal_buffer_t *sample)
{
    int rc,i,n;
    int32_t nmsg;
    char *log = NULL;
    char *severity = NULL;
    char *hostname = NULL;
    struct timeval sampletime;
    orcm_value_t *sensor_metric = NULL;
    orcm_analytics_value_t *analytics_vals = NULL;
    opal_list_t *key = NULL;
    opal_list_t *non_compute_data = NULL;

    /* unpack the host this came from */
    n=1;
    rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING);
    ON_FAILURE_GOTO(rc,cleanup);
    ON_NULL_GOTO(hostname, cleanup);

    /* and the number of messages recieved on that host */
    n=1;
    rc = opal_dss.unpack(sample, &nmsg, &n, OPAL_INT32);
    ON_FAILURE_GOTO(rc, cleanup);
    /* sample time */
    n=1;
    rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_TIMEVAL);
    ON_FAILURE_GOTO(rc, cleanup);

    n=1;
    uint8_t flags = 0;
    rc = opal_dss.unpack(sample, &flags, &n, OPAL_UINT8);
    ON_FAILURE_GOTO(rc, cleanup);

    key = OBJ_NEW(opal_list_t);
    ON_NULL_GOTO(key,cleanup);

    non_compute_data = OBJ_NEW(opal_list_t);
    ON_NULL_GOTO(non_compute_data, cleanup);

    /* load sample timestamp */
    sensor_metric = orcm_util_load_orcm_value("ctime", &sampletime, OPAL_TIMEVAL, NULL);
    ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

    /* load the hostname */
    sensor_metric = orcm_util_load_orcm_value("hostname", hostname, OPAL_STRING, NULL);
    ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    /* load the data_group */
    sensor_metric = orcm_util_load_orcm_value("data_group", "syslog", OPAL_STRING, NULL);
    ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    /* load syslog message */
    for(i=0; i < nmsg; i++) {
        analytics_vals = orcm_util_load_orcm_analytics_value(key,non_compute_data, NULL);
        ON_NULL_GOTO(analytics_vals, cleanup);
        ON_NULL_GOTO(analytics_vals->key, cleanup);
        ON_NULL_GOTO(analytics_vals->non_compute_data, cleanup);
        ON_NULL_GOTO(analytics_vals->compute_data, cleanup);

        n=1;
        if(USE_SEVERITY == (flags & USE_SEVERITY)) {
            rc = opal_dss.unpack(sample, &severity, &n, OPAL_STRING);
            ON_FAILURE_GOTO(rc, cleanup);
        }
        n=1;
        if(USE_LOG == (flags & USE_LOG)) {
            rc = opal_dss.unpack(sample, &log, &n, OPAL_STRING);
            ON_FAILURE_GOTO(rc, cleanup);
        }
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "log_message syslog_log: %s = %s\n",log,
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

        if(USE_SEVERITY == (flags & USE_SEVERITY)) {
            sensor_metric = orcm_util_load_orcm_value("severity", severity, OPAL_STRING, "sev");
            ON_NULL_GOTO(sensor_metric, cleanup);
            SAFEFREE(severity);
            opal_list_append(analytics_vals->compute_data, (opal_list_item_t *)sensor_metric);
        }

        if(USE_LOG == (flags & USE_LOG)) {
            sensor_metric = orcm_util_load_orcm_value("log_message", log, OPAL_STRING, "log");
            ON_NULL_GOTO(sensor_metric, cleanup);
            SAFEFREE(log);
            opal_list_append(analytics_vals->compute_data, (opal_list_item_t *)sensor_metric);
        }

        orcm_analytics.send_data(analytics_vals);

        ORCM_RELEASE(analytics_vals);
    }

cleanup:
    ORCM_RELEASE(analytics_vals);
    SAFEFREE(hostname);
    SAFEFREE(log);
    SAFEFREE(severity);
    ORCM_RELEASE(key);
    ORCM_RELEASE(non_compute_data);
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
 * Dedicated callback to catch any syslog event
 *
 * @returns insert any gotten message into a queue
 */
void syslog_listener(int fd, short flags, void *arg)
{
    int n = 0;
    int socket;
    syslog_msg *log;
    char msg[MAXLEN];

    socket = syslog_socket();
    if (0 > socket) {
        opal_output(0, "SYSLOG ERROR: Unable to open socket, sensor won't collect data\n");
        return;
    }
    memset(msg, 0, MAXLEN);
    n = recv(socket, msg, MAXLEN, 0);
    if ( n > 0 ) {
        log = OBJ_NEW(syslog_msg);
        log->log = strdup(msg);
        opal_list_append(&msgQueue, &log->super);
    }
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
    ON_NULL_RETURN(sample_rate);
    *sample_rate = mca_sensor_syslog_component.sample_rate;
    return;
}

int syslog_enable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_syslog_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, true, sensor_specification);
}

int syslog_disable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_syslog_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, false, sensor_specification);
}

int syslog_reset_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_syslog_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_reset(metrics, sensor_specification);
}

static char* make_test_syslog_message(time_t t)
{
    static const char* syslog_time_test_msg_fmt = "%FT%T%z";
    static const char* syslog_test_msg_fmt = "<30>1 %s host1 snmpd 23611 - - \
    Connection from UDP: [127.0.0.1]:58374->[127.0.0.1]";
    static const int MAX_TIME_LEN = 32;
    char stime[MAX_TIME_LEN];
    char* result = NULL;
    struct tm ttm;

    localtime_r(&t, &ttm);
    memset(stime, 0, MAX_TIME_LEN);
    if(0 >= strftime(stime, MAX_TIME_LEN, syslog_time_test_msg_fmt, &ttm)) {
        return NULL;
    }
    stime[MAX_TIME_LEN - 1] = '\0';
    asprintf(&result, syslog_test_msg_fmt, stime);
    return result;
}

#define LOCAL_ON_FAIL_CLEANUP(x) if(ORCM_SUCCESS!=x){ORTE_ERROR_LOG(x);result=false;goto cleanup;}
#define LOCAL_ON_NULL_CLEANUP(x) if(NULL==x){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);result=false;goto cleanup;}
bool orcm_sensor_syslog_generate_test_vector(opal_buffer_t* buffer)
{
    char* name = "syslog";
    int ret = ORCM_SUCCESS;
    int32_t sample_count = 1;
    bool result = true;
    struct current_time;
    char* msg = NULL;
    regex_t regex_comp_log;
    int regex_res;
    size_t log_parts = 5;
    regmatch_t log_matches[log_parts];
    int prival_int = 0;
    char *prival_str = NULL;
    const char *severity = NULL;
    const char *facility = NULL;
    char *message = NULL;
    char *log_msg = NULL;
    struct timeval current_time;

    ret = opal_dss.pack(buffer, &name, 1, OPAL_STRING);
    LOCAL_ON_FAIL_CLEANUP(ret);

    /* store our hostname */
    ret = opal_dss.pack(buffer, &orcm_sensor_base.host_tag_value, 1, OPAL_STRING);
    LOCAL_ON_FAIL_CLEANUP(ret);

    /* store the number of messages */
    ret = opal_dss.pack(buffer, &sample_count, 1, OPAL_INT32);
    LOCAL_ON_FAIL_CLEANUP(ret);

    /* get the sample time */
    gettimeofday(&current_time, NULL);
    ret = opal_dss.pack(buffer, &current_time, 1, OPAL_TIMEVAL);
    LOCAL_ON_FAIL_CLEANUP(ret);

    uint8_t include_labels = 0; //USE_SEVERITY | USE_LOG;
    void* rtm = mca_sensor_syslog_component.runtime_metrics;
    include_labels |= orcm_sensor_base_runtime_metrics_do_collect(rtm, "severity")?USE_SEVERITY:0;
    include_labels |= orcm_sensor_base_runtime_metrics_do_collect(rtm, "log_message")?USE_LOG:0;

    ret = opal_dss.pack(buffer, &include_labels, 1, OPAL_UINT8);
    ON_FAILURE_GOTO(ret, cleanup);

    msg = make_test_syslog_message(current_time.tv_sec);
    LOCAL_ON_NULL_CLEANUP(msg);

    regcomp(&regex_comp_log, "<([0-9]+)>(.*)", REG_EXTENDED);
    regex_res = regexec(&regex_comp_log, msg, log_parts, log_matches, 0);
    if (!regex_res) {
        char *tmp = NULL;
        prival_str = strndup(&msg[log_matches[1].rm_so],
                             (int)(log_matches[1].rm_eo - log_matches[1].rm_so));
        LOCAL_ON_NULL_CLEANUP(prival_str);

        prival_int = strtol(prival_str, &tmp, 10);
        SAFEFREE(prival_str);
        LOCAL_ON_NULL_CLEANUP(tmp);

        severity = syslog_get_severity(prival_int);
        LOCAL_ON_NULL_CLEANUP(severity);

        facility = syslog_get_facility(prival_int);
        LOCAL_ON_NULL_CLEANUP(facility);

        message = strndup(&msg[log_matches[2].rm_so],
                          (int)(log_matches[2].rm_eo - log_matches[2].rm_so));
        LOCAL_ON_NULL_CLEANUP(message);

        asprintf(&log_msg, "%s: %s", facility, message);
        LOCAL_ON_NULL_CLEANUP(log_msg);


        ret = opal_dss.pack(buffer, &severity, 1, OPAL_STRING);
        ON_FAILURE_GOTO(ret, cleanup);

        ret = opal_dss.pack(buffer, &log_msg, 1, OPAL_STRING);
        LOCAL_ON_FAIL_CLEANUP(ret);
    } else {
        result = false;
    }

cleanup:
    SAFEFREE(prival_str);
    SAFEFREE(msg);
    SAFEFREE(message);
    SAFEFREE(log_msg);
    return result;
}
#undef LOCAL_ON_FAIL_CLEANUP
#undef LOCAL_ON_NULL_CLEANUP

static void syslog_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    struct timeval sample_time;
    const char *ctemp = "syslog";
    char *comp = NULL;
    int rc = OPAL_SUCCESS;

    rc = opal_dss.pack(inventory_snapshot, &ctemp, 1, OPAL_STRING);
    ON_FAILURE_RETURN(rc);

    rc = opal_dss.pack(inventory_snapshot, &orcm_sensor_base.host_tag_value, 1, OPAL_STRING);
    ON_FAILURE_RETURN(rc);

    gettimeofday(&sample_time, NULL);
    rc = opal_dss.pack(inventory_snapshot, &sample_time, 1, OPAL_TIMEVAL);
    ON_FAILURE_RETURN(rc);

    comp = "log_message";
    orcm_sensor_base_runtime_metrics_track(mca_sensor_syslog_component.runtime_metrics, comp);
    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ON_FAILURE_RETURN(rc);

    comp = "severity";
    orcm_sensor_base_runtime_metrics_track(mca_sensor_syslog_component.runtime_metrics, comp);
    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ON_FAILURE_RETURN(rc);
}

static void my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata)
{
    OBJ_RELEASE(kvs);
}

static void syslog_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
{
    int n = 1;
    opal_list_t *records = NULL;
    int rc = OPAL_SUCCESS;
    orcm_value_t *time_stamp = NULL;
    struct timeval current_time;
    char *tmp = NULL;
    orcm_value_t* host = NULL;
    orcm_value_t* items[2] = { NULL, NULL };

    n=1;
    rc = opal_dss.unpack(inventory_snapshot, &tmp, &n, OPAL_STRING);
    ON_FAILURE_RETURN(rc);
    SAFEFREE(tmp);

    n=1;
    rc = opal_dss.unpack(inventory_snapshot, &current_time, &n, OPAL_TIMEVAL);
    ON_FAILURE_RETURN(rc);

    time_stamp = orcm_util_load_orcm_value("ctime", &current_time, OPAL_TIMEVAL, NULL);
    ON_NULL_RETURN(time_stamp);

    host = orcm_util_load_orcm_value("hostname", hostname, OPAL_STRING, NULL);
    ON_NULL_RETURN(host);

    records = OBJ_NEW(opal_list_t);
    ON_NULL_GOTO(records, cleanup);

    opal_list_append(records, (opal_list_item_t*)time_stamp);
    opal_list_append(records, (opal_list_item_t*)host);
    time_stamp = NULL;
    host = NULL;

    n=1;
    rc = opal_dss.unpack(inventory_snapshot, &tmp, &n, OPAL_STRING);
    ON_FAILURE_GOTO(rc, cleanup);
    items[0] = orcm_util_load_orcm_value("sensor_syslog_1", tmp, OPAL_STRING, NULL);
    SAFEFREE(tmp);
    ON_NULL_GOTO(items[0], cleanup);
    opal_list_append(records, (opal_list_item_t*)items[0]);
    items[0] = NULL;

    n=1;
    rc = opal_dss.unpack(inventory_snapshot, &tmp, &n, OPAL_STRING);
    ON_FAILURE_GOTO(rc, cleanup);
    items[1] = orcm_util_load_orcm_value("sensor_syslog_2", tmp, OPAL_STRING, NULL);
    SAFEFREE(tmp);
    ON_NULL_GOTO(items[1], cleanup);
    opal_list_append(records, (opal_list_item_t*)items[1]);
    items[1] = NULL;

    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA, records, NULL, my_inventory_log_cleanup, NULL);
        records = NULL;
    }

cleanup:
    ORCM_RELEASE(items[0]);
    ORCM_RELEASE(items[1]);
    ORCM_RELEASE(time_stamp);
    ORCM_RELEASE(host);
    ORCM_RELEASE(records);
}
