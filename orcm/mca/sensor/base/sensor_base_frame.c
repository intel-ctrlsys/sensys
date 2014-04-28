/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/mca.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"
#include "opal/class/opal_pointer_array.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"

/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/sensor/base/static-components.h"

/*
 * Global variables
 */
orcm_sensor_base_API_module_t orcm_sensor = {
    orcm_sensor_base_start,
    orcm_sensor_base_stop
};
orcm_sensor_base_t orcm_sensor_base;

/*
 * Local variables
 */
static int orcm_sensor_base_sample_rate = 0;

static int orcm_sensor_base_register(mca_base_register_flag_t flags)
{
    int var_id;

    orcm_sensor_base_sample_rate = 0;
    var_id = mca_base_var_register("orcm", "sensor", "base", "sample_rate",
                                   "Sample rate in seconds",
                                   MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                   OPAL_INFO_LVL_9,
                                   MCA_BASE_VAR_SCOPE_READONLY,
                                   &orcm_sensor_base_sample_rate);
    mca_base_var_register_synonym(var_id, "orcm", "sensor", NULL, "sample_rate",
                                  MCA_BASE_VAR_SYN_FLAG_DEPRECATED);
  
    /* see if we want samples logged */
    orcm_sensor_base.log_samples = false;
    var_id = mca_base_var_register("orcm", "sensor", "base", "log_samples",
                                   "Log samples to database",
                                   MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                   OPAL_INFO_LVL_9,
                                   MCA_BASE_VAR_SCOPE_READONLY,
                                   &orcm_sensor_base.log_samples);
    mca_base_var_register_synonym(var_id, "orcm", "sensor", NULL, "log_samples",
                                  MCA_BASE_VAR_SYN_FLAG_DEPRECATED);

    return ORCM_SUCCESS;
}

static int orcm_sensor_base_close(void)
{
    orcm_sensor_active_module_t *i_module;
    int i;
    
    for (i=0; i < orcm_sensor_base.modules.size; i++) {
        if (NULL == (i_module = (orcm_sensor_active_module_t*)opal_pointer_array_get_item(&orcm_sensor_base.modules, i))) {
            continue;
        }
        if (NULL != i_module->module->finalize) {
            i_module->module->finalize();
        }
    }
    OBJ_DESTRUCT(&orcm_sensor_base.modules);
    
    /* Close all remaining available components */
    return mca_base_framework_components_close(&orcm_sensor_base_framework, NULL);
}

/**
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_sensor_base_open(mca_base_open_flag_t flags)
{
    /* initialize globals */
    orcm_sensor_base.active = false;

    /* construct the array of modules */
    OBJ_CONSTRUCT(&orcm_sensor_base.modules, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_sensor_base.modules, 3, INT_MAX, 1);
    
    /* get the sample rate */
    orcm_sensor_base.rate.tv_sec = orcm_sensor_base_sample_rate;
    orcm_sensor_base.rate.tv_usec = 0;

    /* Open up all available components */
    return mca_base_framework_components_open(&orcm_sensor_base_framework, flags);
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, sensor, "ORCM Monitoring Sensors",
                           orcm_sensor_base_register,
                           orcm_sensor_base_open, orcm_sensor_base_close,
                           mca_sensor_base_static_components, 0);

static void cons(orcm_sensor_active_module_t *t)
{
    t->sampling = true;
}
OBJ_CLASS_INSTANCE(orcm_sensor_active_module_t,
                   opal_object_t,
                   cons, NULL);
