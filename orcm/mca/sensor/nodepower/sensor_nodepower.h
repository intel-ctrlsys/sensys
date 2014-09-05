/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/**
 * @file
 *
 * nodepower resource manager sensor 
 */
#ifndef ORCM_SENSOR_NODEPOWER_H
#define ORCM_SENSOR_NODEPOWER_H

#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"

BEGIN_C_DECLS

typedef struct {
    orcm_sensor_base_component_t super;
    bool test;
} orcm_sensor_nodepower_component_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_nodepower_component_t mca_sensor_nodepower_component;
extern orcm_sensor_base_module_t orcm_sensor_nodepower_module;

typedef struct _stats_value{
    struct timeval tv;        //msec resolution
    double avg_value;        //averag
    double min;            //
    double max;            //
    double cur;    //current instaneous value
}orcm_sensor_nodepower_stats_value;

typedef struct _capabilities{
    unsigned char node_accuracy;    //in steps of 1 %
    double node_resolution;      // 
    unsigned int node_min_avg_time; //in milliseconds e.g. min = 100 msec
    unsigned int node_max_avg_time; //in milliseconds e.g. max = 10 min
}nodepower_capabilities;

typedef struct _node_power_data{
    orcm_sensor_nodepower_stats_value node_power;        //node power, in Watts
    unsigned char raw_string[256];
    int str_len;
    unsigned long ret_val[2];
}node_power_data;

typedef struct {
    unsigned long long readein_accu_curr;
    unsigned long long readein_accu_prev;
    unsigned long long readein_cnt_curr;
    unsigned long long readein_cnt_prev;
    unsigned long long ipmi_calls;
} __readein;
typedef struct {
    struct timeval tv_curr;
    struct timeval tv_prev;
    unsigned long long interval;
} __time_val;

END_C_DECLS

#endif

