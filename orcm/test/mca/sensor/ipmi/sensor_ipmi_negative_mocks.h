/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SENSOR_IPMI_PARSER_INTERFACE_MOCKED_H
#define SENSOR_IPMI_PARSER_INTERFACE_MOCKED_H

extern bool is_load_ipmi_config_file_expected_to_succeed;
extern bool is_get_bmcs_for_aggregator_expected_to_succeed;
extern bool is_opal_progress_thread_init_expected_to_succeed;
extern bool is_get_sdr_cache_expected_to_succeed;
extern int find_sdr_next_success_count;

#ifdef __cplusplus
extern "C"{
#endif

    #include "orcm/mca/sensor/ipmi/ipmi_parser_interface.h"
    #include "opal/runtime/opal_progress_threads.h"

    extern bool __real_load_ipmi_config_file(void);

    extern bool __real_get_bmcs_for_aggregator(char*, ipmi_collector**, int*);

    extern opal_event_base_t* __real_opal_progress_thread_init(const char*);

    extern int __real_get_sdr_cache(unsigned char **pcache);

    extern void __real_free_sdr_cache(unsigned char **pcache);

    extern int __real_find_sdr_next(unsigned char *psdr, unsigned char *pcache, unsigned short id);

    extern int __real_ipmi_cmd_mc(unsigned short, unsigned char, int, unsigned char, int, unsigned char, char);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_IPMI_PARSER_INTERFACE_MOCKED_H

