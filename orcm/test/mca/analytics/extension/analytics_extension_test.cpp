/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "analytics_extension_test.hpp"

#include "gtest/gtest.h"
#include "orcm/test/mca/analytics/util/analytics_util.h"
#include "orcm/mca/analytics/analytics_interface.h"
#include "orcm/mca/analytics/base/analytics_factory.h"
#include "analytics_extension_mocking.h"
#include <list>


extern "C" {
    extern orcm_analytics_base_module_t* extension_component_create(void);
    extern bool extension_component_avail(void);
}
extern opal_list_t* convert_to_opal_list(dataContainer& dc);
extern opal_list_t* convert_to_event_list(std::list<Event>& events, orcm_workflow_caddy_t* current_caddy, char* event_key);
extern bool is_orcm_util_load_time_expected_succeed;
extern bool database_log_fail;
extern bool is_orcm_util_append_expected_succeed;
extern bool is_malloc_expected_succeed;

class sample : public Analytics
{
public:
    int analyze(DataSet& ds);
};

int sample::analyze(DataSet& ds)
{
    return 0;
}

TEST(analytics_extension, component_avail)
{
    ASSERT_TRUE(extension_component_avail());
}

TEST(analytics_extension, test_extension_component_create)
{
    is_malloc_expected_succeed = false;
    ASSERT_EQ((void*)NULL, (void*)extension_component_create());
    is_malloc_expected_succeed = true;
    orcm_analytics_base_module_t* mod = extension_component_create();
    ASSERT_NE((void*)NULL, (void*)mod);
    free(mod);
}

TEST(analytics_extension, init_null_input)
{
    int rc = orcm_analytics_extension_module.api.init(NULL);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(analytics_extension, init_test)
{
    int rc = -1;

    mca_analytics_extension_module_t *mod = NULL;
    orcm_analytics_base_module_t *base_mod = (orcm_analytics_base_module_t*)malloc(
                                          sizeof(orcm_analytics_base_module_t));
    if (NULL != base_mod) {
        rc = orcm_analytics_extension_module.api.init(base_mod);
        mod = (mca_analytics_extension_module_t*)base_mod;
        free(mod);
        ASSERT_EQ(ORCM_SUCCESS, rc);
    }
}

TEST(analytics_extension, finalize_null_input)
{
    orcm_analytics_extension_module.api.finalize(NULL);
}

TEST(analytics_extension, finalize_test)
{
    orcm_analytics_base_module_t *base_mod = (orcm_analytics_base_module_t*)malloc(
                                                  sizeof(orcm_analytics_base_module_t));
    if (NULL != base_mod) {
        orcm_analytics_extension_module.api.finalize(base_mod);
    }
}


TEST(analytics_extension, analyze_null_caddy)
{
    int rc = orcm_analytics_extension_module.api.analyze(0, 0, NULL);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}


TEST(analytics_extension, analyze_null_mod)
{
    int rc = -1;

    orcm_workflow_caddy_t *caddy = analytics_util::create_caddy(NULL, NULL, NULL, NULL, NULL);
    if (NULL != caddy) {
        rc = orcm_analytics_extension_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
    }
}

TEST(analytics_extension, analyze_null_wf)
{
    int rc = -1;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
                        malloc(sizeof(orcm_analytics_base_module_t)), NULL, NULL, NULL, NULL);
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_extension_module.api.analyze(0, 0, caddy);
        orcm_analytics_extension_module.api.finalize(mod);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
    }
}

TEST(analytics_extension, analyze_null_wfstep)
{
    int rc = -1;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t), NULL, NULL, NULL);
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_extension_module.api.analyze(0, 0, caddy);
        orcm_analytics_extension_module.api.finalize(mod);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
    }
}

TEST(analytics_extension, analyze_null_analytics_value)
{
    int rc = -1;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), NULL, NULL);
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_extension_module.api.analyze(0, 0, caddy);
        orcm_analytics_extension_module.api.finalize(mod);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
    }
}

TEST(analytics_extension, analyze_no_plugin)
{
    int rc = -1;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
                          malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, NULL);
    double value = 37.5;
    struct timeval time = {2500, 0};
    if (NULL != caddy) {
        if (NULL != caddy->imod) {
            mod = caddy->imod;
        }
        analytics_util::fill_analytics_value(caddy, "localhost", &time, &value, 1);
        if(caddy->wf_step!= NULL){
            caddy->wf_step->analytic = strdup("test");
        }
        rc = orcm_analytics_extension_module.api.analyze(0, 0, caddy);
        orcm_analytics_extension_module.api.finalize(mod);
        ASSERT_EQ(ORCM_ERR_OUT_OF_RESOURCE, rc);
    }
}

TEST(analytics_extension, analyze_valid_input)
{
    int rc = -1;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
                          malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, NULL);
    double value = 37.5;
    struct timeval time = {2500, 0};
    if (NULL != caddy) {
        if (NULL != caddy->imod) {
            mod = caddy->imod;
        }
        if (NULL != caddy->wf_step) {
            analytics_util::fill_attribute(&(caddy->wf_step->attributes), "db", "no");
        }
        analytics_util::fill_analytics_value(caddy, "localhost", &time, &value, 1);
        caddy->imod->orcm_mca_analytics_data_store = new sample();
        is_orcm_util_load_time_expected_succeed = true;
        rc = orcm_analytics_extension_module.api.analyze(0, 0, caddy);
        orcm_analytics_extension_module.api.finalize(mod);
        ASSERT_EQ(ORCM_SUCCESS, rc);
    }
}

TEST(analytics_extension, analyze_valid_input_load_val_fail)
{
    int rc = -1;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
                          malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, NULL);
    double value = 37.5;
    struct timeval time = {2500, 0};
    if (NULL != caddy) {
        if (NULL != caddy->imod) {
            mod = caddy->imod;
        }
        analytics_util::fill_analytics_value(caddy, "localhost", &time, &value, 1);
        caddy->imod->orcm_mca_analytics_data_store = new sample();
        is_orcm_util_load_time_expected_succeed = false;
        rc = orcm_analytics_extension_module.api.analyze(0, 0, caddy);
        orcm_analytics_extension_module.api.finalize(mod);
        ASSERT_EQ(ORCM_ERR_OUT_OF_RESOURCE, rc);
    }
}

TEST(analytics_extension, analyze_db_logging_fail)
{
    int rc = -1;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
                          malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, NULL);
    double value = 37.5;
    struct timeval time = {2500, 0};
    if (NULL != caddy) {
        if (NULL != caddy->imod) {
            mod = caddy->imod;
        }
        if (NULL != caddy->wf_step) {
            analytics_util::fill_attribute(&(caddy->wf_step->attributes), "db", "yes");
        }
        analytics_util::fill_analytics_value(caddy, "localhost", &time, &value, 1);
        caddy->imod->orcm_mca_analytics_data_store = new sample();
        is_orcm_util_load_time_expected_succeed = true;
        database_log_fail = true;
        rc = orcm_analytics_extension_module.api.analyze(0, 0, caddy);
        orcm_analytics_extension_module.api.finalize(mod);
        ASSERT_EQ(ORCM_ERROR, rc);
    }
}



TEST(analytics_extension, convert_to_event_list_empty_list)
{
    std::list<Event> events;
    opal_list_t* evlist = convert_to_event_list(events, NULL, NULL);
}

TEST(analytics_extension, convert_to_event_list_valid_list)
{
    std::list<Event> events;
    char* event_key = strdup("Test_example");
    Event ev = {
    "crit",
    "syslog",
    "Test",
    34.56,
    "C",
    };
    events.push_back(ev);
    orcm_workflow_step_t* wf_step = OBJ_NEW(orcm_workflow_step_t);
    double value = 37.5;
    struct timeval time = {2500, 0};
    wf_step->attributes = *OBJ_NEW(opal_list_t);
    orcm_workflow_caddy_t *caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
                          malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          wf_step, NULL, NULL);
    analytics_util::fill_analytics_value(caddy, "localhost", &time, &value, 1);
    opal_list_t* evlist = convert_to_event_list(events, caddy, event_key);
}

TEST(analytics_extension, convert_to_opal_list)
{
    dataContainer dc;
    char* key = strdup("Test_example");
    char* data = strdup("Test_data");
    dc.put<std::string>(key, data, "");
    is_orcm_util_append_expected_succeed = true;
    opal_list_t* res_list = convert_to_opal_list(dc);
    OBJ_RELEASE(res_list);
    is_orcm_util_append_expected_succeed = false;
    res_list = convert_to_opal_list(dc);
    OBJ_RELEASE(res_list);
}



