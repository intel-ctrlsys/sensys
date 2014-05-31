/*
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2012      Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_SENSOR_PRIVATE_H
#define MCA_SENSOR_PRIVATE_H

/*
 * includes
 */
#include "orcm_config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */

#include "opal/class/opal_pointer_array.h"
#include "opal/mca/event/event.h"
#include "opal/threads/threads.h"

#include "orte/runtime/orte_globals.h"

#include "orcm/mca/sensor/sensor.h"


/*
 * Global functions for MCA overall collective open and close
 */
BEGIN_C_DECLS

/* define a struct to hold framework-global values */
typedef struct {
    opal_event_base_t *ev_base;
    bool ev_active;
    opal_pointer_array_t modules;
    bool log_samples;
    int sample_rate;
    int dbhandle;
    bool dbhandle_requested;
} orcm_sensor_base_t;

typedef struct {
    opal_object_t super;
    orcm_sensor_base_component_t *component;
    orcm_sensor_base_module_t *module;
    int priority;
    bool sampling;
} orcm_sensor_active_module_t;
OBJ_CLASS_DECLARATION(orcm_sensor_active_module_t);


ORCM_DECLSPEC extern orcm_sensor_base_t orcm_sensor_base;
ORCM_DECLSPEC void orcm_sensor_base_start(orte_jobid_t job);
ORCM_DECLSPEC void orcm_sensor_base_stop(orte_jobid_t job);
ORCM_DECLSPEC void orcm_sensor_base_log(char *comp, opal_buffer_t *data);
/* manually sample one or more sensors */
ORCM_DECLSPEC void orcm_sensor_base_manually_sample(char *sensors,
                                                    orcm_sensor_sample_cb_fn_t cbfunc,
                                                    void *cbdata);


END_C_DECLS
#endif
