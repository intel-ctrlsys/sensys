/*
 * Copyright (c) 2013-2015 Intel, Inc. All rights reserved.
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_var.h"

#include "sensor_dmidata.h"

/*
 * Local functions
 */

static int orcm_sensor_dmidata_open(void);
static int orcm_sensor_dmidata_close(void);
static int orcm_sensor_dmidata_query(mca_base_module_t **module, int *priority);
static int dmidata_component_register(void);

orcm_sensor_dmidata_component_t mca_sensor_dmidata_component = {
    {
        {
            ORCM_SENSOR_BASE_VERSION_1_0_0,
            /* Component name and version */
            .mca_component_name = "dmidata",
            MCA_BASE_MAKE_VERSION(component, ORCM_MAJOR_VERSION, ORCM_MINOR_VERSION,
                                  ORCM_RELEASE_VERSION),
        
            /* Component open and close functions */
            .mca_open_component = orcm_sensor_dmidata_open,
            .mca_close_component = orcm_sensor_dmidata_close,
            .mca_query_component = orcm_sensor_dmidata_query,
            .mca_register_component_params = dmidata_component_register
        },
        .base_data = {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
        "dmidata"  // data being sensed
    }
};

/**
  * component open/close/init function
  */
static int orcm_sensor_dmidata_open(void)
{
    return ORCM_SUCCESS;
}

static int orcm_sensor_dmidata_query(mca_base_module_t **module, int *priority)
{
    /* if we can build, then we definitely want to be used
     * even if we aren't going to sample as we have to be
     * present in order to log any received results. Note that
     * we tested for existence and read-access for at least
     * one socket in the configure test, so we don't have to
     * check again here
     */
    *priority = 50; 
    *module = (mca_base_module_t *)&orcm_sensor_dmidata_module;
    return ORCM_SUCCESS;
}

/**
 *  Close all subsystems.
 */

static int orcm_sensor_dmidata_close(void)
{
    return ORCM_SUCCESS;
}

static int dmidata_component_register(void)
{
    mca_base_component_t *c = &mca_sensor_dmidata_component.super.base_version;

    mca_sensor_dmidata_component.test = false;
    (void) mca_base_component_var_register (c, "test",
                                            "Generate and pass test vector",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_dmidata_component.test);
    mca_sensor_dmidata_component.ntw_dev = false;
    (void) mca_base_component_var_register (c, "ntw_dev",
                                            "Collect pci based network inventory",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_dmidata_component.ntw_dev);
    mca_sensor_dmidata_component.blk_dev = false;
    (void) mca_base_component_var_register (c, "blk_dev",
                                            "Collect pci based block devices inventory",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_dmidata_component.blk_dev);
    
    return ORCM_SUCCESS;
}
