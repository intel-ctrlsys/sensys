/*
 * Copyright (c) 2010-2011 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012      Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2014-2016 Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "opal/mca/base/base.h"
#include "opal/util/output.h"
#include "opal/class/opal_pointer_array.h"

#include "sensor_evinj.h"

/*
 * Local functions
 */
static int orcm_sensor_evinj_register (void);
static int orcm_sensor_evinj_open(void);
static int orcm_sensor_evinj_close(void);
static int orcm_sensor_evinj_query(mca_base_module_t **module, int *priority);

orcm_sensor_evinj_component_t mca_sensor_evinj_component = {
    {
        {
            ORCM_SENSOR_BASE_VERSION_1_0_0,
            /* Component name and version */
            .mca_component_name = "evinj",
            MCA_BASE_MAKE_VERSION(component, ORCM_MAJOR_VERSION, ORCM_MINOR_VERSION,
                                  ORCM_RELEASE_VERSION),

            /* Component open and close functions */
            .mca_open_component = orcm_sensor_evinj_open,
            .mca_close_component = orcm_sensor_evinj_close,
            .mca_query_component = orcm_sensor_evinj_query,
            .mca_register_component_params = orcm_sensor_evinj_register
        },
        .base_data = {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
        "nothing"    // data being sensed
    }
};

static char *prob = NULL;
opal_rng_buff_t orcm_sensor_inj_rng_buff;

/**
  * component register/open/close/init function
  */
static int orcm_sensor_evinj_register (void)
{
    mca_base_component_t *c = &mca_sensor_evinj_component.super.base_version;

    prob = NULL;
    (void) mca_base_component_var_register (c, "prob", "Probability of injecting a RAS event during this sample period",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &prob);

    mca_sensor_evinj_component.vector_file = NULL;
    (void) mca_base_component_var_register (c, "vectors", "File of RAS event test vectors",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_sensor_evinj_component.vector_file);

    mca_sensor_evinj_component.collect_metrics = true;
    (void) mca_base_component_var_register(c, "collect_metrics",
                                           "Enable metric collection for the evinj plugin",
                                           MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_sensor_evinj_component.collect_metrics);

    return ORCM_SUCCESS;
}

static int orcm_sensor_evinj_open(void)
{
    /* lookup parameters */
    if (NULL != prob) {
        mca_sensor_evinj_component.prob = strtof(prob, NULL);
        if ('%' == prob[strlen(prob)-1] ||
            1.0 < mca_sensor_evinj_component.prob) {
            /* given in percent */
            mca_sensor_evinj_component.prob /= 100.0;
        }
    } else {
        mca_sensor_evinj_component.prob = 0.0;
    }
    return ORCM_SUCCESS;
}


static int orcm_sensor_evinj_query(mca_base_module_t **module, int *priority)
{
    if (0.0 < mca_sensor_evinj_component.prob) {
        *priority = 1;  /* at the bottom */
        *module = (mca_base_module_t *)&orcm_sensor_evinj_module;
        /* seed the random number generator */
        opal_srand(&mca_sensor_evinj_component.rng_buff, (uint32_t)getpid());
        return ORCM_SUCCESS;
    }
    *priority = 0;
    *module = NULL;
    return ORTE_ERROR;

}

/**
 *  Close all subsystems.
 */

static int orcm_sensor_evinj_close(void)
{
    return ORCM_SUCCESS;
}
