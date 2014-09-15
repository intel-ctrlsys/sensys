/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
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

#define HAVE_HWLOC_DIFF  // protect the hwloc diff.h file from ipmicmd.h conflict
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
#include "sensor_nodepower.h"

#include <ipmicmd.h>

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void nodepower_sample(orcm_sensor_sampler_t *sampler);
static void nodepower_log(opal_buffer_t *buf);
static int call_readein(node_power_data *, int);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_nodepower_module = {
    init,
    finalize,
    start,
    stop,
    nodepower_sample,
    nodepower_log
};

__readein _readein;
__time_val _tv;

int init_done=0;

node_power_data _node_power, node_power;

/*
use read_ein command to get input power of PSU. refer to PSU spec for details.
 */
static int call_readein(node_power_data *data, int to_print)
{
    unsigned char responseData[1024];
    int responseLength = 1024;
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
    cmd[5]=(unsigned char)0xb0;
    cmd[6]=(unsigned char)0x07;
    cmd[7]=(unsigned char)0x86;
    len=8;

    netfn=cmd[2]>>2;
    lun=cmd[2]&0x03;

    ret = ipmi_cmdraw(cmd[3], netfn, cmd[1], cmd[0], lun, &cmd[4], (unsigned char)(len-4), responseData, &responseLength, &completionCode, 0);

    if(ret) {
        error_string = decode_rv(ret);
        opal_output(0,"Unable to reach IPMI device for node: %s",cap->node.name );
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

    if (to_print){
        opal_output(0, "ret_val[0]=%lu, ret_val[1]=%lu\n", data->ret_val[0], data->ret_val[1]);
    }
    ipmi_close();

    return ORCM_SUCCESS;
}

static bool log_enabled = true;

static int init(void)
{
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

    gettimeofday(&(_tv.tv_curr), NULL);
    _tv.tv_prev=_tv.tv_curr;
    _tv.interval=0;

    ret=call_readein(&_node_power, 0);
    if (ret==ORCM_ERROR){
        opal_output(0,"Unable to reach IPMI device for node: %s",cap->node.name );
        _readein.readein_accu_prev=0;
        _readein.readein_cnt_prev=0;
        return;
    }

    _readein.readein_accu_prev=_node_power.ret_val[0];
    _readein.readein_cnt_prev=_node_power.ret_val[1];
    _readein.ipmi_calls=0;

    return;
}


/*
 stop nodepower plug-in
 */
static void stop(orte_jobid_t jobid)
{
    return;
}

/*
 sample nodepower every X seconds
 */
static void nodepower_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    char *freq;
    opal_buffer_t data, *bptr;
    time_t now;
    char time_str[40];
    char *timestamp_str;
    bool packed;

    unsigned long val1, val2;
    float node_power_cur;
    struct tm *sample_time;

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

    ret=call_readein(&_node_power, 0);
    _readein.ipmi_calls++;
    if (ret==ORCM_ERROR){
        opal_output(0,"Unable to reach IPMI device for node: %s",cap->node.name );
        _readein.readein_accu_curr=_readein.readein_accu_prev;
        _readein.readein_cnt_curr=_readein.readein_cnt_prev;
    } else {
        _readein.readein_accu_curr=_node_power.ret_val[0];
        _readein.readein_cnt_curr=_node_power.ret_val[1];
    }

    /* detect overflow and calculate psu power */
    if (_readein.readein_accu_curr >= _readein.readein_accu_prev){
        val1=_readein.readein_accu_curr - _readein.readein_accu_prev;
    } else {
        val1=_readein.readein_accu_curr + 32768 - _readein.readein_accu_prev;
    }

    if (_readein.readein_cnt_curr >= _readein.readein_cnt_prev){
        val2=_readein.readein_cnt_curr - _readein.readein_cnt_prev;
    } else {
        val2=_readein.readein_cnt_curr + 65536 - _readein.readein_cnt_prev;
    }

    if (!val2){
        val2=1;
    }

    node_power.node_power.cur=(double)val1/(double)val2;
    if (node_power.node_power.cur>1000.0){
        node_power.node_power.cur=-1.0;
    }

    _readein.readein_accu_prev=_readein.readein_accu_curr;
    _readein.readein_cnt_prev=_readein.readein_cnt_curr;

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
 nodepower to be picked up by heartbeat
 */
static void nodepower_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    char *sampletime;
    int rc;
    int32_t n;
    opal_list_t *vals;
    opal_value_t *kv;

    float node_power_cur;

    if (!log_enabled) {
        return;
    }

    /* unpack the host this came from */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
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
                        "%s Received freq log from host %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname);

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
    if (hostname==NULL){
      kv->data.string = strdup("NULL");
    } else {
      kv->data.string = strdup(hostname);
    }
    opal_list_append(vals, &kv->super);

    kv = OBJ_NEW(opal_value_t);
    kv->key=strdup("nodepower");
    kv->type=OPAL_FLOAT;
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &node_power_cur, &n, OPAL_FLOAT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    kv->data.fval=node_power_cur;
    opal_list_append(vals, &kv->super);

    /* store it */
    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store(orcm_sensor_base.dbhandle, "nodepower", vals, mycleanup, NULL);
    } else {
        OPAL_LIST_RELEASE(vals);
    }

    if (NULL != hostname) {
        free(hostname);
    }
}
