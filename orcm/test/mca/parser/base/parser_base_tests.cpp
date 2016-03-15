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
#include "parser_base_tests.h"
// OPAL
#include "opal/dss/dss.h"
#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"

extern "C" {
    // C
    #include "memory.h"

    // ORCM
    #include "orcm/runtime/orcm_globals.h"
    #include "orcm/mca/parser/base/base.h"

};

#define MY_ORCM_SUCCESS 0
#define MY_MODULE_PRIORITY 20

// Fixture
using namespace std;

static void comp_init(void);
static void comp_finalize(void);
static int comp_open(char const *file);
static int comp_close(int file_id);
static opal_list_t* comp_retrieve_section(int file_id, opal_list_item_t *start,
                                          char const* key, char const* name);
static void comp_write_section(opal_list_t *result, int file_id, char const *key);

orcm_parser_base_module_t orcm_parser_sample_module = {
    comp_init,
    comp_finalize,
    comp_open,
    comp_close,
    comp_retrieve_section,
    comp_write_section
};

orcm_parser_base_module_t orcm_parser_sample_null_api_module = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static void comp_init(void)
{
    return;
}

static void comp_finalize(void)
{
    return;
}

static int comp_open(char const *file)
{
    int random_file_id = MY_ORCM_SUCCESS;
    return random_file_id;
}

static int comp_close(int file_id)
{
    return MY_ORCM_SUCCESS;
}

static opal_list_t* comp_retrieve_section(int file_id, opal_list_item_t *start,
                                           char const* key, char const* name)
{
    return NULL;
}

static void comp_write_section(opal_list_t *result, int file_id, char const *key)
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
    *module = (mca_base_module_t*)&orcm_parser_sample_null_api_module;
    *priority = MY_MODULE_PRIORITY;
    return MY_ORCM_SUCCESS;
}

int component_query_with_module(mca_base_module_t **module, int *priority)
{
    *module = (mca_base_module_t*)&orcm_parser_sample_module;
    *priority = MY_MODULE_PRIORITY;
    return MY_ORCM_SUCCESS;
}

void ut_parser_base_tests::SetUpTestCase()
{
    OBJ_CONSTRUCT(&orcm_parser_base_framework.framework_components, opal_list_t);
    OBJ_CONSTRUCT(&orcm_parser_base.actives, opal_list_t);
}

void ut_parser_base_tests::TearDownTestCase()
{
    OPAL_LIST_DESTRUCT(&orcm_parser_base_framework.framework_components);
    OPAL_LIST_DESTRUCT(&orcm_parser_base.actives);

}


TEST_F(ut_parser_base_tests, parser_base_double_select)
{
    int ret;

    ret = orcm_parser_base_select();
    ret = orcm_parser_base_select();
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}



TEST_F(ut_parser_base_tests, parser_base_select_NULL_component)
{
    int ret;
    orcm_parser_base_component_t *parser_component = NULL;
    mca_base_component_list_item_t *component;

    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) parser_component;

    opal_list_append(&orcm_parser_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_parser_base_select();
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_select_NULL_query_component)
{
    int ret;
    orcm_parser_base_component_t *parser_component;
    mca_base_component_list_item_t *component;

    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) parser_component;

    parser_component->base_version.mca_query_component = NULL;

    opal_list_append(&orcm_parser_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_parser_base_select();
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_select_query_component_with_NULL_module)
{
    int ret;
    orcm_parser_base_component_t *parser_component;
    mca_base_component_list_item_t *component;

    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) parser_component;

    parser_component->base_version.mca_query_component = component_query_null_module;

    opal_list_append(&orcm_parser_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_parser_base_select();
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_select_query_component_with_NULL_init_module)
{
    int ret;
    orcm_parser_base_component_t *parser_component;
    mca_base_component_list_item_t *component;

    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) parser_component;

    parser_component->base_version.mca_query_component = component_query_with_null_api_module;

    opal_list_append(&orcm_parser_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_parser_base_select();
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_select_query_component_with_one_module)
{
    int ret;
    orcm_parser_base_component_t *parser_component;
    mca_base_component_list_item_t *component;

    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) parser_component;

    parser_component->base_version.mca_query_component = component_query_with_module;

    opal_list_append(&orcm_parser_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_parser_base_select();
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_no_active_module_open_file)
{
    int ret;

    //This file need not exist. I am testing framework API. Not the plugin level
    ret = orcm_parser.open("sample.xml");
    ASSERT_NE(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_open_file)
{
    int ret;

    orcm_parser_active_module_t *act_module;

    act_module = OBJ_NEW(orcm_parser_active_module_t);

    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_sample_module;

    opal_list_append(&orcm_parser_base.actives, &act_module->super);

    //This file need not exist. I am testing framework API. Not the plugin level
    ret = orcm_parser.open("sample.xml");
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_no_active_module_close_file)
{
    int ret;

    //Random number to test. I am testing framework API. Not the plugin level
    ret = orcm_parser.close(1);
    ASSERT_NE(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_close_file)
{
    int ret;

    orcm_parser_active_module_t *act_module;

    act_module = OBJ_NEW(orcm_parser_active_module_t);

    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_sample_module;

    opal_list_append(&orcm_parser_base.actives, &act_module->super);

    //Random number to test. I am testing framework API. Not the plugin level
    ret = orcm_parser.close(1);
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_no_active_module_retrieve_section)
{
    //Random values to test. I am testing framework API. Not the plugin level
    orcm_parser.retrieve_section(1, NULL, "", "");
}

TEST_F(ut_parser_base_tests, parser_base_stubs_retrieve_section)
{
    orcm_parser_active_module_t *act_module;

    act_module = OBJ_NEW(orcm_parser_active_module_t);

    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_sample_module;

    opal_list_append(&orcm_parser_base.actives, &act_module->super);

    //Random values to test. I am testing framework API. Not the plugin level
    orcm_parser.retrieve_section(1, NULL, "", "");
}

TEST_F(ut_parser_base_tests, parser_base_stubs_no_active_module_write_section)
{
    //Random values to test. I am testing framework API. Not the plugin level
    orcm_parser.write_section(NULL, 1, "");
}

TEST_F(ut_parser_base_tests, parser_base_stubs_write_section)
{
    orcm_parser_active_module_t *act_module;

    act_module = OBJ_NEW(orcm_parser_active_module_t);

    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_sample_module;

    opal_list_append(&orcm_parser_base.actives, &act_module->super);

    //Random values to test. I am testing framework API. Not the plugin level
    orcm_parser.write_section(NULL, 1, "");
}
