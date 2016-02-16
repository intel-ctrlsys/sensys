/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <time.h>

#ifndef SENSOR_MEASUREMENT_H

typedef struct {
    struct timespec ti, tf;
} time_measure;

double measurement_to_seconds(time_measure*);
void start_time_measurement(time_measure*);
void stop_time_measurement(time_measure*);
void print_time_measurement(time_measure*, char*);

#endif /* SENSOR_MEASUREMENT_H*/
