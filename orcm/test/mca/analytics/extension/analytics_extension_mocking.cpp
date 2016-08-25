/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "analytics_extension_mocking.h"
#include "analytics_extension_test.hpp"
#include "orcm/mca/analytics/analytics_interface.h"
#include "orcm/mca/analytics/base/analytics_factory.h"
#include "orcm/util/utils.h"

#include <iostream>
using namespace std;

bool is_orcm_util_load_time_expected_succeed = true;
bool database_log_fail = false;
bool is_orcm_util_append_expected_succeed = true;
bool is_malloc_expected_succeed = true;

extern "C" {
    orcm_analytics_value_t* __wrap_orcm_util_load_analytics_time_compute(opal_list_t* key, opal_list_t* non_compute, opal_list_t* compute){
        if(!is_orcm_util_load_time_expected_succeed){
            return NULL;
        }
        return __real_orcm_util_load_analytics_time_compute(key, non_compute, compute);
    }
    int __wrap_orcm_analytics_base_log_to_database_event(orcm_analytics_value_t* value){
        if(database_log_fail){
            return ORCM_ERROR;
        }
        return __real_orcm_analytics_base_log_to_database_event(value);
    }
    orcm_value_t* __wrap_orcm_util_load_orcm_value(char *key, void *data, opal_data_type_t type, char *units){
        if(!is_orcm_util_append_expected_succeed){
            return NULL;
        }
        return __real_orcm_util_load_orcm_value(key, data, type, units);
    }
    void* __wrap_malloc(size_t size)
    {
        if(is_malloc_expected_succeed){
            return __real_malloc(size);
        }
        return NULL;
    }
}
