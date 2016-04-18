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

#ifdef __cplusplus
}
#endif // __cplusplus

typedef int (*orcm_cfgi_base_get_hostname_proc_callback_fn_t)(char* hostname, orte_process_name_t* proc);

class octl_tests_mocking
{
    public:
        // Construction
        octl_tests_mocking();

        // Public Callbacks
        orcm_cfgi_base_get_hostname_proc_callback_fn_t orcm_cfgi_base_get_hostname_proc_callback;
};

extern octl_tests_mocking octl_mocking;

#endif // OCTL_MOCKING_H
