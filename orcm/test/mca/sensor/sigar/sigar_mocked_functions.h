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
#include "orcm/util/utils.h"

#ifdef __cplusplus
extern "C"{
#endif
    extern bool __real_orcm_sensor_base_runtime_metrics_do_collect(void*, const char*);
    extern orcm_value_t* __real_orcm_util_load_orcm_value(char*, void*, opal_data_type_t, char*);
#ifdef __cplusplus
}
#endif

const int sigar_mem_size = 4;
const int sigar_swap_size = 4;
const int sigar_cpu_size = 3;
const int sigar_load_size = 3;
const int sigar_disk_size = 11;
const int sigar_network_size = 10;
const int sigar_gproc_size = 7;
const int sigar_proc_size = 14;

extern bool collect_sample_flag;
extern bool collect_sys_success_flag;
extern bool collect_mem_success_flag[sigar_mem_size];
extern bool collect_swap_success_flag[sigar_swap_size];
extern bool collect_cpu_success_flag[sigar_cpu_size];
extern bool collect_load_success_flag[sigar_load_size];
extern bool collect_disk_success_flag[sigar_disk_size];
extern bool collect_network_success_flag[sigar_network_size];
extern bool collect_gproc_success_flag[sigar_gproc_size];
extern bool collect_proc_success_flag[sigar_proc_size];

extern bool uvalue_sample_flag;
extern bool uvalue_sys_success_flag;
extern bool uvalue_mem_success_flag[sigar_mem_size];
extern bool uvalue_swap_success_flag[sigar_swap_size];
extern bool uvalue_cpu_success_flag[sigar_cpu_size];
extern bool uvalue_load_success_flag[sigar_load_size];
extern bool uvalue_disk_success_flag[sigar_disk_size];
extern bool uvalue_network_success_flag[sigar_network_size];
extern bool uvalue_gproc_success_flag[sigar_gproc_size];
extern bool uvalue_proc_success_flag[sigar_proc_size];

bool eval_collect_mem(const char* sensor_spec);
bool eval_collect_swap(const char* sensor_spec);
bool eval_collect_cpu(const char* sensor_spec);
bool eval_collect_load(const char* sensor_spec);
bool eval_collect_disk(const char* sensor_spec);
bool eval_collect_network(const char* sensor_spec);
bool eval_collect_system(const char* sensor_spec);
bool eval_collect_gproc(const char* sensor_spec);
bool eval_collect_proc(const char* sensor_spec);

bool eval_uvalue_mem(char* key);
bool eval_uvalue_swap(char* key);
bool eval_uvalue_cpu(char* key);
bool eval_uvalue_load(char* key);
bool eval_uvalue_disk(char* key);
bool eval_uvalue_network(char* key);
bool eval_uvalue_system(char* key);
bool eval_uvalue_gproc(char* key);
bool eval_uvalue_proc(char* key);

bool eval_collect_spec(const char* sensor_spec);
bool eval_uvalue_key(char* key);
#endif // SIGAR_MOCKED_FUNCTIONS_H
