/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ANALYTICS_EXTENSION_MOCKING_H
#define ANALYTICS_EXTENSION_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    #include "opal/class/opal_list.h"
    #include "orcm/util/utils.h"

    extern orcm_analytics_value_t* __real_orcm_util_load_analytics_time_compute(
            opal_list_t* key, opal_list_t* non_compute, opal_list_t* compute);
    extern int __real_orcm_analytics_base_log_to_database_event(orcm_analytics_value_t* value);
    extern orcm_value_t* __real_orcm_util_load_orcm_value(char *key, void *data, opal_data_type_t type, char *units);
    extern char* __real_malloc(size_t size);
#ifdef __cplusplus
}
#endif // __cplusplus



extern bool is_orcm_util_load_time_expected_succeed;
extern bool database_log_fail;
extern bool is_orcm_util_append_expected_succeed;
extern bool is_malloc_expected_succeed;


#endif // ANALYTICS_EXTENSION_MOCKING_H
