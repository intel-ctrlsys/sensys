/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_var.h"

#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_ipmi.h"

/*
 * Local functions
 */

static int orcm_sensor_ipmi_open(void);
static int orcm_sensor_ipmi_close(void);
static int orcm_sensor_ipmi_query(mca_base_module_t **module, int *priority);
static int ipmi_component_register(void);

orcm_sensor_ipmi_component_t mca_sensor_ipmi_component = {
    {
        {
            ORCM_SENSOR_BASE_VERSION_1_0_0,
            
            "ipmi", /* MCA component name */
            ORCM_MAJOR_VERSION,  /* MCA component major version */
            ORCM_MINOR_VERSION,  /* MCA component minor version */
            ORCM_RELEASE_VERSION,  /* MCA component release version */
            orcm_sensor_ipmi_open,  /* component open  */
            orcm_sensor_ipmi_close, /* component close */
            orcm_sensor_ipmi_query,  /* component query */
            ipmi_component_register
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
        "ipmi"
    }
};

/**
  * component open/close/init function
  */
static int orcm_sensor_ipmi_open(void)
{
    return ORCM_SUCCESS;
}

static int orcm_sensor_ipmi_query(mca_base_module_t **module, int *priority)
{
    /* if we can build, then we definitely want to be used
     * even if we aren't going to sample as we have to be
     * present in order to log any received results
     */
    *priority = 50;  /* ahead of heartbeat */
    *module = (mca_base_module_t *)&orcm_sensor_ipmi_module;
    return ORCM_SUCCESS;
}

/**
 *  Close all subsystems.
 */

static int orcm_sensor_ipmi_close(void)
{
    return ORCM_SUCCESS;
}

static int ipmi_component_register(void)
{
    mca_base_component_t *c = &mca_sensor_ipmi_component.super.base_version;

     mca_sensor_ipmi_component.sensors = NULL;
    (void) mca_base_component_var_register (c, "sensors",
                                            "List of sensors to monitor",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_sensor_ipmi_component.sensors);
    return ORCM_SUCCESS;
}
