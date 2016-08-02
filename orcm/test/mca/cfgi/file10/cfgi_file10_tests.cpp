/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include "cfgi_file10_tests.h"
#include "orcm/mca/cfgi/base/base.h"

#define SAFEFREE(x) if(NULL != x) {free(x); x = NULL;}

char* ut_cfgi_file10_tests::MockedStrdup(const char *s)
{
    return NULL;
}

int ut_cfgi_file10_tests::ParserOpen (char const *file)
{
    return 0;
}

int ut_cfgi_file10_tests::ParserClose (int file_id)
{
    return 0;
}

opal_list_t* ut_cfgi_file10_tests::ParserRetrieveDocument (int file_id)
{
    opal_list_t *list = OBJ_NEW(opal_list_t);
    if (NULL == list) {
        return NULL;
    }
    orcm_value_t *item = OBJ_NEW(orcm_value_t);
    if (NULL == item) {
        return NULL;
    }
    item->value.type = OPAL_PTR;

    opal_list_append(list, (opal_list_item_t *) item);
    return list;
}

void ut_cfgi_file10_tests::SetUpTestCase()
{
    cfgi_file10_mocking_object.orcm_parser_base_open_file_callback =  ParserOpen;
    cfgi_file10_mocking_object.orcm_parser_base_close_file_callback = ParserClose;
    cfgi_file10_mocking_object.orcm_parser_base_retrieve_document_callback = ParserRetrieveDocument;
}

void ut_cfgi_file10_tests::TearDownTestCase()
{

}

char* ut_cfgi_file10_tests::CopyConfigFileName(char *config_file)
{
    char *old_config;

    if (NULL == config_file) {
        return NULL;
    }

    old_config = strdup(config_file);
    if (NULL == old_config) {
        return NULL;
    }

    SAFEFREE(config_file);

    return old_config;

}

TEST_F(ut_cfgi_file10_tests, cfgi_file10_sample_tests)
{
    int rc = -1;
    char *old_config = NULL;

    old_config = CopyConfigFileName(orcm_cfgi_base.config_file);

    orcm_cfgi_base.config_file = strdup("/tmp/data.xml");
    if (NULL == old_config) {
        return;
    }

    cfgi_file10_mocking_object.strdup_callback = MockedStrdup;
    rc = orcm_cfgi_file10_module.read_config(NULL);
    cfgi_file10_mocking_object.strdup_callback = NULL;

    orcm_cfgi_base.config_file = CopyConfigFileName(old_config);

    ASSERT_NE(ORCM_SUCCESS, rc);

}
