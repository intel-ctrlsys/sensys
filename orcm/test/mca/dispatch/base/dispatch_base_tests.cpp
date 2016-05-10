/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
// C++
#include <iostream>
#include "dispatch_base_tests.h"
// OPAL
#include "opal/dss/dss.h"
#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"

extern "C" {
    // C
    #include "memory.h"

    // ORCM
    #include "orcm/runtime/orcm_globals.h"
    #include "orcm/mca/dispatch/base/base.h"

};

#define MY_ORCM_SUCCESS 0
#define MY_MODULE_PRIORITY 20

// Fixture
using namespace std;


static void comp_init(void);
static void comp_finalize(void);
//static void comp_generate(void);

orcm_dispatch_base_module_t orcm_dispatch_sample_module = {
        comp_init,
        comp_finalize,
        //comp_generate
};

orcm_dispatch_base_module_t orcm_dispatch_sample_null_api_module = {
        NULL,
        NULL,
        NULL,
};

static void comp_init(void)
{
    return;
}

static void comp_finalize(void)
{
    return;
}

int component_query_null_module(mca_base_module_t **module, int *priority)
{
    *module = NULL;
    *priority = MY_MODULE_PRIORITY;
    return MY_ORCM_SUCCESS;
}

int component_query_with_null_api_module(mca_base_module_t **module, int *priority)
{
    *module = (mca_base_module_t*)&orcm_dispatch_sample_null_api_module;
    *priority = MY_MODULE_PRIORITY;
    return MY_ORCM_SUCCESS;
}

int component_query_with_module(mca_base_module_t **module, int *priority)
{
    *module = (mca_base_module_t*)&orcm_dispatch_sample_module;
    *priority = MY_MODULE_PRIORITY;
    return MY_ORCM_SUCCESS;
}

void ut_dispatch_base_tests::reset_module_list()
{
    OPAL_LIST_DESTRUCT(&orcm_dispatch_base.actives);
    OBJ_CONSTRUCT(&orcm_dispatch_base.actives, opal_list_t);
}

void ut_dispatch_base_tests::SetUpTestCase()
{
    OBJ_CONSTRUCT(&orcm_dispatch_base_framework.framework_components, opal_list_t);
    OBJ_CONSTRUCT(&orcm_dispatch_base.actives, opal_list_t);
}

void ut_dispatch_base_tests::TearDownTestCase()
{
    OPAL_LIST_DESTRUCT(&orcm_dispatch_base_framework.framework_components);
    OPAL_LIST_DESTRUCT(&orcm_dispatch_base.actives);

}

void ut_dispatch_base_tests::reset_base_framework_comp()
{
    OPAL_LIST_DESTRUCT(&orcm_dispatch_base_framework.framework_components);
    OBJ_CONSTRUCT(&orcm_dispatch_base_framework.framework_components, opal_list_t);
    orcm_dispatch_base_reset_selected();
}

TEST_F(ut_dispatch_base_tests, dispatch_base_double_select)
{
    int ret;

    ret = orcm_dispatch_base_select();
    ret = orcm_dispatch_base_select();

    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_dispatch_base_tests, dispatch_base_select_NULL_component)
{
    int ret;
    orcm_dispatch_base_component_t *dispatch_component = NULL;
    mca_base_component_list_item_t *component;

    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) dispatch_component;

    opal_list_append(&orcm_dispatch_base_framework.framework_components, (opal_list_item_t *)component);

    ret = orcm_dispatch_base_select();

    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_dispatch_base_tests, dispatch_base_select_NULL_query_component)
{
    int ret;
    orcm_dispatch_base_component_t dispatch_component;
    mca_base_component_list_item_t *component;

    ut_dispatch_base_tests::reset_base_framework_comp();
    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) &dispatch_component;

    dispatch_component.base_version.mca_query_component = NULL;

    opal_list_append(&orcm_dispatch_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_dispatch_base_select();

    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_dispatch_base_tests, dispatch_base_select_query_component_with_NULL_module)
{
    int ret;
    orcm_dispatch_base_component_t dispatch_component;
    mca_base_component_list_item_t *component;

    ut_dispatch_base_tests::reset_base_framework_comp();
    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) &dispatch_component;

    dispatch_component.base_version.mca_query_component = component_query_null_module;

    opal_list_append(&orcm_dispatch_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_dispatch_base_select();

    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_dispatch_base_tests, dispatch_base_select_query_component_with_NULL_init_module)
{
    int ret;
    orcm_dispatch_base_component_t dispatch_component;
    mca_base_component_list_item_t *component;

    ut_dispatch_base_tests::reset_base_framework_comp();
    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) &dispatch_component;

    dispatch_component.base_version.mca_query_component = component_query_with_null_api_module;

    opal_list_append(&orcm_dispatch_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_dispatch_base_select();
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_dispatch_base_tests, dispatch_base_select_query_component_with_one_module)
{
    int ret;
    orcm_dispatch_base_component_t dispatch_component;
    mca_base_component_list_item_t *component;

    ut_dispatch_base_tests::reset_base_framework_comp();
    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) &dispatch_component;

    dispatch_component.base_version.mca_query_component = component_query_with_module;

    opal_list_append(&orcm_dispatch_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_dispatch_base_select();
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_dispatch_base_tests, dispatch_base_frame_print_type_led)
{
    const char* ret = NULL;
    const char* act_val = "CHASSIS_ID_LED";
    int ev_type = 4;
    bool result = false;
    bool exp_ret = true;

    ret = orcm_dispatch_base_print_type(ev_type);

    if( std::string(ret) == std::string(act_val)){
        result = true;
    }

    EXPECT_EQ(exp_ret, result);
}

TEST_F(ut_dispatch_base_tests, dispatch_base_frame_print_type_trans)
{
    const char* ret = NULL;
    const char* act_val = "TRANSITION";
    int ev_type = 3;
    bool result = false;
    bool exp_ret = true;

    ret = orcm_dispatch_base_print_type(ev_type);

    if( std::string(ret) == std::string(act_val)){
        result = true;
    }

    EXPECT_EQ(exp_ret, result);
}

TEST_F(ut_dispatch_base_tests, dispatch_base_frame_print_type_default)
{
    const char* ret = NULL;
    const char* act_val = "UNKNOWN";
    int ev_type = MY_MODULE_PRIORITY;
    bool result = false;
    bool exp_ret = true;

    ret = orcm_dispatch_base_print_type(ev_type);

    if( std::string(ret) == std::string(act_val)){
        result = true;
    }

    EXPECT_EQ(exp_ret, result);
}

TEST_F(ut_dispatch_base_tests, dispatch_base_frame_print_severity_alert)
{
    const char* ret = NULL;
    const char* act_val = "ALERT";
    int ev_type = 1;
    bool result = false;
    bool exp_ret = true;

    ret = orcm_dispatch_base_print_severity(ev_type);

    if( std::string(ret) == std::string(act_val)){
        result = true;
    }

    EXPECT_EQ(exp_ret, result);
}

TEST_F(ut_dispatch_base_tests, dispatch_base_frame_print_severity_debug)
{
    const char* ret = NULL;
    const char* act_val = "DEBUG";
    int ev_type = 7;
    bool result = false;
    bool exp_ret = true;

    ret = orcm_dispatch_base_print_severity(ev_type);

    if( std::string(ret) == std::string(act_val)){
        result = true;
    }

    EXPECT_EQ(exp_ret, result);
}

TEST_F(ut_dispatch_base_tests, dispatch_base_frame_print_severity_unknown)
{
    const char* ret = NULL;
    const char* act_val = "UNKNOWN";
    int ev_type = 8;
    bool result = false;
    bool exp_ret = true;

    ret = orcm_dispatch_base_print_severity(ev_type);

    if( std::string(ret) == std::string(act_val)){
        result = true;
    }

    EXPECT_EQ(exp_ret, result);
}


TEST_F(ut_dispatch_base_tests, dispatch_base_frame_print_severity_default)
{
    const char* ret = NULL;
    const char* act_val = "UNKNOWN";
    int ev_type = MY_MODULE_PRIORITY;
    bool result = false;
    bool exp_ret = true;

    ret = orcm_dispatch_base_print_severity(ev_type);

    if( std::string(ret) == std::string(act_val)){
        result = true;
    }

    EXPECT_EQ(exp_ret, result);
}
