/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "heartbeat_tests_mocking.h"

using namespace std;

heartbeat_tests_mocking heartbeat_mocking;

extern "C" { // Mocking must use correct "C" linkages
    #include <stdarg.h>

    void __wrap_orte_errmgr_base_log(int err, char* file, int lineno)
    {
        if(NULL == heartbeat_mocking.orte_errmgr_base_log_callback) {
            __real_orte_errmgr_base_log(err, file, lineno);
        } else {
            heartbeat_mocking.orte_errmgr_base_log_callback(err, file, lineno);
        }
    }

    char* __wrap_orte_util_print_name_args(const orte_process_name_t* name)
    {
        if(NULL == heartbeat_mocking.orte_util_print_name_args_callback) {
            return __real_orte_util_print_name_args(name);
        } else {
            return heartbeat_mocking.orte_util_print_name_args_callback(name);
        }
    }

    opal_event_base_t* __wrap_opal_progress_thread_init(const char* name)
    {
        if(NULL == heartbeat_mocking.opal_progress_thread_init_callback) {
            return __real_opal_progress_thread_init(name);
        } else {
            return heartbeat_mocking.opal_progress_thread_init_callback(name);
        }
    }

    int __wrap_opal_progress_thread_finalize(const char* name)
    {
        if(NULL == heartbeat_mocking.opal_progress_thread_finalize_callback) {
            return __real_opal_progress_thread_finalize(name);
        } else {
            return heartbeat_mocking.opal_progress_thread_finalize_callback(name);
        }
    }

    orte_job_t* __wrap_orte_get_job_data_object(orte_jobid_t job)
    {
        if(NULL == heartbeat_mocking.orte_get_job_data_object_callback) {
            return __real_orte_get_job_data_object(job);
        } else {
            return heartbeat_mocking.orte_get_job_data_object_callback(job);
        }
    }

} // extern "C"

heartbeat_tests_mocking::heartbeat_tests_mocking() :
    orte_errmgr_base_log_callback(NULL),
    orte_util_print_name_args_callback(NULL),
    opal_progress_thread_init_callback(NULL),
    opal_progress_thread_finalize_callback(NULL)
{
}
