/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_ipmi.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void ipmi_sample(void);
static void ipmi_log(opal_buffer_t *buf);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_ipmi_module = {
    init,
    finalize,
    start,
    stop,
    ipmi_sample,
    ipmi_log
};

/* local variables */

static int init(void)
{
    return ORCM_SUCCESS;
}

static void finalize(void)
{
}

/*
 * Start monitoring of local processes
 */
static void start(orte_jobid_t jobid)
{
    return;
}


static void stop(orte_jobid_t jobid)
{
    return;
}

static void ipmi_sample(void)
{
    /* this is currently a no-op */
    opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                        "IPMI sensors not currently implemented");
}

static void ipmi_log(opal_buffer_t *sample)
{
}
