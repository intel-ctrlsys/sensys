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
static void ipmi_ts_set_sample_rate(int sample_rate);
static void ipmi_ts_get_sample_rate(int *sample_rate);
static void ipmi_ts_inventory_collect(opal_buffer_t *inventory_snapshot);
int ipmi_ts_enable_sampling(const char* sensor_specification);
int ipmi_ts_disable_sampling(const char* sensor_specification);
int ipmi_ts_reset_sampling(const char* sensor_specification);
static void ipmi_ts_fill_compute_data(opal_list_t* compute, dataContainer* cnt);
static void my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata);
static void ipmi_ts_send_log_to_analytics(opal_list_t *key, opal_list_t *non_compute, opal_list_t *compute);
static void ipmi_ts_log_inventory_content(std::string hostname, dataContainer* dc);
static void ipmi_ts_log_sampling_content(std::string hostname, dataContainer* dc);
void ipmi_ts_output_error_messages(string bmc, string errorMessage, string completionMessage);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_ipmi_ts_module = {
    init,
    finalize,
    start,
    stop,
    ipmi_ts_sample,
    NULL, // Logging will be performed by a callback function
    ipmi_ts_inventory_collect,
    NULL, // Logging will be performed by a callback function
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
    char* hostname = NULL;

    if (!ORCM_PROC_IS_AGGREGATOR)
    {
        opal_output(0, "ERROR: Running ipmi_ts should only be done in an aggregator.");
        return ORCM_ERROR;
    }

    if (NULL == (hostname = orcm_get_proc_hostname())) {
        hostname = (char*) "localhost";
    }

    int rc = ORCM_SUCCESS;
    factory = ipmiSensorFactory::getInstance();
    int pluginsLoaded = 0;

    mca_sensor_ipmi_ts_component.diagnostics = 0;
    mca_sensor_ipmi_ts_component.runtime_metrics =
        orcm_sensor_base_runtime_metrics_create("ipmi_ts", orcm_sensor_base.collect_metrics,
                                                mca_sensor_ipmi_ts_component.collect_metrics);

    try {
        factory->load(mca_sensor_ipmi_ts_component.test, hostname);
    } catch (ipmiSensorFactoryException& e){
        opal_output(0, "ERROR: %s ", e.what());
        rc = ORCM_ERROR;
    }

    if (ORCM_SUCCESS == rc){
        try {
            factory->init();
            factory->setCallbackPointers(ipmi_ts_log_sampling_content,
                                         ipmi_ts_log_inventory_content,
                                         ipmi_ts_output_error_messages);
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
    if (!ORCM_PROC_IS_AGGREGATOR)
        return;

     orcm_sensor_base_runtime_metrics_destroy(mca_sensor_ipmi_ts_component.runtime_metrics);
     mca_sensor_ipmi_ts_component.runtime_metrics = NULL;
     factory->close();

    ipmiHAL *instance = ipmiHAL::getInstance();
    instance->terminateInstance();
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

// This function will on
void collect_ipmi_ts_sample(orcm_sensor_sampler_t *sampler) {
    void *metrics_obj = mca_sensor_ipmi_ts_component.runtime_metrics;

    if (0 == orcm_sensor_base_runtime_metrics_active_label_count(metrics_obj) &&
        !orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, NULL)) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor ipmi_ts : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    mca_sensor_ipmi_ts_component.diagnostics |= 0x1;
    orcm_sensor_base_runtime_metrics_begin(metrics_obj);

    try {
        factory->sample();
    } catch (ipmiSensorFactoryException &e) {
        opal_output(0, "ERROR: %s ", e.what());
    }
}

void ipmi_ts_log_sampling_content(std::string hostname, dataContainer* dc)
{
    int rc = ORCM_SUCCESS;
    struct timeval sampletime;
    opal_list_t *key = NULL;
    opal_list_t* compute = NULL;
    opal_list_t *non_compute = NULL;
    std::string prefix("ipmi_ts");

    if (NULL == dc)
        return;

    void *metrics_obj = mca_sensor_ipmi_ts_component.runtime_metrics;
    if (!orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, const_cast<char*>(hostname.c_str())))
    {
        return;
    }

    gettimeofday(&sampletime, NULL);

    /* fill the key list with hostname and data_group */
    key = OBJ_NEW(opal_list_t);
    ORCM_ON_NULL_GOTO(key,clean_sample_log);

    /* fill bmc hostname*/
    rc = orcm_util_append_orcm_value(key,(char *)"hostname",const_cast<char*>(hostname.c_str()), OPAL_STRING, NULL);
    ORCM_ON_FAILURE_GOTO(rc,clean_sample_log);

    rc = orcm_util_append_orcm_value(key,(char *)"data_group",const_cast<char*>(prefix.c_str()), OPAL_STRING, NULL);
    ORCM_ON_FAILURE_GOTO(rc,clean_sample_log);

    /* fill non compute data list with time stamp */
    non_compute = OBJ_NEW(opal_list_t);
    ORCM_ON_NULL_GOTO(non_compute,clean_sample_log);

    rc = orcm_util_append_orcm_value(non_compute,(char *)"ctime", &sampletime, OPAL_TIMEVAL, NULL);
    ORCM_ON_FAILURE_GOTO(rc,clean_sample_log);

    compute = OBJ_NEW(opal_list_t);
    ipmi_ts_fill_compute_data(compute, dc);

    ipmi_ts_send_log_to_analytics(key, non_compute, compute);

clean_sample_log:
    ORCM_RELEASE(key);
    ORCM_RELEASE(non_compute);
    ORCM_RELEASE(compute);
}


static void ipmi_ts_fill_compute_data(opal_list_t* compute, dataContainer *cnt)
{
    try {
        dataContainerHelper::dataContainerToList(*cnt,(void *)compute);
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

static void ipmi_ts_inventory_collect(opal_buffer_t *inventory_snapshot) {
    try {
        factory->collect_inventory();
    }
    catch (ipmiSensorFactoryException &e) {
        opal_output(0, "ERROR: %s ", e.what());
        return;
    }
}

void ipmi_ts_log_inventory_content(std::string hostname, dataContainer* dc)
{
    int rc = ORCM_SUCCESS;
    std::string inventory_tag;
    struct timeval current_time;
    std::string prefix("sensor_ipmi_ts_");
    opal_list_t *records = NULL;

    if (NULL == dc)
        return;

    gettimeofday(&current_time, NULL);

    void *metrics_obj = mca_sensor_ipmi_ts_component.runtime_metrics;

    orcm_sensor_base_runtime_metrics_track(metrics_obj, const_cast<char*>(hostname.c_str()));

    records = OBJ_NEW(opal_list_t);

    rc = orcm_util_append_orcm_value(records,
                                     (char *)"hostname",
                                     const_cast<char*>(hostname.c_str()),
                                     OPAL_STRING,
                                     NULL);
    ORCM_ON_FAILURE_RETURN(rc);

    rc = orcm_util_append_orcm_value(records, (char *)"ctime", &current_time, OPAL_TIMEVAL, NULL);
    ORCM_ON_FAILURE_RETURN(rc);

    /* fill with sensor plugins inventory */
    ipmi_ts_fill_compute_data(records, dc);

    if (0 <= orcm_sensor_base.dbhandle)
        orcm_db.store_new(orcm_sensor_base.dbhandle,
                          ORCM_DB_INVENTORY_DATA,
                          records,
                          NULL,
                          my_inventory_log_cleanup,
                          NULL);
    else
        my_inventory_log_cleanup(-1, -1, records, NULL, NULL);
}

static void my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata)
{
    ORCM_RELEASE(kvs);
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

void ipmi_ts_output_error_messages(string bmc, string errorMessage, string completionMessage)
{
    opal_output(0, "ERROR in BMC %s: %s ", bmc.c_str(), errorMessage.c_str());

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor ipmi_ts on BMC %s:\n    ERROR: %s\n    Completion Message: %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        bmc.c_str(),
                        errorMessage.c_str(),
                        completionMessage.c_str());
}
