/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef OCTL_MOCKING_H
#define OCTL_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include <sys/stat.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include "orte/include/orte/types.h"
    #include "orcm/mca/sensor/ipmi/ipmi_parser_interface.h"

    extern int __real_orcm_cfgi_base_get_hostname_proc(char* hostname, orte_process_name_t* proc);

    extern bool __real_get_bmc_info(char* hostname, ipmi_collector *ic);

    extern int __real_opal_dss_pack(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type);

#ifdef __cplusplus
}
#endif // __cplusplus

typedef int (*orcm_cfgi_base_get_hostname_proc_callback_fn_t)(char* hostname, orte_process_name_t* proc);
typedef int (*opal_dss_pack_fn_t)(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type);

class octl_tests_mocking
{
    public:
        // Construction
        octl_tests_mocking();

        // Public Callbacks
        orcm_cfgi_base_get_hostname_proc_callback_fn_t orcm_cfgi_base_get_hostname_proc_callback;
        opal_dss_pack_fn_t opal_dss_pack_callback;

        //Public variables
        int fail_at;
        int current_execution;
};

extern octl_tests_mocking octl_mocking;

#endif // OCTL_MOCKING_H
