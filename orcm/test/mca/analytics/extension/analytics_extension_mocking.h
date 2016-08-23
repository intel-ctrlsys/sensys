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
#ifdef __cplusplus
}
#endif // __cplusplus



extern bool is_orcm_util_load_time_expected_succeed;
extern bool database_log_fail;

#endif // ANALYTICS_EXTENSION_MOCKING_H
