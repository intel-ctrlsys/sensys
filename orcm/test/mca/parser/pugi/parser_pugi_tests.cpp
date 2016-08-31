/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "parser_pugi_tests.h"
#include "opal/class/opal_list.h"
#include "orcm/runtime/orcm_globals.h"
#include "parser_pugi_mocking.h"

BEGIN_C_DECLS
    #include "orcm/util/utils.h"
    extern void orcm_util_release_nested_orcm_value_list_item(orcm_value_t **item);
    extern void orcm_util_release_nested_orcm_value_list(opal_list_t *list);
    extern orcm_value_t* orcm_util_load_orcm_value(char *key, void *data, opal_data_type_t type, char *units);
END_C_DECLS

#define OPEN_VALID_FILE(x)   int x = pugi_open(validFile.c_str()); \
                             ASSERT_LT(0, x);

using namespace std;

typedef pugi_impl parser_t;
extern bool is_orcm_util_append_expected_succeed;
extern bool is_strdup_expected_succeed;

void print_opal_list(opal_list_t* list);
bool allItemsInListHaveGivenKey(opal_list_t *list, char const *key);
void parser_pugi_load_orcm_value (opal_list_t *input, char *key, void *data, opal_data_type_t type);

/* Pugi_Impl Tests */


TEST_F(ut_parser_pugi_tests, test_pugiImpl_createParser_emptyConstructor)
{
    parser_t p;
    parser_t *ptr = new parser_t();
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}

TEST_F(ut_parser_pugi_tests, test_pugiImpl_createParser_emptyStringConstructor)
{
    parser_t p(string(""));
    parser_t *ptr = new parser_t(string(""));
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}

TEST_F(ut_parser_pugi_tests, test_pugiImpl_createParser_emptyCharptrConstructor)
{
    parser_t p("");
    parser_t *ptr = new parser_t("");
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}

TEST_F(ut_parser_pugi_tests, test_pugiImpl_createParser_stringConstructor)
{
    parser_t p(string(invalidFile));
    parser_t *ptr = new parser_t(string(invalidFile));
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}

TEST_F(ut_parser_pugi_tests, test_pugiImpl_createParser_charptrConstructor)
{
    parser_t p(invalidFile);
    parser_t *ptr = new parser_t(invalidFile);
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}

TEST_F(ut_parser_pugi_tests, test_pugiImpl_loadFile_invalidFile)
{
    parser_t p(invalidFile);
    ASSERT_THROW(p.loadFile(), unableToOpenFile);
}

TEST_F(ut_parser_pugi_tests, test_pugiImpl_loadFile_validFile)
{
    parser_t p(validFile);
    ASSERT_FALSE(p.loadFile());
}


/* Parser Pugi API Tests */

// Finalize Function Tests.

TEST_F(ut_parser_pugi_tests, test_API_finalize)
{
   OPEN_VALID_FILE(file_id);
   EXPECT_EQ(++openedFiles, file_id);
   pugi_finalize();
   openedFiles=0;
   file_id = pugi_open(validFile.c_str());
   EXPECT_EQ(++openedFiles, file_id);
   pugi_close(file_id);
}

// Open and Close API Tests.

TEST_F(ut_parser_pugi_tests, test_API_open_invalidFilebool)
{
    int file_id = pugi_open(invalidFile);
    ASSERT_EQ(ORCM_ERR_FILE_OPEN_FAILURE,file_id);
}

TEST_F(ut_parser_pugi_tests, test_API_close_invalidFileId)
{
    int ret = pugi_close(ORCM_ERROR);
    ASSERT_EQ(ORCM_ERROR,ret);
}

TEST_F(ut_parser_pugi_tests, test_API_openAndClose_validFile)
{
    OPEN_VALID_FILE(file_id);
    EXPECT_EQ(++openedFiles, file_id);
    ASSERT_EQ(ORCM_SUCCESS,pugi_close(file_id));
}

TEST_F(ut_parser_pugi_tests, test_API_openTwice_validFile)
{
    OPEN_VALID_FILE(file_id_1);
    OPEN_VALID_FILE(file_id_2);
    EXPECT_EQ(++openedFiles, file_id_1);
    EXPECT_EQ(++openedFiles, file_id_2);
    EXPECT_EQ(ORCM_SUCCESS, pugi_close(file_id_1));
    EXPECT_EQ(ORCM_SUCCESS, pugi_close(file_id_2));
}

TEST_F(ut_parser_pugi_tests, test_API_closeTwice_validFile)
{
    OPEN_VALID_FILE(file_id);
    ASSERT_EQ(++openedFiles, file_id);
    EXPECT_EQ(ORCM_SUCCESS, pugi_close(file_id));
    EXPECT_EQ(ORCM_ERROR, pugi_close(file_id));
}

// Retrieve Document API Tests.

TEST_F(ut_parser_pugi_tests, test_API_retrieveDocument_validFileId)
{
    opal_list_t *list;
    orcm_value_t *root;
    OPEN_VALID_FILE(file_id);
    list = pugi_retrieve_document(file_id);
    EXPECT_TRUE(NULL != list);
    if (list){
        EXPECT_EQ(1, opal_list_get_size(list));
        root = (orcm_value_t*) opal_list_get_first(list);
        EXPECT_STREQ(rootKey, root->value.key);
        EXPECT_EQ(OPAL_PTR, root->value.type);
        if (OPAL_PTR == root->value.type){
            EXPECT_TRUE(NULL != root->value.data.ptr);
        }
        SAFE_RELEASE_NESTED_LIST(list);
    }
    pugi_close(file_id);
}

TEST_F(ut_parser_pugi_tests, test_API_retrieveDocument_invalidFileId)
{
    opal_list_t *list = pugi_retrieve_document(ORCM_ERROR);
    EXPECT_TRUE(NULL == list);
    SAFE_RELEASE_NESTED_LIST(list);
}

// Retrieve Section API Tests.

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_invalidFileId_invalidKey_noName)
{
    opal_list_t *list = pugi_retrieve_section(ORCM_ERROR,"","");
    EXPECT_TRUE(NULL == list);
    SAFE_RELEASE_NESTED_LIST(list);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_invalidFileId_validKey_noName)
{
    opal_list_t *list = pugi_retrieve_section(ORCM_ERROR,validKey,"");
    EXPECT_TRUE(NULL == list);
    SAFE_RELEASE_NESTED_LIST(list);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_validFileId_invalidKey_noName)
{
    OPEN_VALID_FILE(file_id);
    opal_list_t *list = pugi_retrieve_section(file_id,invalidKey,"");
    EXPECT_TRUE(NULL == list);
    SAFE_RELEASE_NESTED_LIST(list);
    pugi_close(file_id);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_validFileId_validKey_noName)
{
    OPEN_VALID_FILE(file_id);
    opal_list_t *list = pugi_retrieve_section(file_id,validKey,"");
    EXPECT_TRUE(NULL != list);
    EXPECT_TRUE(allItemsInListHaveGivenKey(list,validKey));
    SAFE_RELEASE_NESTED_LIST(list);
    pugi_close(file_id);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_validFileId_invalidKey_invalidName)
{
    OPEN_VALID_FILE(file_id);
    opal_list_t *list = pugi_retrieve_section(file_id,invalidKey,invalidName);
    EXPECT_TRUE(NULL == list);
    SAFE_RELEASE_NESTED_LIST(list);
    pugi_close(file_id);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_validFileId_validKey_invalidName)
{
    OPEN_VALID_FILE(file_id);
    opal_list_t *list = pugi_retrieve_section(file_id,validKey,invalidName);
    EXPECT_TRUE(NULL == list);
    SAFE_RELEASE_NESTED_LIST(list);
    pugi_close(file_id);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_validFileId_validKey_validName)
{
    OPEN_VALID_FILE(file_id);
    opal_list_t *list = pugi_retrieve_section(file_id,"tagname","tag_name2");
    EXPECT_TRUE(NULL != list);
    if (list){
        EXPECT_EQ(1,opal_list_get_size(list));
        orcm_value_t *name = NULL;
        orcm_value_t *item = (orcm_value_t*)opal_list_get_first(list);
        EXPECT_STREQ("tagname", item->value.key);
        if (OPAL_PTR == item->value.type && NULL != item->value.data.ptr){
            name = (orcm_value_t*)opal_list_get_first((opal_list_t*)
                                                      item->value.data.ptr);
            if (OPAL_STRING != name->value.type){
                name = NULL;
            }
        }
        EXPECT_TRUE(NULL != name && 0 == strcmp("tag_name2", name->value.data.string));
    }
    SAFE_RELEASE_NESTED_LIST(list);
    pugi_close(file_id);
}

// Retrieve Section From List API Tests.

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSectionFromList_invalidFileId)
{
    opal_list_t *list = pugi_retrieve_section_from_list(ORCM_ERROR, NULL, "", "");
    EXPECT_TRUE(NULL == list);
    SAFE_RELEASE_NESTED_LIST(list);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSectionFromList_validFileId_validKey_noName)
{
    opal_list_t *rootList, *groupList;
    OPEN_VALID_FILE(file_id);
    rootList = pugi_retrieve_document(file_id);
    EXPECT_TRUE(NULL != rootList);
    if (NULL == rootList) {
        pugi_close(file_id);
        return;
    }
    EXPECT_TRUE(1 == opal_list_get_size(rootList));
    groupList = pugi_retrieve_section_from_list(file_id, opal_list_get_first(rootList),
                                      validKey,"");
    EXPECT_TRUE(NULL != groupList);
    if (NULL == groupList) {
        pugi_close(file_id);
        SAFE_RELEASE_NESTED_LIST(rootList);
        return;
    }
    EXPECT_TRUE(numOfValidKey == opal_list_get_size(groupList));
    EXPECT_FALSE(pugi_close(file_id));
    SAFE_RELEASE_NESTED_LIST(groupList);
    SAFE_RELEASE_NESTED_LIST(rootList);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSectionFromList_validFileId_validKey_validName_attribute)
{
    opal_list_t *rootList, *groupList;
    OPEN_VALID_FILE(file_id);
    rootList = pugi_retrieve_document(file_id);
    EXPECT_TRUE(NULL != rootList);
    if (NULL == rootList) {
        pugi_close(file_id);
        return;
    }
    EXPECT_TRUE(1 == opal_list_get_size(rootList));
    groupList = pugi_retrieve_section_from_list(file_id, opal_list_get_first(rootList),
                                      validKey,validName);
    EXPECT_TRUE(NULL != groupList);
    if (NULL == groupList) {
        pugi_close(file_id);
        SAFE_RELEASE_NESTED_LIST(rootList);
        return;
    }
    EXPECT_TRUE(1 == opal_list_get_size(groupList));
    EXPECT_FALSE(pugi_close(file_id));
    SAFE_RELEASE_NESTED_LIST(groupList);
    SAFE_RELEASE_NESTED_LIST(rootList);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSectionFromList_validFileId_validKey_validName_tag)
{
    opal_list_t *rootList, *groupList;
    OPEN_VALID_FILE(file_id);
    rootList = pugi_retrieve_document(file_id);
    EXPECT_TRUE(NULL != rootList);
    if (NULL == rootList) {
        pugi_close(file_id);
        return;
    }
    EXPECT_TRUE(1 == opal_list_get_size(rootList));
    groupList = pugi_retrieve_section_from_list(file_id, opal_list_get_first(rootList),
                                      validKey,validNameTag);
    EXPECT_TRUE(NULL != groupList);
    if (NULL == groupList) {
        pugi_close(file_id);
        SAFE_RELEASE_NESTED_LIST(rootList);
        return;
    }
    EXPECT_TRUE(1 == opal_list_get_size(groupList));
    EXPECT_FALSE(pugi_close(file_id));
    SAFE_RELEASE_NESTED_LIST(groupList);
    SAFE_RELEASE_NESTED_LIST(rootList);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSectionFromList_validFileId_validKey_invalidName)
{
    opal_list_t *rootList, *groupList;
    OPEN_VALID_FILE(file_id);
    rootList = pugi_retrieve_document(file_id);
    EXPECT_TRUE(NULL != rootList);
    if (NULL == rootList) {
        pugi_close(file_id);
        return;
    }
    EXPECT_TRUE(1 == opal_list_get_size(rootList));
    groupList = pugi_retrieve_section_from_list(file_id, opal_list_get_first(rootList),
                                      validKey,invalidName);
    EXPECT_TRUE(NULL == groupList);
    EXPECT_FALSE(pugi_close(file_id));
    SAFE_RELEASE_NESTED_LIST(groupList);
    SAFE_RELEASE_NESTED_LIST(rootList);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_invalidFile)
{
    int ret = pugi_write_section(-1, NULL, NULL, NULL, false);
    ASSERT_EQ(ORCM_ERR_FILE_OPEN_FAILURE, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_nullInput_overwrite_false)
{
    int file_id = pugi_open(writeFile.c_str());
    int ret = pugi_write_section(file_id, NULL, NULL, NULL, false);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_oneNodeoneAttributeNonRoot)
{
    FILE *xml_file;

    xml_file = fopen("/tmp/write_xml", "w+");
    fprintf(xml_file, "<group>value</group>\n");
    fclose(xml_file);

    int file_id = pugi_open("/tmp/write_xml");
    char *key = strdup("nonroot");
    opal_list_t *input = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (input, strdup("group"), strdup("value"), OPAL_STRING);

    int ret = pugi_write_section(file_id, input, key, NULL, false);

    SAFEFREE(key);
    pugi_close(file_id);
    unlink("/tmp/write_xml");
    ASSERT_NE(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_oneNodeMockingUtilAppendNonRoot)
{
    FILE *xml_file;

    xml_file = fopen("/tmp/write_xml", "w+");
    fprintf(xml_file, "<group>\n</group>\n");
    fclose(xml_file);

    int file_id = pugi_open("/tmp/write_xml");
    char *key = strdup("nonroot");
    opal_list_t *input = OBJ_NEW(opal_list_t);

    is_orcm_util_append_expected_succeed = false;

    parser_pugi_load_orcm_value (input, strdup("group"), strdup("value"), OPAL_STRING);

    int ret = pugi_write_section(file_id, input, key, NULL, false);

    SAFEFREE(key);
    pugi_close(file_id);
    unlink("/tmp/write_xml");
    is_orcm_util_append_expected_succeed = true;
    ASSERT_NE(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_oneNodeAtRoot)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("root");
    opal_list_t *input = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (input, strdup("group"), strdup("value"), OPAL_STRING);

    int ret = pugi_write_section(file_id, input, key, NULL, false);

    SAFEFREE(key);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_oneNodeAtNonRoot)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("nonroot");
    opal_list_t *input = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (input, strdup("group"), strdup("value"), OPAL_STRING);

    int ret = pugi_write_section(file_id, input, key, NULL, false);

    SAFEFREE(key);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_oneNodeWithEmptyStringAtNonRoot)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("nonroot");
    opal_list_t *input = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (input, strdup("group"), strdup("value"), OPAL_STRING);

    int ret = pugi_write_section(file_id, input, key, "", false);

    SAFEFREE(key);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_oneNode_withNameAtNonRoot)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("nonroot");
    opal_list_t *input = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (input, strdup("group"), strdup("value"), OPAL_STRING);

    int ret = pugi_write_section(file_id, input, key, "name", false);

    SAFEFREE(key);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_twoNodeHirearchyAtRoot)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("root");

    opal_list_t *input = OBJ_NEW(opal_list_t);
    opal_list_t *inner_list = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (input, strdup("group"), inner_list, OPAL_PTR);

    int ret = pugi_write_section(file_id, input, key, NULL, false);

    SAFEFREE(key);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_twoNodeHirearchyWithNameAttrAtRoot)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("root");

    opal_list_t *input = OBJ_NEW(opal_list_t);
    opal_list_t *inner_list = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (input, strdup("group"), inner_list, OPAL_PTR);

    int ret = pugi_write_section(file_id, input, key, NULL, false);

    SAFEFREE(key);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_twoNodeHirearchyWithIntAtRoot)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("root");

    opal_list_t *input = OBJ_NEW(opal_list_t);
    opal_list_t *inner_list = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    int int_value = 20;

    parser_pugi_load_orcm_value (inner_list, strdup("int_value"), &int_value, OPAL_INT);

    parser_pugi_load_orcm_value (input, strdup("group"), inner_list, OPAL_PTR);

    int ret = pugi_write_section(file_id, input, key, NULL, false);

    SAFEFREE(key);
    pugi_close(file_id);
    ASSERT_NE(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_threeNodeHirearchyWithNameAttrAtRoot)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("root");

    opal_list_t *input = OBJ_NEW(opal_list_t);
    opal_list_t *inner_list = OBJ_NEW(opal_list_t);
    opal_list_t *third_inner_list = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (third_inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (third_inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("thirdgroup"), third_inner_list, OPAL_PTR);


    parser_pugi_load_orcm_value (inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (input, strdup("group"), inner_list, OPAL_PTR);

    int ret = pugi_write_section(file_id, input, key, NULL, false);

    SAFEFREE(key);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_threeNodeHirearchyWithNameAttrAtNULL)
{
    int file_id = pugi_open(writeFile.c_str());

    opal_list_t *input = OBJ_NEW(opal_list_t);
    opal_list_t *inner_list = OBJ_NEW(opal_list_t);
    opal_list_t *third_inner_list = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (third_inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (third_inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("thirdgroup"), third_inner_list, OPAL_PTR);


    parser_pugi_load_orcm_value (inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (input, strdup("group"), inner_list, OPAL_PTR);

    int ret = pugi_write_section(file_id, input, NULL, NULL, false);

    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_oneNodeWithNameAttrAtNULL)
{
    int file_id = pugi_open(writeFile.c_str());

    opal_list_t *input = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (input, strdup("group"), NULL, OPAL_PTR);

    int ret = pugi_write_section(file_id, input, NULL, NULL, false);

    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_threeNodeHirearchyWithNameAttrAtInnerLoop)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("innerloop");

    opal_list_t *input = OBJ_NEW(opal_list_t);
    opal_list_t *inner_list = OBJ_NEW(opal_list_t);
    opal_list_t *third_inner_list = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (third_inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (third_inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("thirdgroup"), third_inner_list, OPAL_PTR);


    parser_pugi_load_orcm_value (inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (input, strdup("group"), inner_list, OPAL_PTR);

    int ret = pugi_write_section(file_id, input, key, NULL, false);

    SAFEFREE(key);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_threeNodeHirearchyWithNameAttrAtInnerInnerLoop)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("innerinnerloop");

    opal_list_t *input = OBJ_NEW(opal_list_t);
    opal_list_t *inner_list = OBJ_NEW(opal_list_t);
    opal_list_t *third_inner_list = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (third_inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (third_inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("thirdgroup"), third_inner_list, OPAL_PTR);


    parser_pugi_load_orcm_value (inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (input, strdup("group"), inner_list, OPAL_PTR);

    int ret = pugi_write_section(file_id, input, key, NULL, false);

    SAFEFREE(key);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_threeNodeHirearchyWithNameAttrAtInnerInnerNamedLoop)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("namedloop");
    char *name = strdup("dummy");

    opal_list_t *input = OBJ_NEW(opal_list_t);
    opal_list_t *inner_list = OBJ_NEW(opal_list_t);
    opal_list_t *third_inner_list = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (third_inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (third_inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("thirdgroup"), third_inner_list, OPAL_PTR);


    parser_pugi_load_orcm_value (inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (input, strdup("group"), inner_list, OPAL_PTR);

    int ret = pugi_write_section(file_id, input, key, name, false);

    SAFEFREE(key);
    SAFEFREE(name);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_threeNodeHirearchyWithNameAttrOverWriteSetAtRoot)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("overwriteloop");

    opal_list_t *input = OBJ_NEW(opal_list_t);
    opal_list_t *inner_list = OBJ_NEW(opal_list_t);
    opal_list_t *third_inner_list = OBJ_NEW(opal_list_t);

    parser_pugi_load_orcm_value (third_inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (third_inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("thirdgroup"), third_inner_list, OPAL_PTR);


    parser_pugi_load_orcm_value (inner_list, strdup("members"), strdup("master"), OPAL_STRING);

    parser_pugi_load_orcm_value (inner_list, strdup("name"), strdup("aggregators"), OPAL_STRING);

    parser_pugi_load_orcm_value (input, strdup("group"), inner_list, OPAL_PTR);

    int ret = pugi_write_section(file_id, input, key, NULL, true);

    SAFEFREE(key);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_parser_pugi_tests, test_API_writeSection_OverWriteSetAtRoot)
{
    int file_id = pugi_open(writeFile.c_str());
    char *key = strdup("overwriteloop");

    int ret = pugi_write_section(file_id, NULL, key, NULL, true);
    SAFEFREE(key);
    pugi_close(file_id);
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

/* Utilities */


void print_opal_list (opal_list_t *root)
{
    if (NULL == root){
        return;
    }
    orcm_value_t *ptr;
    OPAL_LIST_FOREACH(ptr,root,orcm_value_t){
        cout << endl<< "item: " << ptr->value.key;
        if (ptr->value.type == OPAL_STRING){
            cout << " , value: " << ptr->value.data.string << endl;
        } else {
            cout << endl << "children: ";
            print_opal_list((opal_list_t*)ptr->value.data.ptr);
            cout << " end-list" << endl;
        }
    }
}

bool allItemsInListHaveGivenKey(opal_list_t *list, char const *key){
    if (NULL == list){
        return false;
    }
    orcm_value_t *item;
    OPAL_LIST_FOREACH(item,list,orcm_value_t){
        if (0 != strcmp(key, item->value.key)){
            return false;
        }
    }
    return true;
}

void parser_pugi_load_orcm_value (opal_list_t *input, char *key, void *data, opal_data_type_t type)
{

    orcm_value_t *group = OBJ_NEW(orcm_value_t);
    group = orcm_util_load_orcm_value(key, data, type, NULL);
    opal_list_append(input, (opal_list_item_t *)group);
}
