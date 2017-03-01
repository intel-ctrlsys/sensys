/*
 * Copyright (c) 2013-2016 Intel Corporation. All rights reserved.
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_var.h"

#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_sigar.h"

extern orcm_sensor_base_t orcm_sensor_base;

/*
 * Local functions
 */

static int orcm_sensor_sigar_open(void);
static int orcm_sensor_sigar_close(void);
static int orcm_sensor_sigar_query(mca_base_module_t **module, int *priority);
static int sigar_component_register(void);

orcm_sensor_sigar_component_t mca_sensor_sigar_component = {
    {
        {
            ORCM_SENSOR_BASE_VERSION_1_0_0,
            /* Component name and version */
            .mca_component_name = "sigar",
            MCA_BASE_MAKE_VERSION(component, ORCM_MAJOR_VERSION, ORCM_MINOR_VERSION,
                                  ORCM_RELEASE_VERSION),

            /* Component open and close functions */
            .mca_open_component = orcm_sensor_sigar_open,
            .mca_close_component = orcm_sensor_sigar_close,
            .mca_query_component = orcm_sensor_sigar_query,
            .mca_register_component_params = sigar_component_register
        },
        .base_data = {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
        "procresource,noderesource"
    }
};

/**
  * component open/close/init function
  */
static int orcm_sensor_sigar_open(void)
{
    return ORCM_SUCCESS;
}

static int orcm_sensor_sigar_query(mca_base_module_t **module, int *priority)
{
    /* if we can build, then we definitely want to be used
     * even if we aren't going to sample as we have to be
     * present in order to log any received results
     */
    *priority = 150;  /* ahead of heartbeat and resusage */
    *module = (mca_base_module_t *)&orcm_sensor_sigar_module;
    return ORCM_SUCCESS;
}

/**
 *  Close all subsystems.
 */

static int orcm_sensor_sigar_close(void)
{
    return ORCM_SUCCESS;
}

static int sigar_component_register(void)
{
    mca_base_component_t *c = &mca_sensor_sigar_component.super.base_version;

    mca_sensor_sigar_component.test = false;
#if OPAL_ENABLE_DEBUG
    (void) mca_base_component_var_register (c, "test",
                                            "Generate and pass test vector",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.test);
#endif
    mca_sensor_sigar_component.mem = true;
    (void) mca_base_component_var_register (c, "mem",
                                            "Enable collecting memory usage",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.mem);
    mca_sensor_sigar_component.swap = true;
    (void) mca_base_component_var_register (c, "swap",
                                            "Enabale collecting swap usage",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.swap);
    mca_sensor_sigar_component.cpu = true;
    (void) mca_base_component_var_register (c, "cpu",
                                            "Enable collecting cpu usage",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.cpu);
    mca_sensor_sigar_component.load = true;
    (void) mca_base_component_var_register (c, "load",
                                            "Enable collecting cpu load",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.load);
    mca_sensor_sigar_component.disk = true;
    (void) mca_base_component_var_register (c, "disk",
                                            "Enable collecting disk usage",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.disk);
    mca_sensor_sigar_component.network = true;
    (void) mca_base_component_var_register (c, "network",
                                            "Enable collecting network usage",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.network);
    mca_sensor_sigar_component.sys = true;
    (void) mca_base_component_var_register (c, "sys",
                                            "Enable collecting system information like uptime",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.proc);
    mca_sensor_sigar_component.proc = true;
    (void) mca_base_component_var_register (c, "proc",
                                            "Enable collecting process information of daemon and child processes",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.proc);
    mca_sensor_sigar_component.use_progress_thread = false;
    (void) mca_base_component_var_register(c, "use_progress_thread",
                                           "Use a dedicated progress thread for sigar sensors [default: false]",
                                           MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_sensor_sigar_component.use_progress_thread);

    mca_sensor_sigar_component.sample_rate = 0;
    (void) mca_base_component_var_register(c, "sample_rate",
                                           "Sample rate in seconds",
                                           MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_sensor_sigar_component.sample_rate);

    mca_sensor_sigar_component.collect_metrics = orcm_sensor_base.collect_metrics;
    (void) mca_base_component_var_register(c, "collect_metrics",
                                           "Enable metric collection for the sigar plugin",
                                           MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_sensor_sigar_component.collect_metrics);

    return ORCM_SUCCESS;
}
