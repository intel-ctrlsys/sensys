/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "opal/dss/dss.h"
#include "opal/mca/event/event.h"
#include "opal/runtime/opal_progress_threads.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/sensor/base/base.h"
#include "base/sensor_measurement.h"

double measurement_to_seconds(time_measure* measure)
{
    double start = (double) measure->ti.tv_sec + 1.0e-9*measure->ti.tv_nsec;
    double end = (double) measure->tf.tv_sec + 1.0e-9*measure->tf.tv_nsec;
    double delta = end - start;

    return delta;
}

void start_time_measurement(time_measure* measure)
{
    clock_gettime(CLOCK_MONOTONIC, &measure->ti);
}

void stop_time_measurement(time_measure* measure)
{
    clock_gettime(CLOCK_MONOTONIC, &measure->tf);
}

void print_time_measurement(time_measure* measure, char * mca_component_name)
{
    double duration = measurement_to_seconds(measure);

    opal_output_verbose(4, orcm_sensor_base_framework.framework_output, "%s measurement %s: %f s\n",
        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
        mca_component_name,
        duration);
}
