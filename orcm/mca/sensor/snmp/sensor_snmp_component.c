/*
 * Copyright (c) 2015-2016 Intel, Inc. All rights reserved.
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"
#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_var.h"
#include "sensor_snmp.h"

/*
 * Local functions
 */

int orcm_sensor_snmp_open(void);
int orcm_sensor_snmp_close(void);
int orcm_sensor_snmp_query(mca_base_module_t **module, int *priority);
int snmp_component_register(void);

orcm_sensor_snmp_component_t mca_sensor_snmp_component = {
    {
        {
            ORCM_SENSOR_BASE_VERSION_1_0_0,
            /* Component name and version */
            .mca_component_name = "snmp",
            MCA_BASE_MAKE_VERSION(component, ORCM_MAJOR_VERSION, ORCM_MINOR_VERSION,
                                  ORCM_RELEASE_VERSION),

            /* Component open and close functions */
            .mca_open_component = orcm_sensor_snmp_open,
            .mca_close_component = orcm_sensor_snmp_close,
            .mca_query_component = orcm_sensor_snmp_query,
            .mca_register_component_params = snmp_component_register
        },
        .base_data = {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
        "snmp"  // data being sensed
    }
};

/**
  * component open/close/init function
  */
int orcm_sensor_snmp_open(void)
{
    return ORCM_SUCCESS;
}

int orcm_sensor_snmp_query(mca_base_module_t **module, int *priority)
{
    /* if we can build, then we definitely want to be used
     * even if we aren't going to sample as we have to be
     * present in order to log any received results. Note that
     * we tested for existence and read-access for at least
     * one socket in the configure test, so we don't have to
     * check again here
     */
    *priority = 50;  /* ahead of heartbeat */
    *module = (mca_base_module_t *)&orcm_sensor_snmp_module;
    return ORCM_SUCCESS;
}

/**
 *  Close all subsystems.
 */
int orcm_sensor_snmp_close(void)
{
    return ORCM_SUCCESS;
}

int snmp_component_register(void)
{
    mca_base_component_t *c = &mca_sensor_snmp_component.super.base_version;

    mca_sensor_snmp_component.use_progress_thread = false;
    (void) mca_base_component_var_register(c, "use_progress_thread",
                                           "Use a dedicated progress thread for snmp "
                                           "sensors [default: false]",
                                           MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_sensor_snmp_component.use_progress_thread);

    mca_sensor_snmp_component.sample_rate = 0;
    (void) mca_base_component_var_register(c, "sample_rate",
                                           "Sample rate in seconds",
                                           MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_sensor_snmp_component.sample_rate);

    mca_sensor_snmp_component.config_file = NULL;
    (void) mca_base_component_var_register (c, "config_file",
                                            "Full path to the SNMP configuration file",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_snmp_component.config_file);

    mca_sensor_snmp_component.collect_metrics = true;
    (void) mca_base_component_var_register(c, "collect_metrics",
                                           "Enable metric collection for the snmp plugin",
                                           MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_sensor_snmp_component.collect_metrics);

    return ORCM_SUCCESS;
}
