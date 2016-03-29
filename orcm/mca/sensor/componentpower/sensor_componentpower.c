/*
 * Copyright (c) 2014-2016  Intel, Inc. All rights reserved.
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
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "sensor_componentpower.h"

#define TEST_SOCKETS (MAX_SOCKETS)

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void componentpower_sample(orcm_sensor_sampler_t *sampler);
static void perthread_componentpower_sample(int fd, short args, void *cbdata);
void collect_componentpower_sample(orcm_sensor_sampler_t *sampler);
static void componentpower_log(opal_buffer_t *buf);
static int detect_num_sockets(void);
static int detect_num_cpus(void);
static void detect_cpu_for_each_socket(void);
static void componentpower_set_sample_rate(int sample_rate);
static void componentpower_get_sample_rate(int *sample_rate);
static void componentpower_inventory_collect(opal_buffer_t *inventory_snapshot);
static void componentpower_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);
int componentpower_enable_sampling(const char* sensor_specification);
int componentpower_disable_sampling(const char* sensor_specification);
int componentpower_reset_sampling(const char* sensor_specification);

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
    componentpower_get_sample_rate,
    componentpower_enable_sampling,
    componentpower_disable_sampling,
    componentpower_reset_sampling
};

static __time_val _tv;
static __rapl _rapl;

static orcm_sensor_sampler_t *componentpower_sampler = NULL;
static orcm_sensor_componentpower_t orcm_sensor_componentpower;

static void generate_test_vector(opal_buffer_t *v);
static int load_msr_file_descriptors_for_each_socket(void);
static void setup_cpu_information(void);
static int read_register_value(int register_name, int socket, unsigned long long *msr);
static int rapl_register_lock_check(void);
static int energy_unit_check(void);
static int rapl_unit_register_check(void);
static int rapl_cpu_energy_register_check(void);
static int rapl_ddr_energy_register_check(void);

#define ON_NULL_RETURN(x) if(NULL==x){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);return;}
#define ON_NULL_GOTO(x,label) if(NULL==x){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);goto label;}
#define ON_FAILURE_RETURN(x) if(ORCM_SUCCESS!=x){ORTE_ERROR_LOG(x);return;}

static int init(void)
{
    mca_sensor_componentpower_component.diagnostics = 0;
    mca_sensor_componentpower_component.runtime_metrics =
        orcm_sensor_base_runtime_metrics_create("componentpower", orcm_sensor_base.collect_metrics,
                                                mca_sensor_componentpower_component.collect_metrics);

    setup_cpu_information();

    detect_cpu_for_each_socket();

    if (ORCM_SUCCESS != load_msr_file_descriptors_for_each_socket()) {
        opal_output(0, "ERROR: opening msr file descriptors\n");
        return ORCM_ERR_FILE_OPEN_FAILURE;
    }

    if (ORCM_SUCCESS != rapl_register_lock_check()) {
        opal_output(0, "ERROR: RAPL is locked\n");
        return ORCM_ERR_RESOURCE_BUSY;
    }

    if (ORCM_SUCCESS != energy_unit_check()) {
        opal_output(0, "WARNING: energy unit check fails" );
    }

    gettimeofday(&(_tv.tv_curr), NULL);
    _tv.tv_prev=_tv.tv_curr;
    _tv.interval=0;

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_componentpower_component.runtime_metrics);
    mca_sensor_componentpower_component.runtime_metrics = NULL;

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

static void setup_cpu_information(void)
{
    int n_cpus;
    int n_sockets;
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
}

static int load_msr_file_descriptors_for_each_socket(void) {
    int i;
    char base_path[] = "/dev/cpu/%d/msr";
    char path[STR_LEN];
    for (i=0; i<_rapl.n_sockets; i++){
        _rapl.cpu_rapl[i]=_rapl.cpu_rapl_prev[i]=0;
        _rapl.ddr_rapl[i]=_rapl.ddr_rapl_prev[i]=0;
        snprintf(path, sizeof(path), base_path, _rapl.cpu_idx[i]);
        _rapl.fd_cpu[i]=open(path, O_RDWR);
        if (_rapl.fd_cpu[i]<0){
            _rapl.dev_msr_support=0;
            _rapl.cpu_rapl_support=0;
            _rapl.ddr_rapl_support=0;
            return ORCM_ERR_FILE_OPEN_FAILURE;
        }
    }
    return ORCM_SUCCESS;
}

static int read_register_value(int register_name, int socket,
    unsigned long long *msr)
{
    int ret;
    int msr_size=sizeof(unsigned long long);
    lseek(_rapl.fd_cpu[socket], register_name, 0);
    ret=read(_rapl.fd_cpu[socket], msr, msr_size);
    if (ret!=msr_size) {
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

static int rapl_register_lock_check(void)
{
    unsigned long long msr;
    const int socket=0;
    if (ORCM_SUCCESS != read_register_value(RAPL_POWER_LIMIT, socket, &msr)){
        _rapl.dev_msr_support=0;
        _rapl.cpu_rapl_support=0;
        _rapl.ddr_rapl_support=0;
        opal_output(0, "ERROR: reading RAPL_POWER_LIMIT register from msr\n");
        return ORCM_ERR_FILE_READ_FAILURE;
    }
    msr=msr&0x80000000;
    if (msr){
        _rapl.dev_msr_support=0;
        _rapl.cpu_rapl_support=0;
        _rapl.ddr_rapl_support=0;
        opal_output(0, "WARNING: rapl register is locked, msr support disabled\n");
    }
    return ORCM_SUCCESS;
}

static int rapl_unit_register_check(void)
{
    unsigned long long msr;
    const int socket=0;
    if (ORCM_SUCCESS != read_register_value(RAPL_UNIT, socket, &msr)){
        _rapl.cpu_rapl_support=0;
        _rapl.ddr_rapl_support=0;
        opal_output(0, "ERROR: reading RAPL_UNIT register from msr\n");
        return ORCM_ERR_FILE_READ_FAILURE;
    }
    msr=(msr>>8)&0x1f;
    if (!msr){
        _rapl.cpu_rapl_support=0;
        _rapl.ddr_rapl_support=0;
        opal_output(0, "WARNING: rapl support was disabled\n");
    }
    _rapl.rapl_esu=1<<msr;
    if (_rapl.rapl_esu==0){
        _rapl.rapl_esu=1<<16;
        _rapl.cpu_rapl_support=0;
        _rapl.ddr_rapl_support=0;
        opal_output(0, "WARNING: rapl support was disabled\n");
    }
    return ORCM_SUCCESS;
}

static int sample_register(int register_name, int socket, int us_delay,
    unsigned long long * msr1, unsigned long long * msr2)
{
    if (ORCM_SUCCESS != read_register_value(register_name, socket, msr1)){
        opal_output(0, "ERROR: sampling register from msr\n");
        return ORCM_ERR_FILE_READ_FAILURE;
    }
    usleep(us_delay);
    if (ORCM_SUCCESS != read_register_value(register_name, socket, msr2)){
        opal_output(0, "ERROR: sampling register from msr\n");
        return ORCM_ERR_FILE_READ_FAILURE;
    }
    return ORCM_SUCCESS;
 }

static int rapl_cpu_energy_register_check(void)
{
    unsigned long long msr1, msr2;
    const int us_delay=100000;
    const int socket=0;
    if (ORCM_SUCCESS != sample_register(RAPL_CPU_ENERGY, socket, us_delay, &msr1, &msr2)){
        _rapl.cpu_rapl_support=0;
        opal_output(0, "ERROR: sampling RAPL_CPU_ENERGY from msr\n");
        return ORCM_ERR_FILE_READ_FAILURE;
    }
    if (msr1==msr2){
        _rapl.cpu_rapl_support=0;
        opal_output(0, "WARNING: cpu rapl support disabled\n");
    }
    return ORCM_SUCCESS;
}

static int rapl_ddr_energy_register_check(void)
{
    unsigned long long msr1, msr2;
    const int us_delay=100000;
    const int socket=0;
    if (ORCM_SUCCESS != sample_register(RAPL_DDR_ENERGY, socket, us_delay, &msr1, &msr2)){
        _rapl.ddr_rapl_support=0;
        opal_output(0, "ERROR: sampling RAPL_DDR_ENERGY from msr\n");
        return ORCM_ERR_FILE_READ_FAILURE;
    }
    if (msr1==msr2){
        _rapl.ddr_rapl_support=0;
        opal_output(0, "WARNING: ddr support disabled\n");
    }
    return ORCM_SUCCESS;
}

static int energy_unit_check(void)
{
    int ret=ORCM_SUCCESS;
    if (ORCM_SUCCESS != rapl_unit_register_check()){
        opal_output(0, "ERROR: RAPL_UNIT register check fails\n");
        ret=ORCM_ERROR;
    }
    else {
        if (ORCM_SUCCESS != rapl_cpu_energy_register_check()){
            opal_output(0, "ERROR: RAPL_CPU_ENERGY register check fails\n");
            ret=ORCM_ERROR;
        }
        if (ORCM_SUCCESS != rapl_ddr_energy_register_check()){
            opal_output(0, "ERROR: RAPL_DDR_ENERGY register check fails\n");
            ret=ORCM_ERROR;
        }
    }
    return ret;
}

/*
 *  start componentpower plug-in
 */
static void start(orte_jobid_t jobid)
{
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
    } else {
        mca_sensor_componentpower_component.sample_rate = orcm_sensor_base.sample_rate;
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
       collect_componentpower_sample(sampler);
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
    collect_componentpower_sample(sampler);
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

void collect_componentpower_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    char* componentpower = NULL;
    opal_buffer_t data, *bptr;
    int32_t nlabels;
    bool packed;

    float power_cur;
    int i;
    unsigned long long interval, msr, rapl_delta;
    void* metrics_obj = mca_sensor_componentpower_component.runtime_metrics;

    if (mca_sensor_componentpower_component.test) {
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
                            "%s sensor componentpower : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    mca_sensor_componentpower_component.diagnostics |= 0x1;

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
    componentpower = "componentpower";
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &componentpower, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* store our hostname */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* store the number of labels */
    if(orcm_sensor_base_runtime_inventory_available(mca_sensor_componentpower_component.runtime_metrics)) {
        nlabels = orcm_sensor_base_runtime_metrics_active_label_count(metrics_obj);
    } else {
        nlabels = _rapl.n_sockets * 2;
    }

    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &nlabels, 1, OPAL_INT32))) {

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
        char* label = NULL;
        uint8_t type = OPAL_FLOAT;
        if (_rapl.rapl_calls<=1){
            power_cur=0.0;
        } else{
            power_cur=(float)(_rapl.cpu_power[i]);
        }
        if(-1 == asprintf(&label, "cpu%d_power", i) || NULL == label) {
            ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
            OBJ_DESTRUCT(&data);
            return;
        }
        if (orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, label)) {
            if(OPAL_SUCCESS != (ret = opal_dss.pack(&data, &label, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);
                SAFEFREE(label);
                return;
            }
            if(OPAL_SUCCESS != (ret = opal_dss.pack(&data, &type, 1, OPAL_UINT8))) {
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);
                SAFEFREE(label);
                return;
            }
            if(OPAL_SUCCESS != (ret = opal_dss.pack(&data, &power_cur, 1, OPAL_FLOAT))) {
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);
                SAFEFREE(label);
                return;
            }
        }
        SAFEFREE(label);
    }

    for (i=0; i<_rapl.n_sockets; i++){
        char* label = NULL;
        uint8_t type = OPAL_FLOAT;
        if (_rapl.rapl_calls<=1){
            power_cur=0.0;
        } else{
            power_cur=(float)(_rapl.ddr_power[i]);
        }
        if(-1 == asprintf(&label, "ddr%d_power", i) || NULL == label) {
            ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
            OBJ_DESTRUCT(&data);
            return;
        }
        if (orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, label)) {
            if(OPAL_SUCCESS != (ret = opal_dss.pack(&data, &label, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);
                SAFEFREE(label);
                return;
            }
            if(OPAL_SUCCESS != (ret = opal_dss.pack(&data, &type, 1, OPAL_UINT8))) {
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);
                SAFEFREE(label);
                return;
            }
            if(OPAL_SUCCESS != (ret = opal_dss.pack(&data, &power_cur, 1, OPAL_FLOAT))) {
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);
                SAFEFREE(label);
                return;
            }
        }
        SAFEFREE(label);
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

static void componentpower_log_cleanup(char *hostname, opal_list_t *key,opal_list_t *non_compute_data,
                                       orcm_analytics_value_t *analytics_vals)
{
    SAFEFREE(hostname);
    if ( NULL != key) {
        OBJ_RELEASE(key);
    }
    if ( NULL != non_compute_data) {
        OBJ_RELEASE(non_compute_data);
    }
    if ( NULL != analytics_vals) {
        OBJ_RELEASE(analytics_vals);
    }
}

/*
 *  componentpower to be picked up by heartbeat
 */
static void componentpower_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    char *label=NULL;
    int rc;
    int32_t n;
    int32_t nlabels;
    int i;
    uint8_t type;
    float value;
    struct timeval tv_curr;
    orcm_value_t *sensor_metric = NULL;
    orcm_analytics_value_t *analytics_vals = NULL;
    opal_list_t *key = NULL;
    opal_list_t *non_compute_data = NULL;


    /* unpack the host this came from */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
        return;
    }
    non_compute_data = OBJ_NEW(opal_list_t);
    sensor_metric = orcm_util_load_orcm_value("hostname", hostname, OPAL_STRING, NULL);
    sensor_metric = orcm_util_load_orcm_value("data_group", "componentpower", OPAL_STRING, NULL);

    /* and the number of labels collected on that host */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &nlabels, &n, OPAL_INT32))) {
        ORTE_ERROR_LOG(rc);
        componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
        return;
    }

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "%s Received componentpower log from host %s with %d labels collected",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname, nlabels);

    /* timestamp */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &tv_curr, &n, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
        return;
    }

    key = OBJ_NEW(opal_list_t);
    if(NULL == key) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
        SAFEFREE(label);
        return;
    }
    sensor_metric = orcm_util_load_orcm_value("hostname", hostname, OPAL_STRING, NULL);
    if(NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
        SAFEFREE(label);
        return;
    }
    opal_list_append(key, (opal_list_item_t *)sensor_metric);
    sensor_metric = orcm_util_load_orcm_value("data_group", "componentpower", OPAL_STRING, NULL);
    if(NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
        SAFEFREE(label);
        return;
    }
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    non_compute_data = OBJ_NEW(opal_list_t);
    if(NULL == non_compute_data) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
        SAFEFREE(label);
        return;
    }
    sensor_metric = orcm_util_load_orcm_value("ctime", &tv_curr, OPAL_TIMEVAL, NULL);
    if(NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
        SAFEFREE(label);
        return;
    }
    opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

    analytics_vals = orcm_util_load_orcm_analytics_value(key, non_compute_data, NULL);
    if(NULL == analytics_vals) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
        SAFEFREE(label);
        return;
    }
    for(i = 0; i < nlabels; ++i) {
        n = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &label, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
            SAFEFREE(label);
            return;
        }
        n = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &type, &n, OPAL_UINT8)) || type != OPAL_FLOAT) {
            ORTE_ERROR_LOG((rc == OPAL_SUCCESS)?OPAL_ERR_TYPE_MISMATCH:rc);
            componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
            SAFEFREE(label);
            return;
        }
        n = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &value, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
            SAFEFREE(label);
            return;
        }
        sensor_metric = orcm_util_load_orcm_value(label, &value, OPAL_FLOAT, "W");
        SAFEFREE(label);
        if(0.0 >= value) {
            opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                                "%s Received componentpower log from host %s with illegal value of %f for %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                (NULL == hostname) ? "NULL" : hostname, value, label);
            continue;
        }
        if(NULL == sensor_metric) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
            return;
        }

        opal_list_append(analytics_vals->compute_data, (opal_list_item_t *)sensor_metric);
    }

    if(0 < opal_list_get_size(analytics_vals->compute_data)) {
        orcm_analytics.send_data(analytics_vals);
        /* Don't release analytics_vals. It's retain(ed) and being used in the workflows at this point
         * This doesn't cause any memory leak*/
        componentpower_log_cleanup(hostname, key, non_compute_data, NULL);
    } else {
        componentpower_log_cleanup(hostname, key, non_compute_data, analytics_vals);
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
        *sample_rate = mca_sensor_componentpower_component.sample_rate;
    }
    return;
}

static void generate_test_vector(opal_buffer_t *v)
{
    int ret;
    int i;
    const char *cpwr = "componentpower";
    int32_t nsockets = TEST_SOCKETS;
    struct timeval tv_test;
    float cpu_test_power;
    float ddr_test_power;

/* Power units are Watts */
    cpu_test_power = 100;
    ddr_test_power = 10;

/* pack the plugin name */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &cpwr, 1, OPAL_STRING))){
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&v);
        return;
    }

/* pack the hostname */
    if (OPAL_SUCCESS != (ret =
                opal_dss.pack(v, &orte_process_info.nodename, 1, OPAL_STRING))){
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&v);
        return;
    }

    uint32_t total_data_items = nsockets * 2;
    /* pack then number of cores */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &total_data_items, 1, OPAL_INT32))) {
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
        char* label = NULL;
        uint8_t type = OPAL_FLOAT;
        asprintf(&label, "cpu%d_power", i);
        if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &label, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&v);
            return;
        }
        if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &type, 1, OPAL_UINT8))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&v);
            return;
        }
        if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &cpu_test_power, 1, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&v);
            return;
        }
        cpu_test_power += 0.1;
    }
/* pack the ddr test power value */
    for (i=0; i < nsockets; i++) {
        char* label = NULL;
        uint8_t type = OPAL_FLOAT;
        asprintf(&label, "ddr%d_power", i);
        if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &label, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&v);
            return;
        }
        if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &type, 1, OPAL_UINT8))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&v);
            return;
        }
        if (OPAL_SUCCESS != (ret =
                    opal_dss.pack(v, &ddr_test_power, 1, OPAL_FLOAT))){
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&v);
            return;
        }
        ddr_test_power += 0.05;
    }

    opal_output_verbose(5,orcm_sensor_base_framework.framework_output,
        "%s sensor:componentpower: Number of test sockets is %d with %d cpus each",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),MAX_SOCKETS,MAX_CPUS);
}

static void generate_test_inv_data(opal_buffer_t *inventory_snapshot)
{
    int nsockets = TEST_SOCKETS;
    unsigned int tot_items = nsockets * 2;
    char *comp = "componentpower";
    int rc = ORCM_SUCCESS;

    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ON_FAILURE_RETURN(rc);
    rc = opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT);
    ON_FAILURE_RETURN(rc);

    comp = NULL;
    for(int i = 0; i < nsockets; ++i) {
        asprintf(&comp, "sensor_componentpower_%d", i+1);
        ON_NULL_RETURN(comp);
        rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
        SAFEFREE(comp);
        ON_FAILURE_RETURN(rc);

        asprintf(&comp, "cpu%d_power", i);
        ON_NULL_RETURN(comp);
        rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
        SAFEFREE(comp);
        ON_FAILURE_RETURN(rc);
    }

    comp = NULL;
    for(int i = 0; i < nsockets; ++i) {
        asprintf(&comp, "sensor_componentpower_%d", nsockets + i + 1);
        ON_NULL_RETURN(comp);
        rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
        SAFEFREE(comp);
        ON_FAILURE_RETURN(rc);

        asprintf(&comp, "ddr%d_power", i);
        ON_NULL_RETURN(comp);
        rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
        SAFEFREE(comp);
        ON_FAILURE_RETURN(rc);
    }
}

static void componentpower_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    if(mca_sensor_componentpower_component.test) {
        generate_test_inv_data(inventory_snapshot);
    } else {
        unsigned int tot_items = (unsigned int)(_rapl.n_sockets * 2) + 1; /* +1 is for "hostname"/nodename pair */
        const char *key = "componentpower";
        char *comp = NULL;
        int rc = OPAL_SUCCESS;
        int i = 0;

        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &key, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        /* store our hostname */
        key = "hostname";
        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &key, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &orte_process_info.nodename, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        for(i = 0; i < _rapl.n_sockets; ++i) {
            asprintf(&comp, "sensor_componentpower_%d", i+1);
            ON_NULL_RETURN(comp);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                SAFEFREE(comp);
                return;
            }
            SAFEFREE(comp);

            asprintf(&comp, "cpu%d_power", i);
            ON_NULL_RETURN(comp);
            orcm_sensor_base_runtime_metrics_track(mca_sensor_componentpower_component.runtime_metrics, comp);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                SAFEFREE(comp);
                return;
            }
            SAFEFREE(comp);
        }
        for(i = 0; i < _rapl.n_sockets; ++i) {
            asprintf(&comp, "sensor_componentpower_%d", i+_rapl.n_sockets+1);
            ON_NULL_RETURN(comp);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                SAFEFREE(comp);
                return;
            }
            SAFEFREE(comp);

            asprintf(&comp, "ddr%d_power", i);
            ON_NULL_RETURN(comp);
            orcm_sensor_base_runtime_metrics_track(mca_sensor_componentpower_component.runtime_metrics, comp);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                SAFEFREE(comp);
                return;
            }
            SAFEFREE(comp);
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
    orcm_value_t *time_stamp = NULL;
    struct timeval current_time;

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &tot_items, &n, OPAL_UINT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    gettimeofday(&current_time, NULL);
    time_stamp = orcm_util_load_orcm_value("ctime", &current_time, OPAL_TIMEVAL, NULL);
    if (NULL == time_stamp) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        return;
    }
    records = OBJ_NEW(opal_list_t);
    if (NULL == records) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        return;
    }
    opal_list_append(records, (opal_list_item_t*)time_stamp);
    while(0 < tot_items) {
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

        mkv = orcm_util_load_orcm_value(inv, inv_val, OPAL_STRING, NULL);
        ON_NULL_GOTO(mkv, cleanup);
        opal_list_append(records, (opal_list_item_t*)mkv);

        --tot_items;
    }
    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA, records, NULL, my_inventory_log_cleanup, NULL);
        goto do_exit;
    }

cleanup:
        my_inventory_log_cleanup(-1, -1, records, NULL, NULL);
do_exit:
    return;
}

int componentpower_enable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_componentpower_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, true, sensor_specification);
}

int componentpower_disable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_componentpower_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, false, sensor_specification);
}

int componentpower_reset_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_componentpower_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_reset(metrics, sensor_specification);
}
