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

#include "sensor_mcedata.h"

/*
 * Local functions
 */

static int orcm_sensor_mcedata_open(void);
static int orcm_sensor_mcedata_close(void);
static int orcm_sensor_mcedata_query(mca_base_module_t **module, int *priority);
static int mcedata_component_register(void);

orcm_sensor_mcedata_component_t mca_sensor_mcedata_component = {
    {
        {
            ORCM_SENSOR_BASE_VERSION_1_0_0,
            
            "mcedata", /* MCA component name */
            ORCM_MAJOR_VERSION,  /* MCA component major version */
            ORCM_MINOR_VERSION,  /* MCA component minor version */
            ORCM_RELEASE_VERSION,  /* MCA component release version */
            orcm_sensor_mcedata_open,  /* component open  */
            orcm_sensor_mcedata_close, /* component close */
            orcm_sensor_mcedata_query,  /* component query */
            mcedata_component_register
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
        "mcedata"  // data being sensed
    }
};

/**
  * component open/close/init function
  */
static int orcm_sensor_mcedata_open(void)
{
    return ORCM_SUCCESS;
}

static int orcm_sensor_mcedata_query(mca_base_module_t **module, int *priority)
{
    /* if we can build, then we definitely want to be used
     * even if we aren't going to sample as we have to be
     * present in order to log any received results. Note that
     * we tested for existence and read-access for at least
     * one socket in the configure test, so we don't have to
     * check again here
     */
    *priority = 50;  /* ahead of heartbeat */
    *module = (mca_base_module_t *)&orcm_sensor_mcedata_module;
    return ORCM_SUCCESS;
}

/**
 *  Close all subsystems.
 */

static int orcm_sensor_mcedata_close(void)
{
    return ORCM_SUCCESS;
}

static int mcedata_component_register(void)
{
    mca_base_component_t *c = &mca_sensor_mcedata_component.super.base_version;

     mca_sensor_mcedata_component.test = false;
    (void) mca_base_component_var_register (c, "test",
                                            "Generate and pass test vector",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_mcedata_component.test);
    mca_sensor_mcedata_component.collect_cache_errors = true;
    (void) mca_base_component_var_register (c, "collect_cache_errors",
                                            "Enable collection of cache errors when available",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_mcedata_component.collect_cache_errors);

    mca_sensor_mcedata_component.mce_logfile = NULL;
    (void) mca_base_component_var_register (c, "mce_logfile",
                                            "Name of log file where mcelog logs the errors",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_mcedata_component.mce_logfile);

    return ORCM_SUCCESS;
}
