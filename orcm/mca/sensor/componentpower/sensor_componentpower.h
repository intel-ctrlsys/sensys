/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/**
 * @file
 *
 * componentpower resource manager sensor 
 */
#ifndef ORCM_SENSOR_COMPONENTPOWER_H
#define ORCM_SENSOR_COMPONENTPOWER_H

#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"

BEGIN_C_DECLS

typedef struct {
    orcm_sensor_base_component_t super;
    bool test;
} orcm_sensor_componentpower_component_t;

ORCM_MODULE_DECLSPEC extern orcm_sensor_componentpower_component_t mca_sensor_componentpower_component;
extern orcm_sensor_base_module_t orcm_sensor_componentpower_module;

typedef struct _stats_value{
    struct timeval tv;        //msec resolution
    double avg_value;        //averag
    double min;            //
    double max;            //
    double cur;    //current instaneous value
}orcm_sensor_componentpower_stats_value;

typedef struct _capabilities{
    unsigned char node_accuracy;    //in steps of 1 %
    double node_resolution;      // 
    unsigned int node_min_avg_time; //in milliseconds e.g. min = 100 msec
    unsigned int node_max_avg_time; //in milliseconds e.g. max = 10 min
}componentpower_capabilities;

typedef struct _freq_data {
    orcm_sensor_componentpower_stats_value freq; // frequency, in MHz, observed over a time window
}freq_data;

#define RAPL_UNIT 0x606
#define RAPL_CPU_ENERGY 0x611
#define RAPL_DDR_ENERGY 0x619
#define RAPL_SHIFT1 (0xf0000)
#define MAX_CPUS 4096
#define MAX_SOCKETS 256
#define STR_LEN 128

typedef struct{
    int n_cpus;
    int n_sockets;
    int cpu_idx[MAX_SOCKETS];
    int fd_cpu[MAX_SOCKETS];
    unsigned long long rapl_esu;
    int dev_msr_support;
    int cpu_rapl_support;
    int ddr_rapl_support;
    unsigned long long cpu_rapl[MAX_SOCKETS];
    unsigned long long cpu_rapl_prev[MAX_SOCKETS];
    unsigned long long ddr_rapl[MAX_SOCKETS];
    unsigned long long ddr_rapl_prev[MAX_SOCKETS];
    double cpu_power[MAX_SOCKETS];
    double ddr_power[MAX_SOCKETS];
    unsigned long long rapl_calls;
}__rapl;
typedef struct{
    struct timeval tv_curr;
    struct timeval tv_prev;
    unsigned long long interval;
}__time_val;


END_C_DECLS

#endif

