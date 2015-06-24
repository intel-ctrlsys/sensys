/*
 * Copyright (c) 2014-2015  Intel, Inc. All rights reserved.
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

#define HAVE_HWLOC_DIFF  // protect the hwloc diff.h file from ipmicmd.h conflict
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

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_nodepower.h"

#include <ipmicmd.h>

#define MAX_IPMI_RESPONSE 1024
/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void nodepower_sample(orcm_sensor_sampler_t *sampler);
static void perthread_nodepower_sample(int fd, short args, void *cbdata);
static void collect_sample(orcm_sensor_sampler_t *sampler);
static void nodepower_log(opal_buffer_t *buf);
static int call_readein(node_power_data *, int, unsigned char);
static void nodepower_set_sample_rate(int sample_rate);
static void nodepower_get_sample_rate(int *sample_rate);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_nodepower_module = {
    init,
    finalize,
    start,
    stop,
    nodepower_sample,
    nodepower_log,
    NULL,
    NULL,
    nodepower_set_sample_rate,
    nodepower_get_sample_rate
};

__readein _readein;
__time_val _tv;

int init_done=0;

static orcm_sensor_sampler_t *nodepower_sampler = NULL;
static orcm_sensor_nodepower_t orcm_sensor_nodepower;
node_power_data _node_power, node_power;

static void generate_test_vector(opal_buffer_t *v);

/*
use read_ein command to get input power of PSU. refer to PSU spec for details.
 */
static int call_readein(node_power_data *data, int to_print, unsigned char psu)
{
    unsigned char responseData[MAX_IPMI_RESPONSE];
    int responseLength = MAX_IPMI_RESPONSE;
    unsigned char completionCode;
    int ret, i, len;
    unsigned char netfn, lun;
    unsigned char cmd[16];
    unsigned char temp_value[8];
    char *error_string;

/* parameters needed to read psu power */
/* bus # */
    cmd[0]=(unsigned char)0x00;
/* sa */
    cmd[1]=(unsigned char)0x20;
/* netfn and lun */
    cmd[2]=(unsigned char)0x18;
/* command */
    cmd[3]=(unsigned char)0x52;
/* pointer to ipmi data (4 bytes) */
    cmd[4]=(unsigned char)0x0f;
    cmd[5]=psu;
    cmd[6]=(unsigned char)0x07;
    cmd[7]=(unsigned char)0x86;
    len=8;

    netfn=cmd[2]>>2;
    lun=cmd[2]&0x03;

    ret = ipmi_cmdraw(cmd[3], netfn, cmd[1], cmd[0], lun, &cmd[4], (unsigned char)(len-4), responseData, &responseLength, &completionCode, 0);

    if(ret) {
        error_string = decode_rv(ret);
        opal_output(0,"ipmi_cmdraw ERROR : %s \n", error_string);
        ipmi_close();
        return ORCM_ERROR;
    }

    if (to_print){
        opal_output(0, "ret=%d, respData[len=%d]: ",ret, responseLength);
        for (i = 0; i < responseLength; i++){
           opal_output(0, "%02x ",responseData[i]);
        }
        opal_output(0, "\n");
    }

    if (completionCode==(unsigned char)0){
        data->str_len=responseLength;
        for (i=0; i<responseLength; i++){
            data->raw_string[i]=responseData[i];
        }

        temp_value[0]=responseData[1];
        temp_value[1]=responseData[2];
        temp_value[1]&=0x7f;
        temp_value[2]=responseData[3];
        if (temp_value[2]&0x1){
            temp_value[1]|=0x80;
        }
        temp_value[2]>>=1;
        data->ret_val[0]=*((unsigned long *)temp_value);

        memset(temp_value, 0, 8);
        temp_value[0]=responseData[4];
        temp_value[1]=responseData[5];
        temp_value[2]=responseData[6];
        data->ret_val[1]=*((unsigned long *)temp_value);
    } else {
        memset(data->raw_string, 0, 7);
        data->ret_val[0]=0;
        data->ret_val[1]=0;
    }

    if (to_print){
        opal_output(0, "ret_val[0]=%lu, ret_val[1]=%lu\n", data->ret_val[0], data->ret_val[1]);
    }

    ipmi_close();

    return ORCM_SUCCESS;
}

static bool log_enabled = true;

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
}

/*
 start nodepower plug-in
 */
static void start(orte_jobid_t jobid)
{
    int ret;

    /* we must be root to run */
    if (0 != geteuid()) {
        return;
    }

    gettimeofday(&(_tv.tv_curr), NULL);
    _tv.tv_prev=_tv.tv_curr;
    _tv.interval=0;

    ret=call_readein(&_node_power, 0, NODEPOWER_PA_R);
    if (ret==ORCM_ERROR){
        opal_output(0,"Unable to read Nodepower");
        _readein.readein_a_accu_prev=0;
        _readein.readein_a_cnt_prev=0;
        return;
    }

    _readein.readein_a_accu_prev=_node_power.ret_val[0];
    _readein.readein_a_cnt_prev=_node_power.ret_val[1];

    ret=call_readein(&_node_power, 0, NODEPOWER_PB_R);
    if (ret==ORCM_ERROR){
        opal_output(0,"Unable to read Nodepower");
        _readein.readein_b_accu_prev=0;
        _readein.readein_b_cnt_prev=0;
        return;
    }

    _readein.readein_b_accu_prev=_node_power.ret_val[0];
    _readein.readein_b_cnt_prev=_node_power.ret_val[1];

    _readein.ipmi_calls=2;
    /* start a separate nodepower progress thread for sampling */
    if (mca_sensor_nodepower_component.use_progress_thread) {
        if (!orcm_sensor_nodepower.ev_active) {
            orcm_sensor_nodepower.ev_active = true;
            if (NULL == (orcm_sensor_nodepower.ev_base = opal_start_progress_thread("nodepower", true))) {
                orcm_sensor_nodepower.ev_active = false;
                return;
            }
        }

        /* setup nodepower sampler */
        nodepower_sampler = OBJ_NEW(orcm_sensor_sampler_t);

        /* check if nodepower sample rate is provided for this*/
        if (!mca_sensor_nodepower_component.sample_rate) {
            mca_sensor_nodepower_component.sample_rate = orcm_sensor_base.sample_rate;
        }
        nodepower_sampler->rate.tv_sec = mca_sensor_nodepower_component.sample_rate;
        nodepower_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(orcm_sensor_nodepower.ev_base, &nodepower_sampler->ev,
                               perthread_nodepower_sample, nodepower_sampler);
        opal_event_evtimer_add(&nodepower_sampler->ev, &nodepower_sampler->rate);
    }
    return;
}


/*
 stop nodepower plug-in
 */
static void stop(orte_jobid_t jobid)
{
    if (orcm_sensor_nodepower.ev_active) {
        orcm_sensor_nodepower.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_stop_progress_thread("nodepower", false);
        OBJ_RELEASE(nodepower_sampler);
    }
    return;
}

/*
 sample nodepower every X seconds
 */
static void nodepower_sample(orcm_sensor_sampler_t *sampler)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor nodepower : nodepower_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    if (!mca_sensor_nodepower_component.use_progress_thread) {
       collect_sample(sampler);
    }

}

static void perthread_nodepower_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor nodepower : perthread_nodepower_sample: called",
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
    /* check if nodepower sample rate is provided for this*/
    if (mca_sensor_nodepower_component.sample_rate != sampler->rate.tv_sec) {
        sampler->rate.tv_sec = mca_sensor_nodepower_component.sample_rate;
    } 
    /* set ourselves to sample again */
    opal_event_evtimer_add(&sampler->ev, &sampler->rate);
}

static void collect_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    char *freq;
    opal_buffer_t data, *bptr;
    bool packed;

    unsigned long val1, val2;
    float node_power_cur;

    /* we must be root to run */
    if (0 != geteuid()) {
        return;
    }

    if (mca_sensor_nodepower_component.test) {

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
    _tv.tv_prev=_tv.tv_curr;

    ret=call_readein(&_node_power, 0, NODEPOWER_PA_R);
    _readein.ipmi_calls++;
    if (ret==ORCM_ERROR){
        opal_output(0,"Unable to read Nodepower");
        _readein.readein_a_accu_curr=_readein.readein_a_accu_prev;
        _readein.readein_a_cnt_curr=_readein.readein_a_cnt_prev;
    } else {
        _readein.readein_a_accu_curr=_node_power.ret_val[0];
        _readein.readein_a_cnt_curr=_node_power.ret_val[1];
    }

    /* detect overflow and calculate psu power */
    if (_readein.readein_a_accu_curr >= _readein.readein_a_accu_prev){
        val1=_readein.readein_a_accu_curr - _readein.readein_a_accu_prev;
    } else {
        val1=_readein.readein_a_accu_curr + 32768 - _readein.readein_a_accu_prev;
    }

    if (_readein.readein_a_cnt_curr >= _readein.readein_a_cnt_prev){
        val2=_readein.readein_a_cnt_curr - _readein.readein_a_cnt_prev;
    } else {
        val2=_readein.readein_a_cnt_curr + 65536 - _readein.readein_a_cnt_prev;
    }

    if (!val2){
        val2=1;
    }

    node_power.power_a.cur=(double)val1/(double)val2;
    if (node_power.power_a.cur>1000.0){
        node_power.power_a.cur=-1.0;
    }

    _readein.readein_a_accu_prev=_readein.readein_a_accu_curr;
    _readein.readein_a_cnt_prev=_readein.readein_a_cnt_curr;

    ret=call_readein(&_node_power, 0, NODEPOWER_PB_R);
    _readein.ipmi_calls++;
    if (ret==ORCM_ERROR){
        opal_output(0,"Unable to read Nodepower");
        _readein.readein_b_accu_curr=_readein.readein_b_accu_prev;
        _readein.readein_b_cnt_curr=_readein.readein_b_cnt_prev;
    } else {
        _readein.readein_b_accu_curr=_node_power.ret_val[0];
        _readein.readein_b_cnt_curr=_node_power.ret_val[1];
    }

    /* detect overflow and calculate psu power */
    if (_readein.readein_b_accu_curr >= _readein.readein_b_accu_prev){
        val1=_readein.readein_b_accu_curr - _readein.readein_b_accu_prev;
    } else {
        val1=_readein.readein_b_accu_curr + 32768 - _readein.readein_b_accu_prev;
    }

    if (_readein.readein_b_cnt_curr >= _readein.readein_b_cnt_prev){
        val2=_readein.readein_b_cnt_curr - _readein.readein_b_cnt_prev;
    } else {
        val2=_readein.readein_b_cnt_curr + 65536 - _readein.readein_b_cnt_prev;
    }

    if (!val2){
        val2=1;
    }

    node_power.power_b.cur=(double)val1/(double)val2;
    if (node_power.power_b.cur>1000.0){
        node_power.power_b.cur=-1.0;
    }

    node_power.node_power.cur=node_power.power_a.cur+node_power.power_b.cur;

    _readein.readein_b_accu_prev=_readein.readein_b_accu_curr;
    _readein.readein_b_cnt_prev=_readein.readein_b_cnt_curr;

    /* prep to store the results */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    packed = false;

    /* pack our name */
    freq = strdup("nodepower");
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

    /* get the sample time */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &(_tv.tv_curr), 1, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }

    if (_readein.ipmi_calls <=2){
        node_power_cur=0.0;
    } else{
        node_power_cur=(float)(node_power.node_power.cur);
    }
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &node_power_cur, 1, OPAL_FLOAT))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
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

static void mycleanup(int dbhandle, int status,
                      opal_list_t *kvs, void *cbdata)
{
    OPAL_LIST_RELEASE(kvs);
    if (ORTE_SUCCESS != status) {
        log_enabled = false;
    }
}

/*
 nodepower to be picked up by heartbeat
 */
static void nodepower_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    int rc;
    int32_t n;
    opal_list_t *vals;
    opal_value_t *kv;
    int sensor_not_avail=0;
    struct timeval tv_curr;
    struct tm *time_info;

    float node_power_cur;
    char time_str[40];

    if (!log_enabled) {
        return;
    }

    /* unpack the host this came from */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    /* unpack timestamp */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &tv_curr, &n, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &node_power_cur, &n, OPAL_FLOAT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "%s Received freq log from host %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname);

    /* xfr to storage */
    vals = OBJ_NEW(opal_list_t);

    /* load the sample time at the start */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("ctime");
    kv->type = OPAL_TIMEVAL;
    kv->data.tv=tv_curr;

    time_info=localtime(&(tv_curr.tv_sec));
    if (NULL == time_info) {
        sensor_not_avail=1;
        opal_output(0,"nodepower sensor data not logged due to error of localtime()\n");
    } else {
        strftime(time_str, sizeof(time_str), "%F %T%z", time_info);
    }

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                       "second=%s\n", time_str);

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                       "sub-second=%.3f\n", (float)(kv->data.tv.tv_usec)/1000000.0);
    opal_list_append(vals, &kv->super);

    /* load the hostname */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("hostname");
    kv->type = OPAL_STRING;
    if (hostname==NULL){
      kv->data.string = strdup("NULL");
    } else {
      kv->data.string = strdup(hostname);
    }
    opal_list_append(vals, &kv->super);

    kv = OBJ_NEW(opal_value_t);
    kv->key=strdup("nodepower:W");
    kv->type=OPAL_FLOAT;
    kv->data.fval=node_power_cur;
    if (node_power_cur<=(float)(0.0)){
        sensor_not_avail=1;
        if (_readein.ipmi_calls>4)
            opal_output(0,"nodepower sensor data not logged due to unexpected return value from PSU\n");
    } else {
        opal_list_append(vals, &kv->super);
    }

/*
 *
don't send as a string. send it as a time_val.
take advantage of existing time_val field of opal_value_t
 *
 */
    /* store it */
    if (0 <= orcm_sensor_base.dbhandle) {
        if (!sensor_not_avail){
            orcm_db.store(orcm_sensor_base.dbhandle, "nodepower", vals, mycleanup, NULL);
        }
    } else {
        OPAL_LIST_RELEASE(vals);
    }

    if (NULL != hostname) {
        free(hostname);
    }
}

static void nodepower_set_sample_rate(int sample_rate)
{
    /* set the nodepower sample rate if seperate thread is enabled */
    if (mca_sensor_nodepower_component.use_progress_thread) {
        mca_sensor_nodepower_component.sample_rate = sample_rate;
    }
    return;
}

static void nodepower_get_sample_rate(int *sample_rate)
{
    if (NULL != sample_rate) {
    /* check if nodepower sample rate is provided for this*/
        if (mca_sensor_nodepower_component.use_progress_thread) {
            *sample_rate = mca_sensor_nodepower_component.sample_rate;
        }
    }
    return;
}

static void generate_test_vector(opal_buffer_t *v)
{
    int ret;
    char *ctmp;
    float test_power;
    struct timeval tv_test;

/* Power units are Watts */
    test_power = 100;

/* pack the plugin name */
    ctmp = strdup("nodepower");
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

/* get the time of sampling */
   gettimeofday(&tv_test,NULL);
/* pack the sampling time */
    if (OPAL_SUCCESS != (ret =
                opal_dss.pack(v, &tv_test, 1, OPAL_TIMEVAL))){
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&v);
        return;
    }

/* pack the test power value */
    if (OPAL_SUCCESS != (ret =
                opal_dss.pack(v, &test_power, 1, OPAL_FLOAT))){
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&v);
        return;
    }

    opal_output_verbose(5,orcm_sensor_base_framework.framework_output,
            "%s sensor:nodepower: Power value of test vector is %f Watts",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),test_power);
}
