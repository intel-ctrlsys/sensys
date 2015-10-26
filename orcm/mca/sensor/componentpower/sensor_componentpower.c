/*
 * Copyright (c) 2014-2015  Intel, Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
#include "orcm/util/utils.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/analytics/analytics.h"


#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_componentpower.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void componentpower_sample(orcm_sensor_sampler_t *sampler);
static void perthread_componentpower_sample(int fd, short args, void *cbdata);
static void collect_sample(orcm_sensor_sampler_t *sampler);
static void componentpower_log(opal_buffer_t *buf);
static int detect_num_sockets(void);
static int detect_num_cpus(void);
static void detect_cpu_for_each_socket(void);
static void componentpower_set_sample_rate(int sample_rate);
static void componentpower_get_sample_rate(int *sample_rate);
static void componentpower_inventory_collect(opal_buffer_t *inventory_snapshot);
static void componentpower_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_componentpower_module = {
    init,
    finalize,
    start,
    stop,
    componentpower_sample,
    componentpower_log,
    componentpower_inventory_collect,
    componentpower_inventory_log,
    componentpower_set_sample_rate,
    componentpower_get_sample_rate
};

static __time_val _tv;
static __rapl _rapl;

static orcm_sensor_sampler_t *componentpower_sampler = NULL;
static orcm_sensor_componentpower_t orcm_sensor_componentpower;

static void generate_test_vector(opal_buffer_t *v);

static int init(void)
{
    int n_cpus, n_sockets, i, ret;
    char path[STR_LEN];
    unsigned long long msr, msr1, msr2;

    /* we must be root to run */
    if (0 != geteuid()) {
        return ORTE_ERROR;
    }

    _rapl.dev_msr_support=1;
    _rapl.cpu_rapl_support=1;
    _rapl.ddr_rapl_support=1;
    _rapl.rapl_calls=0;

    n_cpus=detect_num_cpus();

    n_sockets=detect_num_sockets();
    if (n_sockets > MAX_SOCKETS){
        n_sockets=MAX_SOCKETS;
    }

    _rapl.n_cpus=n_cpus;
    _rapl.n_sockets=n_sockets;

    detect_cpu_for_each_socket();

    for (i=0; i<_rapl.n_sockets; i++){
        _rapl.cpu_rapl[i]=_rapl.cpu_rapl_prev[i]=0;
        _rapl.ddr_rapl[i]=_rapl.ddr_rapl_prev[i]=0;
        snprintf(path, sizeof(path), "/dev/cpu/%d/msr", _rapl.cpu_idx[i]);
        _rapl.fd_cpu[i]=open(path, O_RDWR);
        if (_rapl.fd_cpu[i]<0){
            opal_output(0, "ERROR: opening msr file\n");
            _rapl.dev_msr_support=0;
            _rapl.cpu_rapl_support=0;
            _rapl.ddr_rapl_support=0;
            return ORTE_ERROR;
        }
    }

/* get lock bit*/
    lseek(_rapl.fd_cpu[0], RAPL_POWER_LIMIT, 0);
    ret=read(_rapl.fd_cpu[0], &msr, sizeof(unsigned long long));
    if (ret!=sizeof(unsigned long long)){
        opal_output(0, "ERROR: reading msr file\n");
        return ORTE_ERROR;
    }
    msr=msr&0x80000000;
    if (msr){
        opal_output(0, "ERROR: RAPL locked\n");
        _rapl.dev_msr_support=0;
        _rapl.cpu_rapl_support=0;
        _rapl.ddr_rapl_support=0;
        return ORTE_ERROR;
    }

/* get energy unit */
    lseek(_rapl.fd_cpu[0], RAPL_UNIT, 0);
    ret=read(_rapl.fd_cpu[0], &msr, sizeof(unsigned long long));
    if (ret!=sizeof(unsigned long long)){
        opal_output(0, "ERROR: reading msr file\n");
        return ORTE_ERROR;
    }

    msr=(msr>>8)&0x1f;
    if (!msr){
        opal_output(0, "ERROR: RAPL UNIT register read error\n");
        _rapl.cpu_rapl_support=0;
        _rapl.ddr_rapl_support=0;
        return ORTE_ERROR;
    }
    _rapl.rapl_esu=1<<msr;
    if (_rapl.rapl_esu==0){
        opal_output(0, "ERROR: RAPL UNIT register read error\n");
        _rapl.rapl_esu=1<<16;
        _rapl.cpu_rapl_support=0;
        _rapl.ddr_rapl_support=0;
        return ORTE_ERROR;
    }

    lseek(_rapl.fd_cpu[0], RAPL_CPU_ENERGY, 0);
    ret=read(_rapl.fd_cpu[0], &msr1, sizeof(unsigned long long));
    if (ret!=sizeof(unsigned long long)){
        opal_output(0, "ERROR: reading msr file\n");
        return ORTE_ERROR;
    }
    usleep(100000);
    lseek(_rapl.fd_cpu[0], RAPL_CPU_ENERGY, 0);
    ret=read(_rapl.fd_cpu[0], &msr2, sizeof(unsigned long long));
    if (ret!=sizeof(unsigned long long)){
        opal_output(0, "ERROR: reading msr file\n");
        return ORTE_ERROR;
    }
    if (msr1==msr2){
        opal_output(0, "ERROR: RAPL energy register read error\n");
        _rapl.cpu_rapl_support=0;
        return ORTE_ERROR;
    }

    lseek(_rapl.fd_cpu[0], RAPL_DDR_ENERGY, 0);
    ret=read(_rapl.fd_cpu[0], &msr1, sizeof(unsigned long long));
    if (ret!=sizeof(unsigned long long)){
        opal_output(0, "ERROR: reading msr file\n");
        return ORTE_ERROR;
    }
    usleep(100000);
    lseek(_rapl.fd_cpu[0], RAPL_DDR_ENERGY, 0);
    ret=read(_rapl.fd_cpu[0], &msr2, sizeof(unsigned long long));
    if (ret!=sizeof(unsigned long long)){
        opal_output(0, "ERROR: reading msr file\n");
        return ORTE_ERROR;
    }
    if (msr1==msr2){
        opal_output(0, "ERROR: RAPL energy register read error\n");
        _rapl.ddr_rapl_support=0;
        return ORTE_ERROR;
    }

    gettimeofday(&(_tv.tv_curr), NULL);
    _tv.tv_prev=_tv.tv_curr;
    _tv.interval=0;

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    return;
}

/*
 detect num of sockets
 */
static int detect_num_sockets(void)
{
    FILE *fp;
    char line[1024];
    int physical_id=0, new_id, i;

    fp=fopen("/proc/cpuinfo", "r");

    if (!fp){
        return 1;
    }

    while (!feof(fp)){
        fgets(line, 1024, fp);
        if (strncmp(line, "physical id", strlen("physical id"))==0){
            for (i=(int)(strlen("physical id")); i<(int)(strlen(line)); i++)
                if (line[i]==':'){
                    break;
                }
            if (i<(int)(strlen(line))){
                new_id=atoi(line+i+1);
                if (new_id>physical_id){
                    physical_id=new_id;
                }
            }
        }
    }
    fclose(fp);

    return (physical_id+1);
}
/*
 detect num of cpus
 */
static int detect_num_cpus(void)
{
    FILE *fp;
    char line[1024];
    int cpus=0;

    fp=fopen("/proc/cpuinfo", "r");

    if (!fp){
        return 1;
    }

    while (!feof(fp)){
        fgets(line, 1024, fp);
        if (strncmp(line, "processor", strlen("processor"))==0){
            cpus++;
        }
    }
    fclose(fp);

    return cpus;
}
/*
 detect cpu for each socket
*/
static void detect_cpu_for_each_socket(void)
{
    int i,k,cpu_id,socket_id;
    FILE *fp;
    char line[1024];

    fp=fopen("/proc/cpuinfo", "r");

    if (!fp){
        for (i=0; i<_rapl.n_sockets; i++)
            _rapl.cpu_idx[i]=0;
        return;
    }

    for (i=0; i<_rapl.n_cpus; i++)
        _rapl.cpu_idx[i]=-1;

    cpu_id=-1;
    socket_id=-1;
    while (!feof(fp)){
        fgets(line, 1024, fp);
        if (strncmp(line, "processor", strlen("processor"))==0){
        for (k=(int)(strlen("processor")); k<(int)(strlen(line)); k++)
            if (line[k]==':'){
                break;
            }
        if (k<(int)(strlen(line)))
            cpu_id=atoi(line+k+1);
            if (cpu_id>=MAX_CPUS){
                cpu_id=MAX_CPUS-1;
            }
        }
        if (strncmp(line, "physical id", strlen("physical id"))==0){
            for (k=(int)(strlen("physical id")); k<(int)(strlen(line)); k++)
            if (line[k]==':'){
                break;
            }
            if (k<(int)(strlen(line))){
                socket_id=atoi(line+k+1);
                if (socket_id>=MAX_SOCKETS){
                    socket_id=MAX_SOCKETS-1;
                }
            }
        }
        if ((cpu_id!=-1) && (socket_id!=-1)){
            if (_rapl.cpu_idx[socket_id]==-1){
                _rapl.cpu_idx[socket_id]=cpu_id;
            }
            cpu_id=-1;
            socket_id=-1;
        }
    }
    fclose(fp);

    return;
}

/*
 *  start componentpower plug-in
 */
static void start(orte_jobid_t jobid)
{

    /* we must be root to run */
    if (0 != geteuid()) {
        return;
    }

    /* start a separate componentpower progress thread for sampling */
    if (mca_sensor_componentpower_component.use_progress_thread) {
        if (!orcm_sensor_componentpower.ev_active) {
            orcm_sensor_componentpower.ev_active = true;
            if (NULL == (orcm_sensor_componentpower.ev_base = opal_progress_thread_init("componentpower"))) {
                orcm_sensor_componentpower.ev_active = false;
                return;
            }
        }

        /* setup componentpower sampler */
        componentpower_sampler = OBJ_NEW(orcm_sensor_sampler_t);

        /* check if componentpower sample rate is provided for this*/
        if (!mca_sensor_componentpower_component.sample_rate) {
            mca_sensor_componentpower_component.sample_rate = orcm_sensor_base.sample_rate;
        }
        componentpower_sampler->rate.tv_sec = mca_sensor_componentpower_component.sample_rate;
        componentpower_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(orcm_sensor_componentpower.ev_base, &componentpower_sampler->ev,
                               perthread_componentpower_sample, componentpower_sampler);
        opal_event_evtimer_add(&componentpower_sampler->ev, &componentpower_sampler->rate);
    }
    return;
}

/*
 *  stop componentpower plug-in
 */
static void stop(orte_jobid_t jobid)
{
    if (orcm_sensor_componentpower.ev_active) {
        orcm_sensor_componentpower.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_progress_thread_pause("componentpower");
        OBJ_RELEASE(componentpower_sampler);
    }
    return;
}
/*
 *  sample componentpower every X seconds
*/
static void componentpower_sample(orcm_sensor_sampler_t *sampler)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor componentpower : componentpower_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    if (!mca_sensor_componentpower_component.use_progress_thread) {
       collect_sample(sampler);
    }

}

static void perthread_componentpower_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor componentpower : perthread_componentpower_sample: called",
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
    /* check if componentpower sample rate is provided for this*/
    if (mca_sensor_componentpower_component.sample_rate != sampler->rate.tv_sec) {
        sampler->rate.tv_sec = mca_sensor_componentpower_component.sample_rate;
    }
    /* set ourselves to sample again */
    opal_event_evtimer_add(&sampler->ev, &sampler->rate);
}

static void collect_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    char *freq;
    opal_buffer_t data, *bptr;
    int32_t nsockets;
    bool packed;

    float power_cur;
    int i;
    unsigned long long interval, msr, rapl_delta;

     /* we must be root to run */
    if (0 != geteuid()) {
        return;
    }

    if (mca_sensor_componentpower_component.test) {
        /* generate and send a test vector */
        OBJ_CONSTRUCT(&data, opal_buffer_t);
        generate_test_vector(&data);
        bptr = &data;
        opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
        OBJ_DESTRUCT(&data);
        return;
    }
    gettimeofday(&(_tv.tv_curr), NULL);
    if (_tv.tv_curr.tv_usec>=_tv.tv_prev.tv_usec){
        _tv.interval=(unsigned long long)(_tv.tv_curr.tv_sec-_tv.tv_prev.tv_sec)*1000000
        +(unsigned long long)(_tv.tv_curr.tv_usec-_tv.tv_prev.tv_usec);
    } else {
        _tv.interval=(unsigned long long)(_tv.tv_curr.tv_sec-_tv.tv_prev.tv_sec-1)*1000000
        +(unsigned long long)(_tv.tv_curr.tv_usec+1000000-_tv.tv_prev.tv_usec);
    }
    if (_tv.interval==0){
        opal_output(0, "WARNING: interval is zero\n");
        _tv.interval=1;
    }
    interval=_tv.interval;
    _tv.tv_prev=_tv.tv_curr;

    if (_rapl.cpu_rapl_support==0){
        for (i=0; i<_rapl.n_sockets; i++){
            _rapl.cpu_power[i]=-1.0;
    }
    } else {
        _rapl.rapl_calls++;
        for (i=0; i<_rapl.n_sockets; i++){
            lseek(_rapl.fd_cpu[i], RAPL_CPU_ENERGY, 0);
            read(_rapl.fd_cpu[i], &msr, sizeof(unsigned long long));
            _rapl.cpu_rapl[i]=msr;

            if (_rapl.cpu_rapl[i]>=_rapl.cpu_rapl_prev[i]){
                rapl_delta=_rapl.cpu_rapl[i]-_rapl.cpu_rapl_prev[i];
            } else {
                rapl_delta=_rapl.cpu_rapl[i]+0x100000000-_rapl.cpu_rapl_prev[i];
            }

            if (rapl_delta==0){
                _rapl.cpu_power[i]=-1.0;
            } else {
                _rapl.cpu_power[i]=((double)rapl_delta / (double)(_rapl.rapl_esu))/((double)interval/1000000.0);
            }
            if (_rapl.cpu_power[i]>1000.0){
                _rapl.cpu_power[i]=-1.0;
            }
            _rapl.cpu_rapl_prev[i]=_rapl.cpu_rapl[i];
        }
    }

    if (_rapl.ddr_rapl_support==0){
        for (i=0; i<_rapl.n_sockets; i++){
            _rapl.ddr_power[i]=-1.0;
        }
    } else {
        for (i=0; i<_rapl.n_sockets; i++){
            lseek(_rapl.fd_cpu[i], RAPL_DDR_ENERGY, 0);
            read(_rapl.fd_cpu[i], &msr, sizeof(unsigned long long));
            _rapl.ddr_rapl[i]=msr;
            if (_rapl.ddr_rapl[i]>=_rapl.ddr_rapl_prev[i]){
                rapl_delta=_rapl.ddr_rapl[i]-_rapl.ddr_rapl_prev[i];
            } else {
                rapl_delta=_rapl.ddr_rapl[i]+0x100000000-_rapl.ddr_rapl_prev[i];
            }

            if (rapl_delta==0) {
                _rapl.ddr_power[i]=-1.0;
            } else {
                _rapl.ddr_power[i]=((double)rapl_delta / (double)(_rapl.rapl_esu))/((double)interval/1000000.0);
            }
            if (_rapl.ddr_power[i]>1000.0) {
                _rapl.ddr_power[i]=-1.0;
            }
            _rapl.ddr_rapl_prev[i]=_rapl.ddr_rapl[i];
        }

    }

    /* prep to store the results */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    packed = false;

    /* pack our name */
    freq = strdup("componentpower");
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &freq, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        free(freq);
        return;
    }
    free(freq);

    /* store our hostname */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* store the number of sockets */
    nsockets = detect_num_sockets();

    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &nsockets, 1, OPAL_INT32))) {

        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* get the sample time */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &(_tv.tv_curr), 1, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }

    for (i=0; i<_rapl.n_sockets; i++){
        if (_rapl.rapl_calls<=1){
            power_cur=0.0;
        } else{
            power_cur=(float)(_rapl.cpu_power[i]);
        }
        opal_dss.pack(&data, &power_cur, 1, OPAL_FLOAT);
    }

    for (i=0; i<_rapl.n_sockets; i++){
        if (_rapl.rapl_calls<=1){
            power_cur=0.0;
        } else{
            power_cur=(float)(_rapl.ddr_power[i]);
        }
        opal_dss.pack(&data, &power_cur, 1, OPAL_FLOAT);
    }

    packed=true;

    /* xfer the data for transmission */
    if (packed) {
        bptr = &data;
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }
    }
}

/*
 *  componentpower to be picked up by heartbeat
 */
static void componentpower_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    char temp_str[64];
    int rc;
    int32_t n, nsockets;
    int i;
    int sensor_not_avail=0;
    struct timeval tv_curr;
    float power_cur, cpu_power_temp[MAX_SOCKETS], ddr_power_temp[MAX_SOCKETS];
    char time_str[40];
    struct tm *time_info;
    orcm_value_t *sensor_metric;
    orcm_analytics_value_t *analytics_vals;
    opal_list_t *key;
    opal_list_t *non_compute_data;


    /* unpack the host this came from */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }

    /* and the number of sockets on that host */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &nsockets, &n, OPAL_INT32))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "%s Received freq log from host %s with %d sockets",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname, nsockets);

    /* timestamp */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &tv_curr, &n, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }

    if (nsockets>MAX_SOCKETS)
        nsockets=MAX_SOCKETS;

    /* cpu power */
    n=1;
    for (i=0; i<nsockets; i++){
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &power_cur, &n, OPAL_FLOAT))){
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }
        cpu_power_temp[i]=power_cur;
    }

    /* ddr power */
    n=1;
    for (i=0; i<nsockets; i++){
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &power_cur, &n, OPAL_FLOAT))){
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }
        ddr_power_temp[i]=power_cur;
    }

    key = OBJ_NEW(opal_list_t);
    if (NULL == key) {
        goto cleanup;

    }

    non_compute_data = OBJ_NEW(opal_list_t);
    if (NULL == non_compute_data) {
        goto cleanup;

    }


    sensor_metric = orcm_util_load_orcm_value("ctime", &tv_curr, OPAL_TIMEVAL, NULL);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }

    time_info=localtime(&(tv_curr.tv_sec));
    if (NULL == time_info) {
        sensor_not_avail=1;
        opal_output(0,"componentpower sensor data not logged due to error of localtime()\n");
    } else {
        strftime(time_str, sizeof(time_str), "%F %T%z", time_info);
    }

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                       "second=%s\n", time_str);
    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                       "sub-second=%.3f\n", (float)(tv_curr.tv_usec)/1000000.0);
    opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

    /* load the hostname */
    sensor_metric = orcm_util_load_orcm_value("hostname", hostname, OPAL_STRING, NULL);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("data_group", "componentpower", OPAL_STRING, NULL);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    for (i=0; i<nsockets; i++){
        analytics_vals = orcm_util_load_orcm_analytics_value(key, non_compute_data, NULL);
        if ((NULL == analytics_vals) || (NULL == analytics_vals->key) ||
             (NULL == analytics_vals->non_compute_data) ||(NULL == analytics_vals->compute_data)) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            goto cleanup;
        }

        if (0 > snprintf(temp_str, sizeof(temp_str), "cpu%d_power", i)) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            goto cleanup;
        }

        sensor_metric = orcm_util_load_orcm_value(temp_str, &cpu_power_temp[i], OPAL_FLOAT, "W");
        if (NULL == sensor_metric) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            goto cleanup;
        }
        
        if (cpu_power_temp[i]<=(float)(0.0)){
            sensor_not_avail=1;
            if (_rapl.rapl_calls>3){
                opal_output(0,"componentpower sensor data not logged due to unexpected return value from RAPL\n");
            }
        } else {
            opal_list_append(analytics_vals->compute_data, (opal_list_item_t *)sensor_metric);
        }

        if (!sensor_not_avail) {
            orcm_analytics.send_data(analytics_vals);
        }
        OBJ_RELEASE(analytics_vals);
    }

    for (i=0; i<nsockets; i++){
        analytics_vals = orcm_util_load_orcm_analytics_value(key, non_compute_data, NULL);
        if ((NULL == analytics_vals) || (NULL == analytics_vals->key) ||
             (NULL == analytics_vals->non_compute_data) ||(NULL == analytics_vals->compute_data)) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            goto cleanup;
        }

        if (0 > snprintf(temp_str, sizeof(temp_str), "ddr%d_power", i)) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            goto cleanup;
        }
        sensor_metric = orcm_util_load_orcm_value(temp_str, &ddr_power_temp[i], OPAL_FLOAT, "W");
        if (NULL == sensor_metric) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            goto cleanup;
        }
        
        if (ddr_power_temp[i]<=(float)(0.0)){
            sensor_not_avail=1;
            if (_rapl.rapl_calls>3){
                opal_output(0,"componentpower sensor data not logged due to unexpected return value from RAPL\n");
            }
        } else {
            opal_list_append(analytics_vals->compute_data, (opal_list_item_t *)sensor_metric);
        }

        if (!sensor_not_avail) {
            orcm_analytics.send_data(analytics_vals);
        }
        OBJ_RELEASE(analytics_vals);
    }

cleanup:
    SAFEFREE(hostname);
    if ( NULL != key) {
        OPAL_LIST_RELEASE(key);
    }
    if ( NULL != non_compute_data) {
        OPAL_LIST_RELEASE(non_compute_data);
    }
}

static void componentpower_set_sample_rate(int sample_rate)
{
    /* set the componentpower sample rate if seperate thread is enabled */
    if (mca_sensor_componentpower_component.use_progress_thread) {
        mca_sensor_componentpower_component.sample_rate = sample_rate;
    }
    return;
}

static void componentpower_get_sample_rate(int *sample_rate)
{
    if (NULL != sample_rate) {
    /* check if componentpower sample rate is provided for this*/
        if (mca_sensor_componentpower_component.use_progress_thread) {
            *sample_rate = mca_sensor_componentpower_component.sample_rate;
        }
    }
    return;
}

static void generate_test_vector(opal_buffer_t *v)
{
    int ret;
    int i;
    char *ctmp;
    int32_t nsockets;
    struct timeval tv_test;
    float cpu_test_power;
    float ddr_test_power;
    nsockets = MAX_SOCKETS;

/* Power units are Watts */
    cpu_test_power = 100;
    ddr_test_power = 10;

/* pack the plugin name */
    ctmp = strdup("componentpower");
    if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &ctmp, 1, OPAL_STRING))){
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&v);
        free(ctmp);
        return;
    }
    free(ctmp);

/* pack the hostname */
    if (OPAL_SUCCESS != (ret =
                opal_dss.pack(v, &orte_process_info.nodename, 1, OPAL_STRING))){
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&v);
        return;
    }

    /* pack then number of cores */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &nsockets, 1, OPAL_INT32))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&v);
        return;
    }
/* get the time of sampling */
    gettimeofday(&tv_test,NULL);
/* pack the sampling time */
    if (OPAL_SUCCESS != (ret =
                opal_dss.pack(v, &tv_test, 1, OPAL_TIMEVAL))){
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&v);
        return;
    }

/* pack the cpu test power values */
    for (i=0; i < nsockets; i++) {

        if (OPAL_SUCCESS != (ret =
                    opal_dss.pack(v, &cpu_test_power, 1, OPAL_FLOAT))){
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&v);
            return;
        }
        cpu_test_power += 10.0;
    }
/* pack the ddr test power value */
    for (i=0; i < nsockets; i++) {
        if (OPAL_SUCCESS != (ret =
                    opal_dss.pack(v, &ddr_test_power, 1, OPAL_FLOAT))){
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&v);
            return;
        }
        ddr_test_power += 1.0;
    }

    opal_output_verbose(5,orcm_sensor_base_framework.framework_output,
        "%s sensor:componentpower: Number of test sockets is %d with %d cpus each",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),MAX_SOCKETS,MAX_CPUS);
}

static void generate_test_inv_data(opal_buffer_t *inventory_snapshot)
{
    unsigned int tot_items = 2;
    char *comp = strdup("componentpower");
    int rc = OPAL_SUCCESS;

    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        free(comp);
        return;
    }
    free(comp);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    asprintf(&comp, "sensor_componentpower_1");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        free(comp);
        return;
    }
    free(comp);

    asprintf(&comp, "cpu0_power");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        free(comp);
        return;
    }
    free(comp);
    asprintf(&comp, "sensor_componentpower_2");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        free(comp);
        return;
    }
    free(comp);

    asprintf(&comp, "ddr0_power");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        free(comp);
        return;
    }
    free(comp);
}

static void componentpower_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    if(mca_sensor_componentpower_component.test) {
        generate_test_inv_data(inventory_snapshot);
    } else {
        unsigned int tot_items = (unsigned int)(_rapl.n_sockets * 2) + 1; /* +1 is for "hostname"/nodename pair */
        char *comp = strdup("componentpower");
        int rc = OPAL_SUCCESS;
        int i = 0;

        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            free(comp);
            return;
        }
        free(comp);

        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        /* store our hostname */
        comp = strdup("hostname");
        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            free(comp);
            return;
        }
        free(comp);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &orte_process_info.nodename, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        for(i = 0; i < _rapl.n_sockets; ++i) {
            asprintf(&comp, "sensor_componentpower_%d", i+1);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                free(comp);
                return;
            }
            free(comp);

            asprintf(&comp, "cpu%d_power", i);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                free(comp);
                return;
            }
            free(comp);
        }
        for(i = 0; i < _rapl.n_sockets; ++i) {
            asprintf(&comp, "sensor_componentpower_%d", i+_rapl.n_sockets);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                free(comp);
                return;
            }
            free(comp);

            asprintf(&comp, "ddr%d_power", i);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                free(comp);
                return;
            }
            free(comp);
        }
    }
}

static void my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata)
{
    OBJ_RELEASE(kvs);
}

static void componentpower_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
{
    unsigned int tot_items = 0;
    int n = 1;
    opal_list_t *records = NULL;
    int rc = OPAL_SUCCESS;

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &tot_items, &n, OPAL_UINT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    records = OBJ_NEW(opal_list_t);
    while(tot_items > 0) {
        char *inv = NULL;
        char *inv_val = NULL;
        orcm_value_t *mkv = NULL;

        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &inv, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(records);
            return;
        }
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &inv_val, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(records);
            return;
        }

        mkv = OBJ_NEW(orcm_value_t);
        mkv->value.key = inv;
        mkv->value.type = OPAL_STRING;
        mkv->value.data.string = inv_val;
        opal_list_append(records, (opal_list_item_t*)mkv);

        --tot_items;
    }
    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA, records, NULL, my_inventory_log_cleanup, NULL);
    } else {
        my_inventory_log_cleanup(-1, -1, records, NULL, NULL);
    }
}
