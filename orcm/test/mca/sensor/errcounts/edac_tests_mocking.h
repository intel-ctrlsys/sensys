/*
 * Copyright (c) 2015-2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef EDAC_TESTS_MOCKING_H
#define EDAC_TESTS_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include <sys/stat.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include "orte/include/orte/types.h"
    #include "orcm/mca/analytics/analytics_types.h"

    extern void __real_orte_errmgr_base_log(int err, char* file, int lineno);
    extern void __real_opal_output_verbose(int level, int output_id, const char* format, ...);
    extern char* __real_orte_util_print_name_args(const orte_process_name_t* name);
    extern int __real_opal_dss_pack(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type);
    extern int __real_opal_dss_unpack(opal_buffer_t* buffer, void* dst, int32_t* num_vals, opal_data_type_t type);
    extern void __real_orcm_analytics_base_send_data(orcm_analytics_value_t *data);
    extern opal_event_base_t* __real_opal_progress_thread_init(const char* name);
    extern int __real_opal_progress_thread_finalize(const char* name);

#ifdef __cplusplus
}
#endif // __cplusplus

typedef void (*orte_errmgr_base_log_callback_fn_t)(int err, char* file, int lineno);
typedef void (*opal_output_verbose_callback_fn_t)(int level, int id, const char* line);
typedef char* (*orte_util_print_name_args_fn_t)(const orte_process_name_t* name);
typedef int (*opal_dss_pack_fn_t)(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type);
typedef int (*opal_dss_unpack_fn_t)(opal_buffer_t* buffer, void* dst, int32_t* num_vals, opal_data_type_t type);
typedef void (*orcm_analytics_base_send_data_fn_t)(orcm_analytics_value_t *data);
typedef opal_event_base_t* (*opal_progress_thread_init_fn_t)(const char* name);
typedef int (*opal_progress_thread_finalize_fn_t)(const char* name);

class edac_tests_mocking
{
    public:
        // Construction
        edac_tests_mocking();

        // Public Callbacks
        orte_errmgr_base_log_callback_fn_t orte_errmgr_base_log_callback;
        opal_output_verbose_callback_fn_t opal_output_verbose_callback;
        orte_util_print_name_args_fn_t orte_util_print_name_args_callback;
        opal_dss_pack_fn_t opal_dss_pack_callback;
        opal_dss_unpack_fn_t opal_dss_unpack_callback;
        orcm_analytics_base_send_data_fn_t orcm_analytics_base_send_data_callback;
        opal_progress_thread_init_fn_t opal_progress_thread_init_callback;
        opal_progress_thread_finalize_fn_t opal_progress_thread_finalize_callback;
};

extern edac_tests_mocking edac_mocking;

#endif // EDAC_TESTS_MOCKING_H
