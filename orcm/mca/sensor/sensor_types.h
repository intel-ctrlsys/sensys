/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved. 
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/** @file:
 */

#ifndef ORCM_MCA_SENSOR_TYPES_H
#define ORCM_MCA_SENSOR_TYPES_H

#include "orcm_config.h"
#include "orcm/constants.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif  /* HAVE_SYS_TIME_H */

#include "opal/dss/dss_types.h"
#include "opal/mca/event/event.h"

/*
 * General SENSOR types - instanced in runtime/orcm_globals.c
 */

BEGIN_C_DECLS

enum {
    ORCM_SENSOR_SCALE_LINEAR,
    ORCM_SENSOR_SCALE_LOG,
    ORCM_SENSOR_SCALE_SIGMOID
};

/*
 * Structure for passing data from sensors
 */
typedef struct {
    opal_object_t super;
    char *sensor;
    struct timeval timestamp;
    opal_byte_object_t data;
} orcm_sensor_data_t;
ORCM_DECLSPEC OBJ_CLASS_DECLARATION(orcm_sensor_data_t);

/* define a callback function for calling when a sample
 * request is completed. Note that the buffer passed into
 * this function belongs to the CALLER and cannot be
 * released or destructed in the callback function */
typedef void (*orcm_sensor_sample_cb_fn_t)(opal_buffer_t *buf, void *cbdata);

/* define a tracking "caddy" for passing sampling
 * requests via the event library */
typedef struct {
    opal_object_t super;
    opal_event_t ev;
    struct timeval rate;
    bool log_data;
    char *sensors;           // comma-separated list of names of sensors to be sampled - NULL => sample all
    opal_buffer_t bucket;    // bucket in which to store the data
    orcm_sensor_sample_cb_fn_t cbfunc;  // fn to call upon completion of sampling
    void *cbdata;            // user-supplied data to be returned in cbfunc
} orcm_sensor_sampler_t;
ORCM_DECLSPEC OBJ_CLASS_DECLARATION(orcm_sensor_sampler_t);

END_C_DECLS

#endif
