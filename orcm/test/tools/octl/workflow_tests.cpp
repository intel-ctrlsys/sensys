/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */



#include <iostream>

#include "workflow_tests.h"


using namespace std;

#define MY_ORCM_SUCCESS 0

orcm_parser_module_open_fn_t ut_octl_workflow_tests::saved_open_fn_t = NULL;
orcm_parser_module_retrieve_section_fn_t ut_octl_workflow_tests::saved_retrieve_section_fnt_t = NULL;
orcm_parser_module_close_fn_t ut_octl_workflow_tests::saved_close_fn_t = NULL;

opal_list_t* C_NULLRetrieveSection(int file_id, char const* key, char const* name)
{
    return NULL;

}

opal_list_t* C_RetrieveSection(int file_id, char const* key, char const* name)
{
    opal_list_t *result_list = OBJ_NEW(opal_list_t);
    opal_buffer_t *buf = OBJ_NEW(opal_buffer_t);
    orcm_value_t *value;
    int workflow_value = 10;
    char *workflow_key = strdup("workflows");

    value = orcm_util_load_orcm_value(workflow_key, &workflow_value, OPAL_INT, NULL);

    opal_list_append(result_list, (opal_list_item_t *)value);
    return result_list;

}

opal_list_t* WorkflowsNULL(int file_id, char const* key, char const* name)
{
    opal_list_t *result_list = OBJ_NEW(opal_list_t);
    opal_buffer_t *buf = OBJ_NEW(opal_buffer_t);
    orcm_value_t *value;
    int *workflow_value = NULL;
    char *workflow_key = strdup("workflows");

    value = orcm_util_load_orcm_value(workflow_key, workflow_value, OPAL_PTR, NULL);

    opal_list_append(result_list, (opal_list_item_t *)value);
    return result_list;

}

opal_list_t* WorkflowsIntType(int file_id, char const* key, char const* name)
{
    opal_list_t *result_list = OBJ_NEW(opal_list_t);
    opal_buffer_t *buf = OBJ_NEW(opal_buffer_t);
    orcm_value_t *value;
    orcm_value_t *value1;
    opal_list_t *workflows_value = OBJ_NEW(opal_list_t);
    char *workflows_key = strdup("workflows");
    char *dummy_key = strdup("dummy");
    int dummy_value = 10;

    value1 = orcm_util_load_orcm_value(dummy_key, &dummy_value, OPAL_INT, NULL);

    opal_list_append(workflows_value, (opal_list_item_t *)value1);

    value = orcm_util_load_orcm_value(workflows_key, workflows_value, OPAL_PTR, NULL);

    opal_list_append(result_list, (opal_list_item_t *)value);
    return result_list;

}

opal_list_t* WorkflowsPtrType(int file_id, char const* key, char const* name)
{
    opal_list_t *result_list = OBJ_NEW(opal_list_t);
    opal_buffer_t *buf = OBJ_NEW(opal_buffer_t);
    orcm_value_t *value;
    orcm_value_t *value1;
    orcm_value_t *value2;
    opal_list_t *workflows_value = OBJ_NEW(opal_list_t);
    char *workflows_key = strdup("workflows");
    char *dummy_key = strdup("dummy");
    int dummy_value = 10;

    opal_list_t *workflow_value = OBJ_NEW(opal_list_t);
    char *workflow_key = strdup("workflow");


    value = orcm_util_load_orcm_value(dummy_key, &dummy_value, OPAL_INT, NULL);

    opal_list_append(workflow_value, (opal_list_item_t *)value);

    value1 = orcm_util_load_orcm_value(workflow_key, workflow_value, OPAL_PTR, NULL);

    opal_list_append(workflows_value, (opal_list_item_t *)value1);

    value2 = orcm_util_load_orcm_value(workflows_key, workflows_value, OPAL_PTR, NULL);

    opal_list_append(result_list, (opal_list_item_t *)value2);
    return result_list;

}

void ut_octl_workflow_tests::SetUpTestCase()
{
    saved_open_fn_t = orcm_parser.open;
    saved_retrieve_section_fnt_t = orcm_parser.retrieve_section;
    saved_close_fn_t = orcm_parser.close;
}

void ut_octl_workflow_tests::TearDownTestCase()
{
    orcm_parser.open = saved_open_fn_t;
    orcm_parser.retrieve_section = saved_retrieve_section_fnt_t;
    orcm_parser.close = saved_close_fn_t;

}

int ut_octl_workflow_tests::NegOpenFile(char const *file)
{
    return -1;

}

int ut_octl_workflow_tests::OpenFile(char const *file)
{
    return 0;

}

int ut_octl_workflow_tests::CloseFile(int file_id)
{
    return 0;

}

TEST_F(ut_octl_workflow_tests, workflow_add_negative)
{
    const char* cmdlist[3] = {
        "workflow",
        "add",
        NULL
    };
    int rv = orcm_octl_workflow_add((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_workflow_tests, workflow_add_negative1)
{
    const char* cmdlist[2] = {
        "workflow",
        "add",
    };
    int rv = orcm_octl_workflow_add((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_workflow_tests, workflow_add_negative2)
{
    const char* cmdlist[4] = {
        "workflow",
        "add",
        "workflow.xml",
        "master"
    };

    orcm_parser.open = OpenFile;
    orcm_parser.retrieve_section = C_RetrieveSection;
    orcm_parser.close = CloseFile;

    int rv = orcm_octl_workflow_add((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_workflow_tests, workflow_add_negative3)
{
    const char* cmdlist[4] = {
        "workflow",
        "add",
        "workflow.xml",
        "master"
    };

    orcm_parser.open = OpenFile;
    orcm_parser.retrieve_section = WorkflowsNULL;
    orcm_parser.close = CloseFile;

    int rv = orcm_octl_workflow_add((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_workflow_tests, workflow_add_negative4)
{
    const char* cmdlist[4] = {
        "workflow",
        "add",
        "workflow.xml",
        "master"
    };

    orcm_parser.open = OpenFile;
    orcm_parser.retrieve_section = WorkflowsIntType;
    orcm_parser.close = CloseFile;

    int rv = orcm_octl_workflow_add((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_workflow_tests, workflow_add_negative5)
{
    const char* cmdlist[4] = {
        "workflow",
        "add",
        "workflow.xml",
        "master"
    };

    orcm_parser.open = OpenFile;
    orcm_parser.retrieve_section = WorkflowsPtrType;
    orcm_parser.close = CloseFile;

    int rv = orcm_octl_workflow_add((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}


TEST_F(ut_octl_workflow_tests, retrieve_workflow_open_negative){
    opal_list_t *result_list = NULL;

    orcm_parser.open = NegOpenFile;
    orcm_parser.retrieve_section = C_NULLRetrieveSection;
    orcm_parser.close = CloseFile;

    ASSERT_EQ(orcm_util_workflow_add_retrieve_workflows_section("test.xml"), (opal_list_t *)NULL);

}

TEST_F(ut_octl_workflow_tests, retrieve_workflow_retrieve_negative){
    opal_list_t *result_list = NULL;

    orcm_parser.open = OpenFile;
    orcm_parser.retrieve_section = C_NULLRetrieveSection;
    orcm_parser.close = CloseFile;

    ASSERT_EQ(orcm_util_workflow_add_retrieve_workflows_section("test.xml"), (opal_list_t *)NULL);

}

TEST_F(ut_octl_workflow_tests, extract_workflow_info_negative){
    int rc;

    rc = orcm_util_workflow_add_extract_workflow_info(NULL, NULL, NULL, NULL);

    ASSERT_NE(rc, MY_ORCM_SUCCESS);
}

TEST_F(ut_octl_workflow_tests, extract_workflow_info_negative1){
    int rc;
    opal_list_t *result_list = OBJ_NEW(opal_list_t);

    rc = orcm_util_workflow_add_extract_workflow_info(result_list, NULL, NULL, NULL);

    ASSERT_NE(rc, MY_ORCM_SUCCESS);

}

TEST_F(ut_octl_workflow_tests, extract_workflow_info_negative2){
    int rc;
    opal_list_t *result_list = OBJ_NEW(opal_list_t);
    opal_buffer_t *buf = OBJ_NEW(opal_buffer_t);

    rc = orcm_util_workflow_add_extract_workflow_info(result_list, buf, NULL, NULL);

    OBJ_RELEASE(result_list);

    ASSERT_NE(rc, MY_ORCM_SUCCESS);
}

TEST_F(ut_octl_workflow_tests, extract_workflow_info_negative3){
    opal_list_t *result_list = OBJ_NEW(opal_list_t);
    opal_buffer_t *buf = OBJ_NEW(opal_buffer_t);
    orcm_value_t *value;
    orcm_value_t *value1;
    char *dummy_key = strdup("name");
    int dummy_value = 10;
    int rc;

    value = orcm_util_load_orcm_value(dummy_key, &dummy_value, OPAL_INT, NULL);

    value1 = orcm_util_load_orcm_value(dummy_key, &dummy_value, OPAL_INT, NULL);

    opal_list_append(result_list, (opal_list_item_t *)value);
    opal_list_append(result_list, (opal_list_item_t *)value1);

    rc = orcm_util_workflow_add_extract_workflow_info(result_list, buf, NULL, NULL);

    OBJ_RELEASE(result_list);

    ASSERT_NE(rc, MY_ORCM_SUCCESS);
}
