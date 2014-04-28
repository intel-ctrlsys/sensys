/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2012      Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/base/base.h"
#include "opal/util/output.h"
#include "opal/class/opal_pointer_array.h"

#include "sensor_heartbeat.h"

/*
 * Local functions
 */

static int orcm_sensor_heartbeat_open(void);
static int orcm_sensor_heartbeat_close(void);
static int orcm_sensor_heartbeat_query(mca_base_module_t **module, int *priority);

orcm_sensor_base_component_t mca_sensor_heartbeat_component = {
    {
        ORCM_SENSOR_BASE_VERSION_1_0_0,
            
        "heartbeat", /* MCA component name */
        ORCM_MAJOR_VERSION,  /* MCA component major version */
        ORCM_MINOR_VERSION,  /* MCA component minor version */
        ORCM_RELEASE_VERSION,  /* MCA component release version */
        orcm_sensor_heartbeat_open,  /* component open  */
        orcm_sensor_heartbeat_close, /* component close */
        orcm_sensor_heartbeat_query  /* component query */
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
    "heartbeat"
};


/**
  * component open/close/init function
  */
static int orcm_sensor_heartbeat_open(void)
{
    return ORCM_SUCCESS;
}


static int orcm_sensor_heartbeat_query(mca_base_module_t **module, int *priority)
{
    *priority = 5;  /* lower than all other samplers so that their data gets included in heartbeat */
    *module = (mca_base_module_t *)&orcm_sensor_heartbeat_module;
    return ORCM_SUCCESS;
}

/**
 *  Close all subsystems.
 */

static int orcm_sensor_heartbeat_close(void)
{
    return ORCM_SUCCESS;
}
