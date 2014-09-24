/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
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

#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/db/db.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_componentpower.h"

#include <ipmicmd.h>

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void componentpower_sample(orcm_sensor_sampler_t *sampler);
static void componentpower_log(opal_buffer_t *buf);
static int detect_num_sockets(void);
static int detect_num_cpus(void);
static void detect_cpu_for_each_socket(void);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_componentpower_module = {
    init,
    finalize,
    start,
    stop,
    componentpower_sample,
    componentpower_log
};

__time_val _tv;
__rapl _rapl;

static bool log_enabled = true;

static int init(void)
{
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
    int n_cpus, n_sockets, i;
    char path[STR_LEN];
    unsigned long long msr, msr1, msr2;

    /* we must be root to run */
    if (0 != geteuid()) {
        return;
    }

    gettimeofday(&(_tv.tv_curr), NULL);
    _tv.tv_prev=_tv.tv_curr;
    _tv.interval=0;

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
        snprintf(path, 100, "/dev/cpu/%d/msr", _rapl.cpu_idx[i]);
        _rapl.fd_cpu[i]=open(path, O_RDWR);
        if (_rapl.fd_cpu[i]<=0){
        opal_output(0, "error opening msr file\n");
        _rapl.dev_msr_support=0;
        }
    }

    if (_rapl.dev_msr_support==0){
        _rapl.cpu_rapl_support=0;
        _rapl.ddr_rapl_support=0;
        return;
    }

    lseek(_rapl.fd_cpu[0], RAPL_UNIT, 0);
    read(_rapl.fd_cpu[0], &msr, sizeof(unsigned long long));
/* get energy unit */
    msr=(msr>>8)&0x1f;
    if (!msr){
        opal_output(0, "RAPL is not enabled\n");
        _rapl.cpu_rapl_support=0;
        _rapl.ddr_rapl_support=0;
    }
    _rapl.rapl_esu=1<<msr;
    if (_rapl.rapl_esu==0){
        opal_output(0, "RAPL is not enabled\n");
        _rapl.rapl_esu=1<<16;
        _rapl.cpu_rapl_support=0;
        _rapl.ddr_rapl_support=0;
    }

    lseek(_rapl.fd_cpu[0], RAPL_CPU_ENERGY, 0);
    read(_rapl.fd_cpu[0], &msr1, sizeof(unsigned long long));
    usleep(100000);
    lseek(_rapl.fd_cpu[0], RAPL_CPU_ENERGY, 0);
    read(_rapl.fd_cpu[0], &msr2, sizeof(unsigned long long));
    if (msr1==msr2){
        opal_output(0, "CPU RAPL is not enabled\n");
        _rapl.cpu_rapl_support=0;
    }

    lseek(_rapl.fd_cpu[0], RAPL_DDR_ENERGY, 0);
    read(_rapl.fd_cpu[0], &msr1, sizeof(unsigned long long));
    usleep(100000);
    lseek(_rapl.fd_cpu[0], RAPL_DDR_ENERGY, 0);
    read(_rapl.fd_cpu[0], &msr2, sizeof(unsigned long long));
    if (msr1==msr2){
        opal_output(0, "DDR RAPL is not enabled\n");
        _rapl.ddr_rapl_support=0;
    }

    return;
}

/*
 *  stop componentpower plug-in
 */
static void stop(orte_jobid_t jobid)
{
    return;
}
/*
 *  sample componentpower every X seconds
*/
static void componentpower_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    char *freq;
    opal_buffer_t data, *bptr;
    int32_t nsockets;
    time_t now;
    char time_str[STR_LEN];
    char *timestamp_str;
    bool packed;

    float power_cur;
    int i;
    unsigned long long interval, msr, rapl_delta;
    struct tm *sample_time;

     /* we must be root to run */
    if (0 != geteuid()) {
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
                _rapl.cpu_power[i]=1000.0;
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
            if (_rapl.cpu_power[i]>1000.0) {
                _rapl.cpu_power[i]=1000.0;
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

/*
 *  componentpower to be picked up by heartbeat
 */
static void componentpower_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    char *sampletime;
    char temp_str[64];
    int rc;
    int32_t n, nsockets;
    opal_list_t *vals;
    opal_value_t *kv;
    int i;

    float power_cur;

    if (!log_enabled) {
        return;
    }

    /* unpack the host this came from */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    /* and the number of sockets on that host */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &nsockets, &n, OPAL_INT32))) {
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
                        "%s Received freq log from host %s with %d sockets",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname, nsockets);

    /* xfr to storage */
    vals = OBJ_NEW(opal_list_t);

    /* load the sample time at the start */
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
    if (hostname==NULL)
        kv->data.string = strdup("NULL");
    else
        kv->data.string = strdup(hostname);
    opal_list_append(vals, &kv->super);

    for (i=0; i<nsockets; i++){
        snprintf(temp_str, 100, "cpu%d_power", i);
        kv = OBJ_NEW(opal_value_t);
        kv->key=strdup(temp_str);
        kv->type=OPAL_FLOAT;
        n=1;
        opal_dss.unpack(sample, &power_cur, &n, OPAL_FLOAT);
        kv->data.fval=power_cur;
        opal_list_append(vals, &kv->super);
    }

    for (i=0; i<nsockets; i++){
        snprintf(temp_str, 100, "ddr%d_power", i);
        kv = OBJ_NEW(opal_value_t);
        kv->key=strdup(temp_str);
        kv->type=OPAL_FLOAT;
        n=1;
        opal_dss.unpack(sample, &power_cur, &n, OPAL_FLOAT);
        kv->data.fval=power_cur;
        opal_list_append(vals, &kv->super);
    }

    /* store it */
    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store(orcm_sensor_base.dbhandle, "componentpower", vals, mycleanup, NULL);
    } else {
        OPAL_LIST_RELEASE(vals);
    }

    if (NULL != hostname) {
        free(hostname);
    }

}
