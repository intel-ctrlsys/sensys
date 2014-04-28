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

END_C_DECLS

#endif
