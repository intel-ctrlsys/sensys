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

TEST(analytics_genex, init_null_input)
{
    int rc = orcm_analytics_genex_module.api.init(NULL);
    ASSERT_EQ(ORCM_ERROR, rc);
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
    void *cbdata = malloc(1024);

    genex_workflow_value_t *workflow_value =
        (genex_workflow_value_t*)malloc(sizeof(genex_workflow_value_t));

    workflow_value->msg_regex = strdup("(()");
    int rc = monitor_genex(workflow_value,cbdata,event_list);
    ASSERT_EQ(ORCM_ERROR, rc);

    free(workflow_value);
    free(cbdata);
}

TEST(analytics_genex, get_genex_policy_workflow_null_input)
{
    int rc = get_genex_policy(NULL, NULL);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(analytics_genex, get_genex_policy_wfvalue_null_input)
{
    void *workflow_value = malloc(sizeof(genex_workflow_value_t));
    int rc = get_genex_policy(NULL, (genex_workflow_value_t*)workflow_value);
    ASSERT_EQ(ORCM_ERROR, rc);
    free(workflow_value);
}

TEST(analytics_genex, get_genex_policy_wrong_cbdata_input)
{
    void *cbdata = malloc(1024);
    void *workflow_value = malloc(sizeof(genex_workflow_value_t));
    int rc = get_genex_policy(cbdata, (genex_workflow_value_t*)workflow_value);
    ASSERT_EQ(ORCM_ERROR, rc);
    free(workflow_value);
    free(cbdata);
}
