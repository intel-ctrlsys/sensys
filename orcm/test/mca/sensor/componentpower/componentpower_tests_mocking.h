/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef COMPONENTPOWER_TESTS_MOCKING_H
#define COMPONENTPOWER_TESTS_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include <stdlib.h>
    #include <stdio.h>
    #include "orte/include/orte/types.h"
    #include "orcm/mca/analytics/analytics_types.h"

    extern void __real_orte_errmgr_base_log(int err, char* file, int lineno);
    extern int __real_opal_dss_pack(opal_buffer_t *buffer, const void *src,
                                    int32_t num_vals, opal_data_type_t type);
    extern opal_event_base_t* __real_opal_progress_thread_init(const char* name);
    extern int __real_read(int handle, void *buffer, int nbyte);
    extern int __real_open(char *filename, int access, int permission);
    extern FILE* __real_fopen(const char * filename, const char * mode);

#ifdef __cplusplus
}
#endif // __cplusplus

typedef void (*orte_errmgr_base_log_callback_fn_t)(int err, char* file, int lineno);
typedef int (*opal_dss_pack_callback_fn_t)(opal_buffer_t *buffer, const void *src,
                                           int32_t num_vals, opal_data_type_t type);
typedef opal_event_base_t* (*opal_progress_thread_init_fn_t)(const char* name);
typedef int (*read_callback_fn_t) (int handle, void *buffer, int nbyte);
typedef int (*open_callback_fn_t)(char *filename, int access, int permission);
typedef FILE* (*fopen_callback_fn_t)(const char * filename, const char * mode);

class componentpower_tests_mocking
{
    public:
        componentpower_tests_mocking();

        // Public Callbacks
        orte_errmgr_base_log_callback_fn_t orte_errmgr_base_log_callback;
        opal_dss_pack_callback_fn_t        opal_dss_pack_callback;
        opal_progress_thread_init_fn_t     opal_progress_thread_init_callback;
        read_callback_fn_t                 read_callback;
        read_callback_fn_t                 read_return_error_callback;
        open_callback_fn_t                 open_callback;
        fopen_callback_fn_t                fopen_callback;

        //Public variables
        static int ntimes;
        static bool keep_mocking;
};

extern componentpower_tests_mocking componentpower_mocking;

#endif // COMPONENTPOWER_TESTS_MOCKING_H
