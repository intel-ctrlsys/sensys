/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef HEARTBEAT_TESTS_MOCKING_H
#define HEARTBEAT_TESTS_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include <stdlib.h>
    #include <stdio.h>
    #include "orte/include/orte/types.h"
    #include "orcm/util/utils.h"
    #include "orte/runtime/orte_globals.h"

    extern void __real_orte_errmgr_base_log(int err, char* file, int lineno);
    extern char* __real_orte_util_print_name_args(const orte_process_name_t* name);
    extern opal_event_base_t* __real_opal_progress_thread_init(const char* name);
    extern int __real_opal_progress_thread_finalize(const char* name);
    extern orte_job_t* __real_orte_get_job_data_object(orte_jobid_t job);

#ifdef __cplusplus
}
#endif // __cplusplus

typedef void (*orte_errmgr_base_log_callback_fn_t)(int err, char* file, int lineno);
typedef char* (*orte_util_print_name_args_fn_t)(const orte_process_name_t* name);
typedef opal_event_base_t* (*opal_progress_thread_init_fn_t)(const char* name);
typedef int (*opal_progress_thread_finalize_fn_t)(const char* name);
typedef orte_job_t* (*orte_get_job_data_object_fn_t)(orte_jobid_t job);

class heartbeat_tests_mocking
{
    public:
        // Construction
        heartbeat_tests_mocking();

        // Public Callbacks
        orte_errmgr_base_log_callback_fn_t orte_errmgr_base_log_callback;
        orte_util_print_name_args_fn_t orte_util_print_name_args_callback;
        opal_progress_thread_init_fn_t opal_progress_thread_init_callback;
        opal_progress_thread_finalize_fn_t opal_progress_thread_finalize_callback;
        orte_get_job_data_object_fn_t orte_get_job_data_object_callback;
};

extern heartbeat_tests_mocking heartbeat_mocking;

#endif // HEARTBEAT_TESTS_MOCKING_H
