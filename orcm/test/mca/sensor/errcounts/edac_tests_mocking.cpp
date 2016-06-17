/*
 * Copyright (c) 2015-2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "edac_tests_mocking.h"

#include <iostream>
using namespace std;

edac_tests_mocking edac_mocking;


extern "C" { // Mocking must use correct "C" linkages
    #include <stdarg.h>

    int __wrap_stat(const char* pathname, struct stat* sb)
    {
        if(NULL == edac_mocking.stat_callback) {
            return __real_stat(pathname, sb);
        } else {
            return edac_mocking.stat_callback(pathname, sb);
        }
    }

    void __wrap_orte_errmgr_base_log(int err, char* file, int lineno)
    {
        if(NULL == edac_mocking.orte_errmgr_base_log_callback) {
            __real_orte_errmgr_base_log(err, file, lineno);
        } else {
            edac_mocking.orte_errmgr_base_log_callback(err, file, lineno);
        }
    }

    void __wrap_opal_output_verbose(int level, int output_id, const char* format, ...)
    {
        char buffer[8192];
        va_list args;
        va_start(args, format);
        vsprintf(buffer, format, args);
        va_end(args);
        if(NULL == edac_mocking.opal_output_verbose_callback) {
            __real_opal_output_verbose(level, output_id, (const char*)buffer);
        } else {
            edac_mocking.opal_output_verbose_callback(level, output_id, (const char*)buffer);
        }
    }

    char* __wrap_orte_util_print_name_args(const orte_process_name_t* name)
    {
        if(NULL == edac_mocking.orte_util_print_name_args_callback) {
            __real_orte_util_print_name_args(name);
        } else {
            edac_mocking.orte_util_print_name_args_callback(name);
        }
    }
    int __wrap_opal_dss_pack(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type)
    {
        if(NULL == edac_mocking.opal_dss_pack_callback) {
            __real_opal_dss_pack(buffer, src, num_vals, type);
        } else {
            edac_mocking.opal_dss_pack_callback(buffer, src, num_vals, type);
        }
    }

    int __wrap_opal_dss_unpack(opal_buffer_t* buffer, void* dst, int32_t* num_vals, opal_data_type_t type)
    {
        if(NULL == edac_mocking.opal_dss_unpack_callback) {
            __real_opal_dss_unpack(buffer, dst, num_vals, type);
        } else {
            edac_mocking.opal_dss_unpack_callback(buffer, dst, num_vals, type);
        }
    }
    void __wrap_orcm_analytics_base_send_data(orcm_analytics_value_t *data)
    {
        if(NULL == edac_mocking.orcm_analytics_base_send_data_callback) {
            __real_orcm_analytics_base_send_data(data);
        } else {
            edac_mocking.orcm_analytics_base_send_data_callback(data);
        }
    }
    opal_event_base_t* __wrap_opal_progress_thread_init(const char* name)
    {
        if(NULL == edac_mocking.opal_progress_thread_init_callback) {
            __real_opal_progress_thread_init(name);
        } else {
            edac_mocking.opal_progress_thread_init_callback(name);
        }
    }
    int __wrap_opal_progress_thread_finalize(const char* name)
    {
        if(NULL == edac_mocking.opal_progress_thread_finalize_callback) {
            __real_opal_progress_thread_finalize(name);
        } else {
            edac_mocking.opal_progress_thread_finalize_callback(name);
        }
    }
} // extern "C"

edac_tests_mocking::edac_tests_mocking() :
    stat_callback(NULL), orte_errmgr_base_log_callback(NULL),
    opal_output_verbose_callback(NULL), orte_util_print_name_args_callback(NULL),
    opal_dss_pack_callback(NULL), opal_dss_unpack_callback(NULL),
    orcm_analytics_base_send_data_callback(NULL), opal_progress_thread_init_callback(NULL),
    opal_progress_thread_finalize_callback(NULL)
{
}
