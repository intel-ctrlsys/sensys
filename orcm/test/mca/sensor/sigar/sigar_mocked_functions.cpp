/*
 * Copyright (c) 2016     Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <string.h>
#include "sigar_mocked_functions.h"

bool collect_sample_flag = false;
bool collect_sys_success_flag = false;
bool collect_mem_success_flag[collect_mem_size] = {false};
bool collect_swap_success_flag[collect_swap_size] = {false};
bool collect_cpu_success_flag[collect_cpu_size] = {false};
bool collect_load_success_flag[collect_load_size] = {false};
bool collect_disk_success_flag[collect_disk_size] = {false};
bool collect_network_success_flag[collect_network_size] = {false};
bool collect_gproc_success_flag[collect_gproc_size] = {false};
bool collect_proc_success_flag[collect_proc_size] = {false};

std::string mem_str[collect_mem_size] = {"mem_total", "mem_used", \
                                         "mem_actual_used", "mem_actual_free"};
std::string swap_str[collect_swap_size] = {"swap_total", "swap_used", \
                                           "swap_page_in", "swap_page_out"};
std::string cpu_str[collect_cpu_size] = {"cpu_user", "cpu_sys", "cpu_idle"};
std::string load_str[collect_load_size] = {"load0", "load1", "load2"};
std::string disk_str[collect_disk_size] = {"disk_ro_rate", "disk_wo_rate", \
                                           "disk_rb_rate", "disk_wb_rate", \
                                           "disk_ro_total", "disk_wo_total", \
                                           "disk_rb_total", "disk_wb_total", \
                                           "disk_rt_total", "disk_wt_total", \
                                           "disk_iot_total"};
std::string network_str[collect_network_size] = {"net_rp_rate", "net_wp_rate", \
                                                 "net_rb_rate", "net_wb_rate", \
                                                 "net_wb_total", "net_rb_total", \
                                                 "net_rb_total", "net_rp_total", \
                                                 "net_tx_errors", "net_rx_errors"};
std::string gproc_str[collect_gproc_size] = {"total_processes", "sleeping_processes", \
                                             "running_processes", "zombie_processes", \
                                             "stopped_processes", "idle_processes", \
                                             "total_threads"};
std::string proc_str[collect_proc_size] = {"pid", "state_1", "state_2", "percent_cpu", \
                                           "priority", "num_threads", "vsize", "rss", \
                                           "processor", "shared_memory", "minor_faults", \
                                           "major_faults", "page_faults", "percent"};

bool eval_collect_mem(const char* sensor_spec){
    for(int i = 0; i < collect_mem_size; i++){
        if (!strcmp(sensor_spec, mem_str[i].c_str()) && collect_mem_success_flag[i])
            return true;
    }
    return false;
}

bool eval_collect_swap(const char* sensor_spec){
    for(int i = 0; i < collect_swap_size; i++){
        if (!strcmp(sensor_spec, swap_str[i].c_str()) && collect_swap_success_flag[i])
            return true;
    }
    return false;
}

bool eval_collect_cpu(const char* sensor_spec){
    for(int i = 0; i < collect_cpu_size; i++){
        if (!strcmp(sensor_spec, cpu_str[i].c_str()) && collect_cpu_success_flag[i])
            return true;
    }
    return false;
}

bool eval_collect_load(const char* sensor_spec){
    for(int i = 0; i < collect_load_size; i++){
        if (!strcmp(sensor_spec, load_str[i].c_str()) && collect_load_success_flag[i])
            return true;
    }
    return false;
}

bool eval_collect_disk(const char* sensor_spec){
    for(int i = 0; i < collect_disk_size; i++){
        if (!strcmp(sensor_spec, disk_str[i].c_str()) && collect_disk_success_flag[i])
            return true;
    }
    return false;
}

bool eval_collect_network(const char* sensor_spec){
    for(int i = 0; i < collect_network_size; i++){
        if (!strcmp(sensor_spec, network_str[i].c_str()) && collect_network_success_flag[i])
            return true;
    }
    return false;
}

bool eval_collect_system(const char* sensor_spec){
    if (!strcmp(sensor_spec, "uptime") && collect_sys_success_flag)
        return true;
    return false;
}

bool eval_collect_gproc(const char* sensor_spec){
    for(int i = 0; i < collect_gproc_size; i++){
        if (!strcmp(sensor_spec, gproc_str[i].c_str()) && collect_gproc_success_flag[i])
            return true;
    }
    return false;
}

bool eval_collect_proc(const char* sensor_spec){
    for(int i = 0; i < collect_proc_size; i++){
        if (!strcmp(sensor_spec, proc_str[i].c_str()) && collect_proc_success_flag[i])
            return true;
    }
    return false;
}
bool eval_sensor_spec(const char* sensor_spec){
    if (eval_collect_mem(sensor_spec) || eval_collect_swap(sensor_spec) || \
        eval_collect_cpu(sensor_spec) || eval_collect_load(sensor_spec) || \
        eval_collect_disk(sensor_spec) || eval_collect_network(sensor_spec) || \
        eval_collect_system(sensor_spec) || eval_collect_gproc(sensor_spec) || \
        eval_collect_proc(sensor_spec))
        return true;
    else
        return false;
}

extern "C" {
    bool __wrap_orcm_sensor_base_runtime_metrics_do_collect(void* runtime_metrics, const char* sensor_spec){
        if(NULL != sensor_spec && eval_sensor_spec(sensor_spec)){
            collect_sample_flag = true;
            return false;
        }
        else
            return __real_orcm_sensor_base_runtime_metrics_do_collect(runtime_metrics, sensor_spec);
    }
}
