/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_var.h"

#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_componentpower.h"

/*
 * Local functions
 */

static int orcm_sensor_componentpower_open(void);
static int orcm_sensor_componentpower_close(void);
static int orcm_sensor_componentpower_query(mca_base_module_t **module, int *priority);
static int componentpower_component_register(void);

orcm_sensor_componentpower_component_t mca_sensor_componentpower_component = {
    {
        {
            ORCM_SENSOR_BASE_VERSION_1_0_0,
            
            "componentpower", /* MCA component name */
            ORCM_MAJOR_VERSION,  /* MCA component major version */
            ORCM_MINOR_VERSION,  /* MCA component minor version */
            ORCM_RELEASE_VERSION,  /* MCA component release version */
            orcm_sensor_componentpower_open,  /* component open  */
            orcm_sensor_componentpower_close, /* component close */
            orcm_sensor_componentpower_query,  /* component query */
            componentpower_component_register
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
        "componentpower"  // data being sensed
    }
};

/**
  * component open/close/init function
  */
static int orcm_sensor_componentpower_open(void)
{
    return ORCM_SUCCESS;
}

static int orcm_sensor_componentpower_query(mca_base_module_t **module, int *priority)
{
    /* if we can build, then we definitely want to be used
     * even if we aren't going to sample as we have to be
     * present in order to log any received results. Note that
     * we tested for existence and read-access for at least
     * one socket in the configure test, so we don't have to
     * check again here
     */
    *priority = 50;  /* ahead of heartbeat */
    *module = (mca_base_module_t *)&orcm_sensor_componentpower_module;
    return ORCM_SUCCESS;
}

/**
 *  Close all subsystems.
 */

static int orcm_sensor_componentpower_close(void)
{
    return ORCM_SUCCESS;
}

static int componentpower_component_register(void)
{
    mca_base_component_t *c = &mca_sensor_componentpower_component.super.base_version;

    mca_sensor_componentpower_component.test = false;
    (void) mca_base_component_var_register (c, "componentpower",
                                            "Generate and pass test vector",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_componentpower_component.test);
    return ORCM_SUCCESS;
}
