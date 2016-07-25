/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
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

#include "orcm/util/utils.h"
#include "orcm/runtime/orcm_globals.h"

#include "orcm/mca/analytics/analytics.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "sensor_udsensors.h"

extern "C" {
    #include "opal/runtime/opal_progress_threads.h"
}

BEGIN_C_DECLS
/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void udsensors_sample(orcm_sensor_sampler_t *sampler);
static void perthread_udsensors_sample(int fd, short args, void *cbdata);
void collect_udsensors_sample(orcm_sensor_sampler_t *sampler);
static void udsensors_log(opal_buffer_t *buf);
static void udsensors_set_sample_rate(int sample_rate);
static void udsensors_get_sample_rate(int *sample_rate);
static void udsensors_inventory_collect(opal_buffer_t *inventory_snapshot);
static void udsensors_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);
int udsensors_enable_sampling(const char* sensor_specification);
int udsensors_disable_sampling(const char* sensor_specification);
int udsensors_reset_sampling(const char* sensor_specification);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_udsensors_module = {
    init,
    finalize,
    start,
    stop,
    udsensors_sample,
    udsensors_log,
    udsensors_inventory_collect,
    udsensors_inventory_log,
    udsensors_set_sample_rate,
    udsensors_get_sample_rate,
    udsensors_enable_sampling,
    udsensors_disable_sampling,
    udsensors_reset_sampling
};

static orcm_sensor_sampler_t *udsensors_sampler = NULL;
static orcm_sensor_udsensors_t orcm_sensor_udsensors;

END_C_DECLS

static int init(void)
{
    int rc = ORCM_SUCCESS;

    mca_sensor_udsensors_component.diagnostics = 0;
    mca_sensor_udsensors_component.runtime_metrics =
        orcm_sensor_base_runtime_metrics_create("udsensors", orcm_sensor_base.collect_metrics,
                                                mca_sensor_udsensors_component.collect_metrics);

    return rc;
}

static void finalize(void)
{
}

/*
 * Start monitoring of local temps
 */
static void start(orte_jobid_t jobid)
{
    /* start a separate udsensors progress thread for sampling */
    if (mca_sensor_udsensors_component.use_progress_thread) {
        if (!orcm_sensor_udsensors.ev_active) {
            orcm_sensor_udsensors.ev_active = true;
            if (NULL == (orcm_sensor_udsensors.ev_base = opal_progress_thread_init("udsensors"))) {
                orcm_sensor_udsensors.ev_active = false;
                return;
            }
        }

        /* setup udsensors sampler */
        udsensors_sampler = OBJ_NEW(orcm_sensor_sampler_t);

        /* check if udsensors sample rate is provided for this*/
        if (!mca_sensor_udsensors_component.sample_rate) {
            mca_sensor_udsensors_component.sample_rate = orcm_sensor_base.sample_rate;
        }
        udsensors_sampler->rate.tv_sec = mca_sensor_udsensors_component.sample_rate;
        udsensors_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(orcm_sensor_udsensors.ev_base, &udsensors_sampler->ev,
                               perthread_udsensors_sample, udsensors_sampler);
        opal_event_evtimer_add(&udsensors_sampler->ev, &udsensors_sampler->rate);
    }else{
        mca_sensor_udsensors_component.sample_rate = orcm_sensor_base.sample_rate;
    }

    return;
}

static void stop(orte_jobid_t jobid)
{
    if (orcm_sensor_udsensors.ev_active) {
        orcm_sensor_udsensors.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_progress_thread_pause("udsensors");
        OBJ_RELEASE(udsensors_sampler);
    }
    return;
}

static void udsensors_sample(orcm_sensor_sampler_t *sampler)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor udsensors : udsensors_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    if (!mca_sensor_udsensors_component.use_progress_thread) {
       collect_udsensors_sample(sampler);
    }
}

static void perthread_udsensors_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor udsensors : perthread_udsensors_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* this has fired in the sampler thread, so we are okay to
     * just go ahead and sample since we do NOT allow both the
     * base thread and the component thread to both be actively
     * calling this component */
    collect_udsensors_sample(sampler);
    /* we now need to push the results into the base event thread
     * so it can add the data to the base bucket */
    ORCM_SENSOR_XFER(&sampler->bucket);
    /* clear the bucket */
    OBJ_DESTRUCT(&sampler->bucket);
    OBJ_CONSTRUCT(&sampler->bucket, opal_buffer_t);
    /* check if udsensors sample rate is provided for this*/
    if (mca_sensor_udsensors_component.sample_rate != sampler->rate.tv_sec) {
        sampler->rate.tv_sec = mca_sensor_udsensors_component.sample_rate;
    }
    /* set ourselves to sample again */
    opal_event_evtimer_add(&sampler->ev, &sampler->rate);
}

void collect_udsensors_sample(orcm_sensor_sampler_t *sampler)
{
    void* metrics_obj = mca_sensor_udsensors_component.runtime_metrics;

    if(!orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, NULL)) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor udsensors : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    mca_sensor_udsensors_component.diagnostics |= 0x1;
}

static void udsensors_log(opal_buffer_t *sample)
{
}

static void udsensors_set_sample_rate(int sample_rate)
{
    /* set the udsensors sample rate if seperate thread is enabled */
    if (mca_sensor_udsensors_component.use_progress_thread) {
        mca_sensor_udsensors_component.sample_rate = sample_rate;
    }
    return;
}

static void udsensors_get_sample_rate(int *sample_rate)
{
    if (NULL != sample_rate) {
    /* check if udsensors sample rate is provided for this*/
        *sample_rate = mca_sensor_udsensors_component.sample_rate;
    }
    return;
}

static void udsensors_inventory_collect(opal_buffer_t *inventory_snapshot)
{
}

static void udsensors_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
{
}

int udsensors_enable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_udsensors_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, true, sensor_specification);
}

int udsensors_disable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_udsensors_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, false, sensor_specification);
}

int udsensors_reset_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_udsensors_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_reset(metrics, sensor_specification);
}
