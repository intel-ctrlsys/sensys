/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/sensor/snmp/sensor_snmp.h"
#include "orcm/mca/sensor/snmp/snmp.h"

BEGIN_C_DECLS

// Instance of plugin
snmp_impl *impl = NULL;

int snmp_init_relay(void)
{
    if(NULL != impl) {
        return ORCM_ERROR;
    } else {
        impl = new snmp_impl();
        if (NULL == impl) {
            ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
            return ORCM_ERROR;
        }
        return impl->init();
    }
}

void snmp_finalize_relay(void)
{
    if(NULL != impl) {
        impl->finalize();
        delete impl;
        impl = NULL;
    }
}

void snmp_start_relay(orte_jobid_t jobid)
{
    if(NULL != impl) {
        impl->start(jobid);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void snmp_stop_relay(orte_jobid_t jobid)
{
    if(NULL != impl) {
        impl->stop(jobid);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void snmp_sample_relay(orcm_sensor_sampler_t *sampler)
{
    if(NULL != impl) {
        impl->sample(sampler);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void snmp_log_relay(opal_buffer_t *sample)
{
    if(NULL != impl) {
        impl->log(sample);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void snmp_set_sample_rate_relay(int sample_rate)
{
    if(NULL != impl) {
        impl->set_sample_rate(sample_rate);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void snmp_get_sample_rate_relay(int *sample_rate)
{
    if(NULL != impl) {
        impl->get_sample_rate(sample_rate);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

int snmp_enable_sampling_relay(const char* sensor_specification)
{
    if(NULL != impl) {
        return impl->enable_sampling(sensor_specification);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
        return ORCM_ERR_NOT_AVAILABLE;
    }
}

int snmp_disable_sampling_relay(const char* sensor_specification)
{
    if(NULL != impl) {
        return impl->disable_sampling(sensor_specification);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
        return ORCM_ERR_NOT_AVAILABLE;
    }
}

int snmp_reset_sampling_relay(const char* sensor_specification)
{
    if(NULL != impl) {
        return impl->reset_sampling(sensor_specification);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
        return ORCM_ERR_NOT_AVAILABLE;
    }
}

void snmp_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    if(NULL != impl){
        impl->inventory_collect(inventory_snapshot);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void snmp_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
{
    if(NULL != impl){
        impl->inventory_log(hostname, inventory_snapshot);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

END_C_DECLS;