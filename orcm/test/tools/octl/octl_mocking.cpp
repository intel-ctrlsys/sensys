/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "octl_mocking.h"
#include "orcm/tools/octl/common.h"

#include <iostream>
using namespace std;

octl_tests_mocking octl_mocking;
bool is_mca_base_framework_open_expected_to_succeed = true;
bool is_orcm_parser_base_select_expected_to_succeed = true;
bool is_get_bmc_info_expected_to_succeed = true;
bool is_orcm_logical_group_parse_array_string_expected_to_succeed = true;

extern "C" { // Mocking must use correct "C" linkages
    #include <stdarg.h>

    int __wrap_orcm_cfgi_base_get_hostname_proc(char* hostname, orte_process_name_t* proc)
    {
        if(NULL == octl_mocking.orcm_cfgi_base_get_hostname_proc_callback) {
            return __real_orcm_cfgi_base_get_hostname_proc(hostname, proc);
        } else {
            return octl_mocking.orcm_cfgi_base_get_hostname_proc_callback(hostname, proc);
        }
    }

    int __wrap_opal_dss_pack(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type)
    {
        if(NULL == octl_mocking.opal_dss_pack_callback) {
            __real_opal_dss_pack(buffer, src, num_vals, type);
        } else {
            octl_mocking.opal_dss_pack_callback(buffer, src, num_vals, type);
        }
    }

    bool __wrap_get_bmc_info(char* hostname, ipmi_collector *ic){
        strncpy(ic->aggregator, "localhost", MAX_STR_LEN-1);
        ic->aggregator[MAX_STR_LEN-1] = '\0';
        return is_get_bmc_info_expected_to_succeed;
    }

    int __wrap_mca_base_framework_open(struct mca_base_framework_t *framework, mca_base_open_flag_t flags){
        if (!is_mca_base_framework_open_expected_to_succeed)
            return ORCM_ERROR;
        return __real_mca_base_framework_open(framework, flags);
    }

    int __wrap_orcm_parser_base_select(){
        if (!is_orcm_parser_base_select_expected_to_succeed)
            return ORCM_ERROR;
        return __real_orcm_parser_base_select();
    }

    int __wrap_orcm_logical_group_parse_array_string(char* regex, char*** o_array_string){
        if (!is_orcm_logical_group_parse_array_string_expected_to_succeed)
            return ORCM_ERROR;
        return __real_orcm_logical_group_parse_array_string(regex, o_array_string);
    }

} // extern "C"

octl_tests_mocking::octl_tests_mocking() : orcm_cfgi_base_get_hostname_proc_callback(NULL),
    opal_dss_pack_callback(NULL),
    fail_at(0),
    current_execution(0)
{
}
