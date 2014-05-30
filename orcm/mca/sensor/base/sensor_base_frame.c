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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "opal/mca/mca.h"
#include "opal/util/argv.h"
#include "opal/util/fd.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/threads/threads.h"

#include "orte/mca/errmgr/errmgr.h"

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
    orcm_sensor_base_stop,
    orcm_sensor_base_manually_sample
};
orcm_sensor_base_t orcm_sensor_base;

static opal_event_t blocking_ev;
static int block_pipe[2];

static int orcm_sensor_base_register(mca_base_register_flag_t flags)
{
    int var_id;

    orcm_sensor_base.sample_rate = 0;
    var_id = mca_base_var_register("orcm", "sensor", "base", "sample_rate",
                                   "Sample rate in seconds",
                                   MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                   OPAL_INFO_LVL_9,
                                   MCA_BASE_VAR_SCOPE_READONLY,
                                   &orcm_sensor_base.sample_rate);
  
    /* see if we want samples logged */
    orcm_sensor_base.log_samples = false;
    var_id = mca_base_var_register("orcm", "sensor", "base", "log_samples",
                                   "Log samples to database",
                                   MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                   OPAL_INFO_LVL_9,
                                   MCA_BASE_VAR_SCOPE_READONLY,
                                   &orcm_sensor_base.log_samples);

    return ORCM_SUCCESS;
}

static int orcm_sensor_base_close(void)
{
    orcm_sensor_active_module_t *i_module;
    int i;
    
    if (orcm_sensor_base.progress_thread_running) {
        orcm_sensor_base.ev_active = false;
        /* break the event loop */
        opal_event_base_loopbreak(orcm_sensor_base.ev_base);
        /* wait for thread to exit */
        opal_thread_join(&orcm_sensor_base.progress_thread, NULL);
        OBJ_DESTRUCT(&orcm_sensor_base.progress_thread);
        orcm_sensor_base.progress_thread_running = false;
        opal_event_base_free(orcm_sensor_base.ev_base);
    }

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

static void wakeup(int fd, short args, void *cbdata)
{
    opal_output_verbose(10, orcm_sensor_base_framework.framework_output,
                        "%s sensor:wakeup invoked",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
}

/**
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_sensor_base_open(mca_base_open_flag_t flags)
{
    int rc;

    /* initialize globals */
    orcm_sensor_base.dbhandle = -1;
    orcm_sensor_base.dbhandle_requested = false;

    /* construct the array of modules */
    OBJ_CONSTRUCT(&orcm_sensor_base.modules, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_sensor_base.modules, 3, INT_MAX, 1);
    
    /* Open up all available components */
    if (OPAL_SUCCESS != (rc = mca_base_framework_components_open(&orcm_sensor_base_framework, flags))) {
        return rc;
    }

    /* create the event base */
    orcm_sensor_base.ev_base = opal_event_base_create();
    orcm_sensor_base.ev_active = false;
    orcm_sensor_base.progress_thread_running = false;
    /* add an event it can block on */
    if (0 > pipe(block_pipe)) {
        ORTE_ERROR_LOG(ORTE_ERR_IN_ERRNO);
        return ORTE_ERR_IN_ERRNO;
    }
    /* Make sure the pipe FDs are set to close-on-exec so that
       they don't leak into children */
    if (opal_fd_set_cloexec(block_pipe[0]) != OPAL_SUCCESS ||
        opal_fd_set_cloexec(block_pipe[1]) != OPAL_SUCCESS) {
        close(block_pipe[0]);
        close(block_pipe[1]);
        ORTE_ERROR_LOG(ORTE_ERR_IN_ERRNO);
        return ORTE_ERR_IN_ERRNO;
    }
    opal_event_set(orcm_sensor_base.ev_base, &blocking_ev, block_pipe[0], OPAL_EV_READ, wakeup, NULL);
    opal_event_add(&blocking_ev, 0);

    return OPAL_SUCCESS;
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

static void scon(orcm_sensor_sampler_t *p)
{
    p->sensors = NULL;
    p->rate.tv_sec = 0;
    p->rate.tv_usec = 0;
    OBJ_CONSTRUCT(&p->bucket, opal_buffer_t);
    p->cbfunc = NULL;
    p->cbdata = NULL;
}
static void sdes(orcm_sensor_sampler_t *p)
{
    if (NULL != p->sensors) {
        free(p->sensors);
    }
    OBJ_DESTRUCT(&p->bucket);
}
OBJ_CLASS_INSTANCE(orcm_sensor_sampler_t,
                   opal_object_t,
                   scon, sdes);
