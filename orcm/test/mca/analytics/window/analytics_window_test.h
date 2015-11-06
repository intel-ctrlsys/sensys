/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef GREI_ORCM_TEST_MCA_ANALYTICS_WINDOW_ANALYTICS_WINDOW_TEST_H_
#define GREI_ORCM_TEST_MCA_ANALYTICS_WINDOW_ANALYTICS_WINDOW_TEST_H_

extern "C" {
    #include "orcm/mca/analytics/window/analytics_window.h"
    #include "orcm/util/utils.h"
}

using namespace std;

opal_value_t* load_opal_value(string key, string value, opal_data_type_t type);

orcm_value_t* load_orcm_value(string key, void *data, opal_data_type_t type, string units);

char* cppstr_to_cstr(string str);

orcm_workflow_caddy_t *create_caddy(orcm_analytics_base_module_t *mod,
                                    orcm_workflow_t *workflow,
                                    orcm_workflow_step_t *workflow_step,
                                    orcm_analytics_value_t *analytics_value,
                                    void *data_store);

#endif /* GREI_ORCM_TEST_MCA_ANALYTICS_WINDOW_ANALYTICS_WINDOW_TEST_H_ */
