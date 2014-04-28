/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_var.h"

#include "sensor_file.h"

/*
 * Local functions
 */
static int orcm_sensor_file_register (void);
static int orcm_sensor_file_open(void);
static int orcm_sensor_file_close(void);
static int orcm_sensor_file_query(mca_base_module_t **module, int *priority);

orcm_sensor_file_component_t mca_sensor_file_component = {
    {
        {
            ORCM_SENSOR_BASE_VERSION_1_0_0,
            
            "file", /* MCA component name */
            ORCM_MAJOR_VERSION,  /* MCA component major version */
            ORCM_MINOR_VERSION,  /* MCA component minor version */
            ORCM_RELEASE_VERSION,  /* MCA component release version */
            orcm_sensor_file_open,  /* component open  */
            orcm_sensor_file_close, /* component close */
            orcm_sensor_file_query,  /* component query */
            orcm_sensor_file_register
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
        "filemods"  // data being sensed
    }
};


/**
  * component register/open/close/init function
  */
static int orcm_sensor_file_register (void)
{
    mca_base_component_t *c = &mca_sensor_file_component.super.base_version;

    /* lookup parameters */
    mca_sensor_file_component.file = NULL;
    (void) mca_base_component_var_register (c, "filename", "File to be monitored",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_ALL_EQ,
                                            &mca_sensor_file_component.file);

    mca_sensor_file_component.check_size = false;
    (void) mca_base_component_var_register (c, "check_size", "Check the file size",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_ALL_EQ,
                                            &mca_sensor_file_component.check_size);

    mca_sensor_file_component.check_access = false;
    (void) mca_base_component_var_register (c, "check_access", "Check access time",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_ALL_EQ,
                                            &mca_sensor_file_component.check_access);

    mca_sensor_file_component.check_mod = false;
    (void) mca_base_component_var_register (c, "check_mod", "Check modification time",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_ALL_EQ,
                                            &mca_sensor_file_component.check_mod);

    mca_sensor_file_component.limit = 3;
    (void) mca_base_component_var_register (c, "limit",
                                            "Number of times the sensor can detect no motion before declaring error (default=3)",
                                            MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_ALL_EQ,
                                            &mca_sensor_file_component.limit);
    return ORCM_SUCCESS;
}

static int orcm_sensor_file_open(void)
{
    return ORCM_SUCCESS;
}


static int orcm_sensor_file_query(mca_base_module_t **module, int *priority)
{
    *priority = 20;  /* higher than heartbeat */
    *module = (mca_base_module_t *)&orcm_sensor_file_module;
    return ORCM_SUCCESS;
}

/**
 *  Close all subsystems.
 */

static int orcm_sensor_file_close(void)
{
    return ORCM_SUCCESS;
}
