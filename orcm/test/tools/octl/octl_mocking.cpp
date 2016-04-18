/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "octl_mocking.h"

#include <iostream>
using namespace std;

octl_tests_mocking octl_mocking;


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

    bool __wrap_get_bmc_info(char* hostname, ipmi_collector *ic){
        strncpy(ic->aggregator, "localhost", MAX_STR_LEN-1);
        ic->aggregator[MAX_STR_LEN-1] = '\0';
        return true;
    }
} // extern "C"

octl_tests_mocking::octl_tests_mocking() : orcm_cfgi_base_get_hostname_proc_callback(NULL)
{
}
