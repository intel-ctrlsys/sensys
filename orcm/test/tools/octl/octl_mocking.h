/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
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

    extern int __real_mca_base_framework_open(struct mca_base_framework_t *framework, mca_base_open_flag_t flags);

    extern int __real_orcm_parser_base_select(void);

    extern int __real_orcm_logical_group_parse_array_string(char* regex, char*** o_array_string);

#ifdef __cplusplus
}
#endif // __cplusplus

extern bool is_mca_base_framework_open_expected_to_succeed;
extern bool is_orcm_parser_base_select_expected_to_succeed;
extern bool is_get_bmc_info_expected_to_succeed;
extern bool is_orcm_logical_group_parse_array_string_expected_to_succeed;

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
