/*
 * Copyright (c) 2016     Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SIGAR_MOCKED_FUNCTIONS_H
#define SIGAR_MOCKED_FUNCTIONS_H

#include <iostream>

#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"

#ifdef __cplusplus
extern "C"{
#endif
    extern bool __real_orcm_sensor_base_runtime_metrics_do_collect(void*, const char*);
#ifdef __cplusplus
}
#endif

const int collect_mem_size=4;
const int collect_swap_size=4;
const int collect_cpu_size=3;
const int collect_load_size=3;
const int collect_disk_size=11;
const int collect_network_size=10;
const int collect_gproc_size=7;
const int collect_proc_size=14;

extern bool collect_sample_flag;
extern bool collect_sys_success_flag;
extern bool collect_mem_success_flag[collect_mem_size];
extern bool collect_swap_success_flag[collect_swap_size];
extern bool collect_cpu_success_flag[collect_cpu_size];
extern bool collect_load_success_flag[collect_load_size];
extern bool collect_disk_success_flag[collect_disk_size];
extern bool collect_network_success_flag[collect_network_size];
extern bool collect_gproc_success_flag[collect_gproc_size];
extern bool collect_proc_success_flag[collect_proc_size];

bool eval_collect_mem(const char* sensor_spec);
bool eval_collect_swap(const char* sensor_spec);
bool eval_collect_cpu(const char* sensor_spec);
bool eval_collect_load(const char* sensor_spec);
bool eval_collect_disk(const char* sensor_spec);
bool eval_collect_network(const char* sensor_spec);
bool eval_collect_system(const char* sensor_spec);
bool eval_collect_gproc(const char* sensor_spec);
bool eval_collect_proc(const char* sensor_spec);
bool eval_sensor_spec(const char* sensor_spec);

#endif // SIGAR_MOCKED_FUNCTIONS_H
