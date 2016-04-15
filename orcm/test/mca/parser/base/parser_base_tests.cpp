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
#define RANDOM_VALUE_TO_TEST_API 1
#define RANDOM_FILE_NAME_TO_TEST_API "sample.xml"

// Fixture
using namespace std;

static void comp_init(void);
static void comp_finalize(void);
static int comp_open(char const *file);
static int comp_close(int file_id);
static opal_list_t* comp_retrieve_document(int file_id);
static opal_list_t* comp_retrieve_section(int file_id, char const* key,
                                          char const* name);
static opal_list_t* comp_retrieve_section_from_list(int file_id,
                                          opal_list_item_t *start,
                                          char const* key, char const* name);
static int comp_write_section(int file_id, opal_list_t *input, char const *key, char const *name, bool);

orcm_parser_base_module_t orcm_parser_sample_module = {
    comp_init,
    comp_finalize,
    comp_open,
    comp_close,
    comp_retrieve_document,
    comp_retrieve_section,
    comp_retrieve_section_from_list,
    comp_write_section
};

orcm_parser_base_module_t orcm_parser_sample_null_api_module = {
    NULL,
    NULL,
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

static opal_list_t* comp_retrieve_document(int file_id)
{
    return NULL;
}

static opal_list_t* comp_retrieve_section(int file_id, char const* key,
                                          char const* name)
{
    return NULL;
}

static opal_list_t* comp_retrieve_section_from_list(int file_id,
                                           opal_list_item_t *start,
                                           char const* key, char const* name)
{
    return NULL;
}

static int comp_write_section(int file_id, opal_list_t *input, char const *key, char const *name, bool overwrite)
{
    return MY_ORCM_SUCCESS;
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

void ut_parser_base_tests::reset_module_list()
{
    OPAL_LIST_DESTRUCT(&orcm_parser_base.actives);
    OBJ_CONSTRUCT(&orcm_parser_base.actives, opal_list_t);
}

void ut_parser_base_tests::reset_base_framework_comp()
{
    OPAL_LIST_DESTRUCT(&orcm_parser_base_framework.framework_components);
    OBJ_CONSTRUCT(&orcm_parser_base_framework.framework_components, opal_list_t);
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
    orcm_parser_base_component_t parser_component;
    mca_base_component_list_item_t *component;

    ut_parser_base_tests::reset_base_framework_comp();
    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) &parser_component;

    parser_component.base_version.mca_query_component = NULL;

    opal_list_append(&orcm_parser_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_parser_base_select();
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_select_query_component_with_NULL_module)
{
    int ret;
    orcm_parser_base_component_t parser_component;
    mca_base_component_list_item_t *component;

    ut_parser_base_tests::reset_base_framework_comp();
    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) &parser_component;

    parser_component.base_version.mca_query_component = component_query_null_module;

    opal_list_append(&orcm_parser_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_parser_base_select();
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_select_query_component_with_NULL_init_module)
{
    int ret;
    orcm_parser_base_component_t parser_component;
    mca_base_component_list_item_t *component;

    ut_parser_base_tests::reset_base_framework_comp();
    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) &parser_component;

    parser_component.base_version.mca_query_component = component_query_with_null_api_module;

    opal_list_append(&orcm_parser_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_parser_base_select();
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_select_query_component_with_one_module)
{
    int ret;
    orcm_parser_base_component_t parser_component;
    mca_base_component_list_item_t *component;

    ut_parser_base_tests::reset_base_framework_comp();
    component = OBJ_NEW(mca_base_component_list_item_t);
    component->cli_component = (mca_base_component_t *) &parser_component;

    parser_component.base_version.mca_query_component = component_query_with_module;

    opal_list_append(&orcm_parser_base_framework.framework_components,(opal_list_item_t *)component);

    ret = orcm_parser_base_select();
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_no_active_module_open_file)
{
    int ret;

    ut_parser_base_tests::reset_module_list();

    ret = orcm_parser.open(RANDOM_FILE_NAME_TO_TEST_API);
    ASSERT_NE(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_open_file)
{
    int ret;

    orcm_parser_active_module_t *act_module;

    ut_parser_base_tests::reset_module_list();

    act_module = OBJ_NEW(orcm_parser_active_module_t);

    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_sample_module;

    opal_list_append(&orcm_parser_base.actives, &act_module->super);

    ret = orcm_parser.open(RANDOM_FILE_NAME_TO_TEST_API);
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_no_active_module_close_file)
{
    int ret;

    ut_parser_base_tests::reset_module_list();

    ret = orcm_parser.close(RANDOM_VALUE_TO_TEST_API);
    ASSERT_NE(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_close_file)
{
    int ret;

    orcm_parser_active_module_t *act_module;

    act_module = OBJ_NEW(orcm_parser_active_module_t);

    ut_parser_base_tests::reset_module_list();

    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_sample_module;

    opal_list_append(&orcm_parser_base.actives, &act_module->super);

    ret = orcm_parser.close(RANDOM_VALUE_TO_TEST_API);
    ASSERT_EQ(MY_ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_no_active_module_retrieve_document)
{
    ut_parser_base_tests::reset_module_list();
    orcm_parser.retrieve_document(RANDOM_VALUE_TO_TEST_API);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_retrieve_document)
{
    orcm_parser_active_module_t *act_module;

    act_module = OBJ_NEW(orcm_parser_active_module_t);

    ut_parser_base_tests::reset_module_list();

    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_sample_module;

    opal_list_append(&orcm_parser_base.actives, &act_module->super);

    orcm_parser.retrieve_document(RANDOM_VALUE_TO_TEST_API);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_no_active_module_retrieve_section)
{
    ut_parser_base_tests::reset_module_list();
    orcm_parser.retrieve_section(RANDOM_VALUE_TO_TEST_API, "", "");
}

TEST_F(ut_parser_base_tests, parser_base_stubs_retrieve_section)
{
    orcm_parser_active_module_t *act_module;

    ut_parser_base_tests::reset_module_list();

    act_module = OBJ_NEW(orcm_parser_active_module_t);

    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_sample_module;

    opal_list_append(&orcm_parser_base.actives, &act_module->super);

    orcm_parser.retrieve_section(RANDOM_VALUE_TO_TEST_API, "", "");
}



TEST_F(ut_parser_base_tests,
       parser_base_stubs_no_active_module_retrieve_section_from_list)
{
    ut_parser_base_tests::reset_module_list();

    orcm_parser.retrieve_section_from_list(RANDOM_VALUE_TO_TEST_API, NULL, "", "");
}

TEST_F(ut_parser_base_tests, parser_base_stubs_retrieve_section_from_list)
{
    orcm_parser_active_module_t *act_module;

    ut_parser_base_tests::reset_module_list();

    act_module = OBJ_NEW(orcm_parser_active_module_t);

    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_sample_module;

    opal_list_append(&orcm_parser_base.actives, &act_module->super);

    orcm_parser.retrieve_section_from_list(RANDOM_VALUE_TO_TEST_API, NULL, "", "");
}

TEST_F(ut_parser_base_tests, parser_base_stubs_no_active_module_write_section)
{
    int rc;

    ut_parser_base_tests::reset_module_list();

    rc = orcm_parser.write_section(RANDOM_VALUE_TO_TEST_API, NULL, "", "", true);
    ASSERT_NE(MY_ORCM_SUCCESS, rc);
}

TEST_F(ut_parser_base_tests, parser_base_stubs_write_section)
{
    int rc;
    orcm_parser_active_module_t *act_module;

    ut_parser_base_tests::reset_module_list();

    act_module = OBJ_NEW(orcm_parser_active_module_t);

    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_sample_module;

    opal_list_append(&orcm_parser_base.actives, &act_module->super);

    rc = orcm_parser.write_section(RANDOM_VALUE_TO_TEST_API, NULL, "", "", true);
    ASSERT_EQ(MY_ORCM_SUCCESS, rc);
}
