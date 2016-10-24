/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

extern "C" {
    #include "orcm/util/utils.h"
}

#include <string>

using namespace std;

class analytics_util {
public:
    static char* cppstr_to_cstr(string str);
    static opal_value_t* load_opal_value(string key, string value, opal_data_type_t type);
    static orcm_value_t* load_orcm_value(string key, void* data, opal_data_type_t type, string units);
    static orcm_workflow_caddy_t* create_caddy(orcm_analytics_base_module_t* mod,
                                               orcm_workflow_t* workflow,
                                               orcm_workflow_step_t* workflow_step,
                                               orcm_analytics_value_t* analytics_value,
                                               void* data_store);
    static void fill_attribute(opal_list_t* list, string key, string value);
    static void fill_analytics_value(orcm_workflow_caddy_t* caddy, string hostname,
                                     struct timeval* time, double* value, int count);
};
