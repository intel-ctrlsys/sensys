/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "analytics_util.h"
#include <stdlib.h>

char* analytics_util::cppstr_to_cstr(string str)
{
    char* cstr = NULL;

    if (!str.empty()) {
        cstr = (char*)str.c_str();
    }

    return cstr;
}

opal_value_t* analytics_util::load_opal_value(string key, string value, opal_data_type_t type)
{
    char* key_c = cppstr_to_cstr(key);
    void* value_c = (void*)cppstr_to_cstr(value);
    opal_value_t* opal_value = orcm_util_load_opal_value(key_c, value_c, type);
    return opal_value;
}

orcm_value_t* analytics_util::load_orcm_value(string key, void* data,
                                              opal_data_type_t type, string units)
{
    char* key_c = cppstr_to_cstr(key);
    char* units_c = cppstr_to_cstr(units);
    orcm_value_t* orcm_value = orcm_util_load_orcm_value(key_c, data, type, units_c);
    return orcm_value;
}

orcm_workflow_caddy_t* analytics_util::create_caddy(orcm_analytics_base_module_t* mod,
                                                    orcm_workflow_t* workflow,
                                                    orcm_workflow_step_t* workflow_step,
                                                    orcm_analytics_value_t* analytics_value,
                                                    void* data_store)
{
    orcm_workflow_caddy_t* caddy = OBJ_NEW(orcm_workflow_caddy_t);
    if (NULL != caddy) {
        caddy->imod = mod;
        caddy->wf = workflow;
        caddy->wf_step = workflow_step;
        caddy->analytics_value = analytics_value;
        if (NULL != caddy->imod) {
            caddy->imod->orcm_mca_analytics_data_store = data_store;
        }
    }
    return caddy;
}

void analytics_util::fill_attribute(opal_list_t* list, string key, string value)
{
    opal_value_t* attribute = NULL;
    if (!value.empty()) {
        if (NULL != (attribute = load_opal_value(key, value, OPAL_STRING))) {
            opal_list_append(list, (opal_list_item_t*)attribute);
        }
    }
}

void analytics_util::fill_analytics_value(orcm_workflow_caddy_t* caddy, string hostname,
                                          struct timeval* time, double* value, int count)
{
    int index = 0;
    orcm_value_t* orcm_value = NULL;
    char* c_hostname = cppstr_to_cstr(hostname);
    caddy->analytics_value = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL);

    if (NULL != caddy->analytics_value) {
        if (NULL != (orcm_value = load_orcm_value("hostname", c_hostname, OPAL_STRING, ""))) {
            opal_list_append(caddy->analytics_value->key, (opal_list_item_t*)orcm_value);
        }
        if (NULL != (orcm_value = load_orcm_value("ctime", time, OPAL_TIMEVAL, ""))) {
            opal_list_append(caddy->analytics_value->non_compute_data,
                             (opal_list_item_t*)orcm_value);
        }

        for (index = 0; index < count; index++) {
            if (NULL != (orcm_value = load_orcm_value("core 1", value, OPAL_DOUBLE, "C"))) {
                opal_list_append(caddy->analytics_value->compute_data, (opal_list_item_t*)orcm_value);
            }
        }
    }
}
