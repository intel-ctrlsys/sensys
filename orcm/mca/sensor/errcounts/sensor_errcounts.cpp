/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sensor_errcounts.h"
#include "errcounts.h"

BEGIN_C_DECLS

// Instance of plugin
errcounts_impl* implementation = NULL;

int errcounts_init_relay(void)
{
    if(NULL != implementation) {
        return ORCM_ERROR;
    } else {
        implementation = new errcounts_impl();
        return implementation->init();
    }
}

void errcounts_finalize_relay(void)
{
    if(NULL != implementation) {
        implementation->finalize();
        delete implementation;
        implementation = NULL;
    }
}

void errcounts_start_relay(orte_jobid_t jobid)
{
    if(NULL != implementation) {
        implementation->start(jobid);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void errcounts_stop_relay(orte_jobid_t jobid)
{
    if(NULL != implementation) {
        implementation->stop(jobid);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void errcounts_sample_relay(orcm_sensor_sampler_t *sampler)
{
    if(NULL != implementation) {
        implementation->sample(sampler);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void errcounts_log_relay(opal_buffer_t *sample)
{
    if(NULL != implementation) {
        implementation->log(sample);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void errcounts_set_sample_rate_relay(int sample_rate)
{
    if(NULL != implementation) {
        implementation->set_sample_rate(sample_rate);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void errcounts_get_sample_rate_relay(int *sample_rate)
{
    if(NULL != implementation) {
        implementation->get_sample_rate(sample_rate);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void errcounts_inventory_collect_relay(opal_buffer_t *inventory_snapshot)
{
    if(NULL != implementation) {
        implementation->inventory_collect(inventory_snapshot);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

void errcounts_inventory_log_relay(char *hostname, opal_buffer_t *inventory_snapshot)
{
    if(NULL != implementation) {
        implementation->inventory_log(hostname, inventory_snapshot);
    } else {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_AVAILABLE);
    }
}

END_C_DECLS
