/*
 * Copyright (c) 2014-2016 Intel Corporation. All rights reserved.
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_var.h"

#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_nodepower.h"

extern orcm_sensor_base_t orcm_sensor_base;

/*
 * Local functions
 */

static int orcm_sensor_nodepower_open(void);
static int orcm_sensor_nodepower_close(void);
static int orcm_sensor_nodepower_query(mca_base_module_t **module, int *priority);
static int nodepower_component_register(void);

orcm_sensor_nodepower_component_t mca_sensor_nodepower_component = {
    {
        {
            ORCM_SENSOR_BASE_VERSION_1_0_0,
            /* Component name and version */
            .mca_component_name = "nodepower",
            MCA_BASE_MAKE_VERSION(component, ORCM_MAJOR_VERSION, ORCM_MINOR_VERSION,
                                  ORCM_RELEASE_VERSION),

            /* Component open and close functions */
            .mca_open_component = orcm_sensor_nodepower_open,
            .mca_close_component = orcm_sensor_nodepower_close,
            .mca_query_component = orcm_sensor_nodepower_query,
            .mca_register_component_params = nodepower_component_register
        },
        .base_data = {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
        "nodepower"  // data being sensed
    }
};

/**
  * component open/close/init function
  */
static int orcm_sensor_nodepower_open(void)
{
    return ORCM_SUCCESS;
}

static int orcm_sensor_nodepower_query(mca_base_module_t **module, int *priority)
{
    /* if we can build, then we definitely want to be used
     * even if we aren't going to sample as we have to be
     * present in order to log any received results. Note that
     * we tested for existence and read-access for at least
     * one socket in the configure test, so we don't have to
     * check again here
     */
    *priority = 50;  /* ahead of heartbeat */
    *module = (mca_base_module_t *)&orcm_sensor_nodepower_module;
    return ORCM_SUCCESS;
}

/**
 *  Close all subsystems.
 */

static int orcm_sensor_nodepower_close(void)
{
    return ORCM_SUCCESS;
}

static int nodepower_component_register(void)
{
    mca_base_component_t *c = &mca_sensor_nodepower_component.super.base_version;

    mca_sensor_nodepower_component.test = false;
#if OPAL_ENABLE_DEBUG
    (void) mca_base_component_var_register (c, "test",
                                            "Generate and pass test vector",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_nodepower_component.test);
#endif
    mca_sensor_nodepower_component.use_progress_thread = false;
    (void) mca_base_component_var_register(c, "use_progress_thread",
                                           "Use a dedicated progress thread for nodepower sensors [default: false]",
                                           MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_sensor_nodepower_component.use_progress_thread);

    mca_sensor_nodepower_component.sample_rate = 0;
    (void) mca_base_component_var_register(c, "sample_rate",
                                           "Sample rate in seconds",
                                           MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_sensor_nodepower_component.sample_rate);

    mca_sensor_nodepower_component.collect_metrics = orcm_sensor_base.collect_metrics;
    (void) mca_base_component_var_register(c, "collect_metrics",
                                           "Enable metric collection for the nodepower plugin",
                                           MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_sensor_nodepower_component.collect_metrics);

    return ORCM_SUCCESS;
}
