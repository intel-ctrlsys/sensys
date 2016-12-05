/*
 * Copyright (c) 2016  Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <iostream>
#include <stdio.h>

#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"

#include "sensor_ipmi_ts.h"
#include "ipmiSensorFactory.hpp"

#include "orcm/common/dataContainerHelper.hpp"

extern "C" {
    #include "orcm/util/utils.h"
    #include "orcm/runtime/orcm_globals.h"
    #include "orcm/mca/analytics/analytics.h"
    #include "orcm/mca/sensor/base/base.h"
    #include "orcm/mca/sensor/base/sensor_private.h"
    #include "opal/runtime/opal_progress_threads.h"
    #include "opal/mca/installdirs/installdirs.h"
    #include "orcm/mca/db/db.h"
}

BEGIN_C_DECLS
/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void ipmi_ts_sample(orcm_sensor_sampler_t *sampler);
static void perthread_ipmi_ts_sample(int fd, short args, void *cbdata);
void collect_ipmi_ts_sample(orcm_sensor_sampler_t *sampler);
static void ipmi_ts_log(opal_buffer_t *buf);
static void ipmi_ts_set_sample_rate(int sample_rate);
static void ipmi_ts_get_sample_rate(int *sample_rate);
static void ipmi_ts_inventory_collect(opal_buffer_t *inventory_snapshot);
static void ipmi_ts_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);
int ipmi_ts_enable_sampling(const char* sensor_specification);
int ipmi_ts_disable_sampling(const char* sensor_specification);
int ipmi_ts_reset_sampling(const char* sensor_specification);
static void ipmi_ts_fill_compute_data(opal_list_t* compute, dataContainer& cnt);
static void ipmi_ts_send_log_to_analytics(opal_list_t *key, opal_list_t *non_compute, opal_list_t *compute);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_ipmi_ts_module = {
    init,
    finalize,
    start,
    stop,
    ipmi_ts_sample,
    ipmi_ts_log,
    ipmi_ts_inventory_collect,
    ipmi_ts_inventory_log,
    ipmi_ts_set_sample_rate,
    ipmi_ts_get_sample_rate,
    ipmi_ts_enable_sampling,
    ipmi_ts_disable_sampling,
    ipmi_ts_reset_sampling
};

static orcm_sensor_sampler_t *ipmi_ts_sampler = NULL;
orcm_sensor_ipmi_ts_t orcm_sensor_ipmi_ts;
static struct ipmiSensorFactory *factory;

END_C_DECLS

static int init(void)
{
    int rc = ORCM_SUCCESS;
    factory = ipmiSensorFactory::getInstance();
    int pluginsLoaded = 0;

    mca_sensor_ipmi_ts_component.diagnostics = 0;
    mca_sensor_ipmi_ts_component.runtime_metrics =
        orcm_sensor_base_runtime_metrics_create("ipmi_ts", orcm_sensor_base.collect_metrics,
                                                mca_sensor_ipmi_ts_component.collect_metrics);

    try {
        factory->load(mca_sensor_ipmi_ts_component.test);
    } catch (ipmiSensorFactoryException& e){
        opal_output(0, "ERROR: %s ", e.what());
        rc = ORCM_ERROR;
    }

    if (ORCM_SUCCESS == rc){
        try {
            factory->init();
        } catch (ipmiSensorFactoryException& e){
            opal_output(0, "ERROR: %s ", e.what());
        }
    }

    pluginsLoaded = factory->getLoadedPlugins();

    if (0 == pluginsLoaded){
        rc = ORCM_ERROR;
    }

    return rc;
}

static void finalize(void)
{
     orcm_sensor_base_runtime_metrics_destroy(mca_sensor_ipmi_ts_component.runtime_metrics);
     mca_sensor_ipmi_ts_component.runtime_metrics = NULL;
     factory->close();
}

/*
 * Start monitoring of local temps
 */
static void start(orte_jobid_t jobid)
{
    /* start a separate ipmi_ts progress thread for sampling */
    if (mca_sensor_ipmi_ts_component.use_progress_thread) {
        if (!orcm_sensor_ipmi_ts.ev_active) {
            orcm_sensor_ipmi_ts.ev_active = true;
            if (NULL == (orcm_sensor_ipmi_ts.ev_base = opal_progress_thread_init("ipmi_ts"))) {
                orcm_sensor_ipmi_ts.ev_active = false;
                return;
            }
        }

        /* setup ipmi_ts sampler */
        ipmi_ts_sampler = OBJ_NEW(orcm_sensor_sampler_t);

        /* check if ipmi_ts sample rate is provided for this*/
        if (!mca_sensor_ipmi_ts_component.sample_rate) {
            mca_sensor_ipmi_ts_component.sample_rate = orcm_sensor_base.sample_rate;
        }
        ipmi_ts_sampler->rate.tv_sec = mca_sensor_ipmi_ts_component.sample_rate;
        ipmi_ts_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(orcm_sensor_ipmi_ts.ev_base, &ipmi_ts_sampler->ev,
                               perthread_ipmi_ts_sample, ipmi_ts_sampler);
        opal_event_evtimer_add(&ipmi_ts_sampler->ev, &ipmi_ts_sampler->rate);
    }else{
        mca_sensor_ipmi_ts_component.sample_rate = orcm_sensor_base.sample_rate;
    }
}

static void stop(orte_jobid_t jobid)
{
    if (orcm_sensor_ipmi_ts.ev_active) {
        orcm_sensor_ipmi_ts.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_progress_thread_pause("ipmi_ts");
        OBJ_RELEASE(ipmi_ts_sampler);
    }
    return;
}

static void ipmi_ts_sample(orcm_sensor_sampler_t *sampler)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor ipmi_ts : ipmi_ts_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    if (!mca_sensor_ipmi_ts_component.use_progress_thread) {
       collect_ipmi_ts_sample(sampler);
    }
}

static void perthread_ipmi_ts_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor ipmi_ts : perthread_ipmi_ts_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* this has fired in the sampler thread, so we are okay to
     * just go ahead and sample since we do NOT allow both the
     * base thread and the component thread to both be actively
     * calling this component */
    collect_ipmi_ts_sample(sampler);
    /* we now need to push the results into the base event thread
     * so it can add the data to the base bucket */
    ORCM_SENSOR_XFER(&sampler->bucket);
    /* clear the bucket */
    OBJ_DESTRUCT(&sampler->bucket);
    OBJ_CONSTRUCT(&sampler->bucket, opal_buffer_t);
    /* check if ipmi_ts sample rate is provided for this*/
    if (mca_sensor_ipmi_ts_component.sample_rate != sampler->rate.tv_sec) {
        sampler->rate.tv_sec = mca_sensor_ipmi_ts_component.sample_rate;
    }
    /* set ourselves to sample again */
    opal_event_evtimer_add(&sampler->ev, &sampler->rate);
}

void collect_ipmi_ts_sample(orcm_sensor_sampler_t *sampler)
{
    int rc = ORCM_SUCCESS;
    const char *name = "ipmi_ts";
    opal_buffer_t data, *bptr;
    bool packed = false;
    struct timeval current_time;
    dataContainerMap pluginsContent;
    dataContainerMap __pluginsContent;

    void* metrics_obj = mca_sensor_ipmi_ts_component.runtime_metrics;

    if(0 == orcm_sensor_base_runtime_metrics_active_label_count(metrics_obj) &&
        !orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, NULL)) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor ipmi_ts : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    mca_sensor_ipmi_ts_component.diagnostics |= 0x1;
    orcm_sensor_base_runtime_metrics_begin(metrics_obj);

    try {
        factory->sample(__pluginsContent);
    } catch (ipmiSensorFactoryException& e) {
        opal_output(0, "ERROR: %s ", e.what());
    }

    for (dataContainerMap::iterator it = __pluginsContent.begin() ; it != __pluginsContent.end() ; ++it)
    {
        if (orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, const_cast<char*>(it->first.c_str()))){
            pluginsContent[it->first] = it->second;
        }
    }

    /* prepare to store the results */
    OBJ_CONSTRUCT(&data, opal_buffer_t);

    if(!pluginsContent.empty())
    {
        /* pack our name */
        rc = opal_dss.pack(&data, &name, 1, OPAL_STRING);
        ORCM_ON_FAILURE_GOTO(rc,clean_sample);

        /* store our hostname */
        rc = opal_dss.pack(&data, &orcm_sensor_base.host_tag_value, 1, OPAL_STRING);
        ORCM_ON_FAILURE_GOTO(rc,clean_sample);

        /* get the sample time */
        gettimeofday(&current_time, NULL);
        rc = opal_dss.pack(&data, &current_time, 1, OPAL_TIMEVAL);
        ORCM_ON_FAILURE_GOTO(rc,clean_sample);

        /* Store the samplings from user-defined plugins */
        try{
            dataContainerHelper::serializeMap(pluginsContent, &data);
            packed = true;
        } catch (ErrOpal &e){
            opal_output(0, "ERROR: %s ", e.what());
        }
    }

    if (packed)
    {
        bptr = &data;
        rc = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
    }

clean_sample:
    orcm_sensor_base_runtime_metrics_end(metrics_obj);
    ORTE_ERROR_LOG(rc);
    OBJ_DESTRUCT(&data);

}


static void ipmi_ts_fill_compute_data(opal_list_t* compute, dataContainer& cnt)
{
    try {
        dataContainerHelper::dataContainerToList(cnt,(void *)compute);
    } catch (ErrOpal &e){
        opal_output(0, "ERROR: %s ", e.what());
    }

}

static void ipmi_ts_send_log_to_analytics(opal_list_t *key, opal_list_t *non_compute, opal_list_t *compute)
{
    orcm_analytics_value_t *analytics_vals = NULL;

    if (!opal_list_is_empty(compute)){
        /* send data to analytics */
        analytics_vals = orcm_util_load_orcm_analytics_value(key, non_compute, compute);
        orcm_analytics.send_data(analytics_vals);
    }
    SAFE_RELEASE(analytics_vals);
}


static void ipmi_ts_log(opal_buffer_t *sample)
{
    int rc = ORCM_SUCCESS;
    int32_t n = 0;
    char *hostname = NULL;
    struct timeval sampletime;
    opal_list_t *key = NULL;
    opal_list_t* compute = NULL;
    opal_list_t *non_compute = NULL;
    dataContainerMap pluginsContent;
    std::string prefix("ipmi_ts");
    std::string data_group;

    /* unpack the host this came from */
    n=1;
    rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc,clean_sample_log);

    /* unpack the sample time when the data was sampled */
    n = 1;
    rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_TIMEVAL);
    ORCM_ON_FAILURE_GOTO(rc,clean_sample_log);

    /*deserialize to get dataContainer map*/
    try{
        dataContainerHelper::deserializeMap(pluginsContent, sample);
    } catch (ErrOpal &e){
        opal_output(0, "ERROR: %s ", e.what());
        goto clean_sample_log;
    }

    /* fill with sensor plugins data */
    for (dataContainerMap::iterator it = pluginsContent.begin() ; it != pluginsContent.end() ; ++it){

        /* fill the key list with hostname and data_group */
        key = OBJ_NEW(opal_list_t);
        ORCM_ON_NULL_GOTO(key,clean_sample_log);

        /* fill bmc hostname*/
        rc = orcm_util_append_orcm_value(key,(char *)"hostname",const_cast<char*>(it->first.c_str()), OPAL_STRING, NULL);
        ORCM_ON_FAILURE_GOTO(rc,clean_sample_log);

        data_group.append(prefix);
        rc = orcm_util_append_orcm_value(key,(char *)"data_group",const_cast<char*>(data_group.c_str()), OPAL_STRING, NULL);
        ORCM_ON_FAILURE_GOTO(rc,clean_sample_log);

        /* fill non compute data list with time stamp */
        non_compute = OBJ_NEW(opal_list_t);
        ORCM_ON_NULL_GOTO(key,clean_sample_log);

        rc = orcm_util_append_orcm_value(non_compute,(char *)"ctime", &sampletime, OPAL_TIMEVAL, NULL);
        ORCM_ON_FAILURE_GOTO(rc,clean_sample_log);

        compute = OBJ_NEW(opal_list_t);
        ipmi_ts_fill_compute_data(compute, it->second);

        ipmi_ts_send_log_to_analytics(key, non_compute, compute);

        data_group.clear();
        ORCM_RELEASE(key);
        ORCM_RELEASE(non_compute);
        ORCM_RELEASE(compute);
   }

clean_sample_log:
    SAFEFREE(hostname);
    ORCM_RELEASE(key);
    ORCM_RELEASE(non_compute);
    ORCM_RELEASE(compute);
}


static void ipmi_ts_set_sample_rate(int sample_rate)
{
    /* set the ipmi_ts sample rate if seperate thread is enabled */
    if (mca_sensor_ipmi_ts_component.use_progress_thread) {
        mca_sensor_ipmi_ts_component.sample_rate = sample_rate;
    }
    return;
}

static void ipmi_ts_get_sample_rate(int *sample_rate)
{
    if (NULL != sample_rate)
    {
    /* check if ipmi_ts sample rate is provided for this*/
        *sample_rate = mca_sensor_ipmi_ts_component.sample_rate;
    }
    return;
}

static void ipmi_ts_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    int rc = ORCM_SUCCESS;
    const char *name = "ipmi_ts";
    opal_buffer_t data, *bptr;
    bool packed = false;
    struct timeval current_time;
    struct timeval time_stamp;
    dataContainerMap pluginsContent;
    dataContainerMap __pluginsContent;
    std::string prefix("sensor_ipmi_ts_");

    void* metrics_obj = mca_sensor_ipmi_ts_component.runtime_metrics;

    try
    {
        factory->collect_inventory(__pluginsContent);
    }
    catch (ipmiSensorFactoryException& e)
    {
        opal_output(0, "ERROR: %s ", e.what());
        return;
    }

    dataContainer *tmpdataContainer;
    std::string inventory_tag;
    for (dataContainerMap::iterator it = __pluginsContent.begin(); it != __pluginsContent.end(); ++it){
        orcm_sensor_base_runtime_metrics_track(metrics_obj, const_cast<char*>(it->first.c_str()));
        tmpdataContainer = new dataContainer;
        tmpdataContainer->put(prefix+it->first, it->first, "");
        for (dataContainer::iterator sensor = it->second.begin(); sensor != it->second.end(); ++sensor)
        {
            inventory_tag = prefix + it->first + std::string("_") + sensor->first;
            tmpdataContainer->put(inventory_tag, sensor->first, "");
        }
        if (tmpdataContainer->count())
            pluginsContent[it->first] = *tmpdataContainer;
        delete tmpdataContainer;
    }

    if (!pluginsContent.empty())
    {
        /* pack our name */
        rc = opal_dss.pack(inventory_snapshot, &name, 1, OPAL_STRING);
        ORCM_ON_FAILURE_GOTO(rc,clean_collect);

        /* get the sample time */
        gettimeofday(&current_time, NULL);
        rc = opal_dss.pack(inventory_snapshot, &current_time, 1, OPAL_TIMEVAL);
        ORCM_ON_FAILURE_GOTO(rc,clean_collect);

        /* Store the samplings from user-defined plugins */
        try
        {
            dataContainerHelper::serializeMap(pluginsContent, inventory_snapshot);
            packed = true;
        } catch (ErrOpal &e)
        {
            opal_output(0, "ERROR: %s ", e.what());
            return;
        }
    }

clean_collect:
    ORTE_ERROR_LOG(rc);
}

static void my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata)
{
    ORCM_RELEASE(kvs);
}

static void ipmi_ts_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
{
    int rc = ORCM_SUCCESS;
    int32_t n = 0;
    struct timeval current_time;
    dataContainerMap pluginsContent;
    opal_list_t *records = NULL;

    /* unpack the current time */
    n= 1;
    rc = opal_dss.unpack(inventory_snapshot, &current_time, &n, OPAL_TIMEVAL);
    ORCM_ON_FAILURE_RETURN(rc);

    /*deserialize to get dataContainer map*/
    try
    {
        dataContainerHelper::deserializeMap(pluginsContent, inventory_snapshot);
    }
    catch (ErrOpal &e)
    {
        opal_output(0, "ERROR: %s ", e.what());
        return;
    }

    if (!pluginsContent.empty())
    {
        records = OBJ_NEW(opal_list_t);

        rc = orcm_util_append_orcm_value(records, (char *)"hostname", hostname, OPAL_STRING, NULL);
        ORCM_ON_FAILURE_RETURN(rc);

        rc = orcm_util_append_orcm_value(records, (char *)"ctime", &current_time, OPAL_TIMEVAL, NULL);
        ORCM_ON_FAILURE_RETURN(rc);

        /* fill with sensor plugins inventory */
        for (dataContainerMap::iterator it = pluginsContent.begin() ; it != pluginsContent.end() ; ++it)
            ipmi_ts_fill_compute_data(records, it->second);

        if (0 <= orcm_sensor_base.dbhandle)
            orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA, records, NULL, my_inventory_log_cleanup, NULL);
        else
            my_inventory_log_cleanup(-1, -1, records, NULL, NULL);
    }
}

int ipmi_ts_enable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_ipmi_ts_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, true, sensor_specification);
}

int ipmi_ts_disable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_ipmi_ts_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, false, sensor_specification);
}

int ipmi_ts_reset_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_ipmi_ts_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_reset(metrics, sensor_specification);
}
