/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "gtest/gtest.h"
#include "analytics_genex_test.h"
#include "analytics_util.h"

#define SAFE_FREE(x) if(NULL != x) free((void*)x); x=NULL
#define SAFE_OBJ_FREE(x) if(NULL!=x) OBJ_RELEASE(x); x=NULL

genex_workflow_value_t* create_workflow()
{
    return (genex_workflow_value_t*)malloc(sizeof(genex_workflow_value_t));
}

orcm_workflow_caddy_t* create_caddy()
{
    orcm_value_t* orcm_value;
    orcm_workflow_caddy_t* caddy = OBJ_NEW(orcm_workflow_caddy_t);

    opal_list_t* key = OBJ_NEW(opal_list_t);
    opal_list_t* non_compute = OBJ_NEW(opal_list_t);
    opal_list_t* compute = OBJ_NEW(opal_list_t);

    caddy->analytics_value =
        orcm_util_load_orcm_analytics_value(key, non_compute, compute);

    orcm_value = analytics_util::load_orcm_value("severity", (void*)"crit", OPAL_STRING, "sev");
    opal_list_append(caddy->analytics_value->compute_data, &orcm_value->value.super);

    orcm_value = analytics_util::load_orcm_value("message", (void*)"hello", OPAL_STRING, "log");
    opal_list_append(caddy->analytics_value->compute_data, &orcm_value->value.super);

    return caddy;
}

void destroy_caddy(orcm_workflow_caddy_t* caddy)
{
    SAFE_OBJ_FREE(caddy->analytics_value->key);
    SAFE_OBJ_FREE(caddy->analytics_value->compute_data);
    SAFE_OBJ_FREE(caddy->analytics_value->non_compute_data);
    SAFE_OBJ_FREE(caddy->analytics_value);
    SAFE_OBJ_FREE(caddy);
}

TEST(analytics_genex, init_null_input)
{
    int rc = orcm_analytics_genex_module.api.init(NULL);

    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(analytics_genex, finalize_input)
{
    orcm_analytics_base_module_t *imod =
        (orcm_analytics_base_module_t*)malloc(sizeof(orcm_analytics_base_module_t));

    if ( NULL != imod )
    {
        int rc = orcm_analytics_genex_module.api.init(imod);

        ASSERT_EQ(ORCM_SUCCESS, rc);

        orcm_analytics_genex_module.api.finalize(imod);
    }
}

TEST(analytics_genex, finalize_input_null)
{
    orcm_analytics_base_module_t *imod = NULL;

    orcm_analytics_genex_module.api.finalize(imod);
}

TEST(analytics_genex, analyze_null_input)
{
    int rc = orcm_analytics_genex_module.api.analyze(0, 0, NULL);

    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(analytics_genex, monitor_null_input)
{
    int rc = monitor_genex(NULL, NULL, NULL);

    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(analytics_genex, get_monitor_wrong_regex_input)
{
    opal_list_t* event_list;

    orcm_workflow_caddy_t *caddy;
    caddy = analytics_util::create_caddy(NULL, NULL, NULL, NULL, NULL);

    genex_workflow_value_t *wf_value = create_workflow();
    wf_value = create_workflow();
    wf_value->msg_regex = strdup("(()");
    wf_value->severity = strdup("crit");

    int rc = monitor_genex(wf_value, caddy, event_list);

    SAFE_OBJ_FREE(caddy);
    SAFE_FREE(wf_value);

    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(analytics_genex, get_monitor_wrong_notifier)
{
    opal_list_t* event_list;

    orcm_analytics_base_module_t *imod =
        (orcm_analytics_base_module_t*)malloc(sizeof(orcm_analytics_base_module_t));

    orcm_analytics_genex_module.api.init(imod);

    orcm_workflow_caddy_t* caddy = create_caddy();

    genex_workflow_value_t *wf_value = create_workflow();

    wf_value->msg_regex = strdup("hello");
    wf_value->severity = strdup("crit");

    int rc = monitor_genex(wf_value, caddy, event_list);

    SAFE_FREE(wf_value);

    destroy_caddy(caddy);

    orcm_analytics_genex_module.api.finalize(imod);

    ASSERT_EQ(ORCM_ERROR, rc);

}

TEST(analytics_genex, get_monitor_no_match)
{
    opal_list_t* event_list;

    orcm_analytics_base_module_t *imod =
      (orcm_analytics_base_module_t*)malloc(sizeof(orcm_analytics_base_module_t));

    orcm_analytics_genex_module.api.init(imod);

    orcm_workflow_caddy_t* caddy = create_caddy();

    genex_workflow_value_t *wf_value = create_workflow();

    wf_value->msg_regex = strdup("no Match");
    wf_value->severity = strdup("crit");

    int rc = monitor_genex(wf_value, caddy, event_list);

    SAFE_FREE(wf_value);

    destroy_caddy(caddy);

    orcm_analytics_genex_module.api.finalize(imod);

    ASSERT_EQ(ORCM_SUCCESS, rc);

}

TEST(analytics_genex, get_genex_policy_workflow_null_input)
{
    int rc = get_genex_policy(NULL, NULL);

    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(analytics_genex, get_genex_policy_wfvalue_null_input)
{
    genex_workflow_value_t *wf_value = create_workflow();

    int rc = get_genex_policy(NULL, wf_value);

    SAFE_FREE(wf_value);

    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(analytics_genex, get_genex_policy_wrong_cbdata_input)
{
    void *cbdata = malloc(1024);

    genex_workflow_value_t *wf_value = create_workflow();

    int rc = get_genex_policy(cbdata, wf_value);

    SAFE_FREE(wf_value);
    SAFE_FREE(cbdata);

    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(analytics_genex, get_genenx_policy_duplicate_wf_data)
{
    opal_list_t* event_list;

    orcm_analytics_base_module_t *mod =
        (orcm_analytics_base_module_t*)malloc(sizeof(orcm_analytics_base_module_t));

    orcm_analytics_genex_module.api.init(mod);

    orcm_workflow_caddy_t* caddy = create_caddy();

     caddy->imod = (orcm_analytics_base_module_t*)mod;

    genex_workflow_value_t *wf_value = create_workflow();

    wf_value->msg_regex = strdup("hello");
    wf_value->severity = strdup("crit");
    wf_value->notifier = strdup("smtp");
    wf_value->db = strdup("yes");

    void *cbdata = (void*)caddy;

    caddy->wf = OBJ_NEW(orcm_workflow_t);
    caddy->wf_step = OBJ_NEW(orcm_workflow_step_t);

    analytics_util::fill_attribute(&(caddy->wf_step->attributes), "msg_regex", "hello1");
    analytics_util::fill_attribute(&(caddy->wf_step->attributes), "severity", "crit1");
    analytics_util::fill_attribute(&(caddy->wf_step->attributes), "notifier", "smtp1");
    analytics_util::fill_attribute(&(caddy->wf_step->attributes), "db" , "yes1");

    analytics_util::fill_attribute(&(caddy->wf_step->attributes), "msg_regex", "hello2");
    analytics_util::fill_attribute(&(caddy->wf_step->attributes), "severity", "crit2");
    analytics_util::fill_attribute(&(caddy->wf_step->attributes), "notifier", "smtp2");
    analytics_util::fill_attribute(&(caddy->wf_step->attributes), "db" , "yes2");

    int rc = get_genex_policy(cbdata, wf_value);

    SAFE_FREE(wf_value);

    SAFE_OBJ_FREE(caddy->wf);
    SAFE_OBJ_FREE(caddy->wf_step);

    destroy_caddy(caddy);

    orcm_analytics_genex_module.api.finalize(mod);

    ASSERT_EQ(ORCM_SUCCESS, rc);

}

TEST(analytics_genex, analize_monitor_genex_failure)
{
    orcm_workflow_caddy_t* caddy = create_caddy();

    orcm_analytics_base_module_t *mod =
        (orcm_analytics_base_module_t*)malloc(sizeof(orcm_analytics_base_module_t));

    caddy->imod = (orcm_analytics_base_module_t*)mod;

    caddy->wf = OBJ_NEW(orcm_workflow_t);
    caddy->wf_step = OBJ_NEW(orcm_workflow_step_t);

    int rc = orcm_analytics_genex_module.api.analyze(0, 0, caddy);

    SAFE_FREE(mod);
    SAFE_OBJ_FREE(caddy->wf);
    SAFE_OBJ_FREE(caddy->wf_step);

    ASSERT_EQ(ORCM_ERROR, rc);
}
