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
#include "orte/mca/notifier/notifier.h"
#include "orcm/mca/sensor/sensor.h"


/*
 * Global functions for MCA overall collective open and close
 */
BEGIN_C_DECLS

/****    SENSOR EVENT POLICY TYPE    ****/
/* An sensor event policy consists of:
 * sensor_name: the name of the sensor you want to monitor
 * max_count: max number of samples cross the threshold
 * time_window: time window to test the policy, in seconds
 * threshold: threshold of the sensor reading
 * hi_thres: high or low threshold
 * severity: severity level assigned to this event policy
 * action: notification mechanism of this event
 *
 * Example: coretemp:100:hi:2:60:alert:syslog
 *    If we see two times of coretemp reading higher than 100 in 60 seconds'
 *    time window, an alert event will be generated and logged into syslog
 */
typedef struct {
    opal_list_item_t super;
    char  *sensor_name;
    int   max_count;
    int   time_window;
    float threshold;
    bool  hi_thres;
    orte_notifier_severity_t severity;
    char  *action;
} orcm_sensor_policy_t;
OBJ_CLASS_DECLARATION(orcm_sensor_policy_t);

/* define a struct to hold framework-global values */
typedef struct {
    opal_event_base_t *ev_base;
    bool ev_active;
    opal_pointer_array_t modules;
    bool log_samples;
    int sample_rate;    /* Holds the rate at which the sensors need to be sampeled in seconds */
    opal_list_t policy; /* Holds user configured RAS event policy */
    int dbhandle;       /* Stores the unique database handle assigned for sensor framework after calling db_open */
    bool dbhandle_acquired;
    bool collect_metrics;       /* Holds the user configured variable indicating whether sensor metric sampling is enabled or not */
    bool collect_inventory;     /* Holds the user configured variable indicating whether inventory collection is enabled or not */
    bool set_dynamic_inventory; /* Holds the user configured variable indicating whether dynamic inventory collection is enabled or not */
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
