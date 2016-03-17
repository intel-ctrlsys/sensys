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

#define SAFE_OBJ_RELEASE(x)  if(NULL != x){ OBJ_RELEASE(x); x = NULL;}

#define OPEN_VALID_FILE(x)   int x = pugi_open(validFile.c_str()); \
                             ASSERT_LT(0, x);

using namespace std;

typedef pugi_impl parser_t;

void print_opal_list(opal_list_t* list);
bool allItemsInListHaveGivenKey(opal_list_t *list, char const *key);


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
    ASSERT_TRUE(p.loadFile());
}

TEST_F(ut_parser_pugi_tests, test_pugiImpl_loadFile_validFile)
{
    parser_t p(validFile);
    ASSERT_FALSE(p.loadFile());
}


/* Parser Pugi API Tests */


// Open and Close API Tests.

TEST_F(ut_parser_pugi_tests, test_API_open_invalidFile)
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
        SAFE_OBJ_RELEASE(list);
    }
    pugi_close(file_id);
}

TEST_F(ut_parser_pugi_tests, test_API_retrieveDocument_invalidFileId)
{
    opal_list_t *list = pugi_retrieve_document(ORCM_ERROR);
    EXPECT_TRUE(NULL == list);
    SAFE_OBJ_RELEASE(list);
}

// Retrieve Section API Tests.

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_invalidFileId_invalidKey_noName)
{
    opal_list_t *list = pugi_retrieve_section(ORCM_ERROR,"","");
    EXPECT_TRUE(NULL == list);
    SAFE_OBJ_RELEASE(list);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_invalidFileId_validKey_noName)
{
    opal_list_t *list = pugi_retrieve_section(ORCM_ERROR,validKey,"");
    EXPECT_TRUE(NULL == list);
    SAFE_OBJ_RELEASE(list);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_validFileId_invalidKey_noName)
{
    OPEN_VALID_FILE(file_id);
    opal_list_t *list = pugi_retrieve_section(file_id,invalidKey,"");
    EXPECT_TRUE(NULL == list);
    SAFE_OBJ_RELEASE(list);
    pugi_close(file_id);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_validFileId_validKey_noName)
{
    OPEN_VALID_FILE(file_id);
    opal_list_t *list = pugi_retrieve_section(file_id,validKey,"");
    EXPECT_TRUE(NULL != list);
    EXPECT_TRUE(allItemsInListHaveGivenKey(list,validKey));
    SAFE_OBJ_RELEASE(list);
    pugi_close(file_id);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_validFileId_invalidKey_invalidName)
{
    OPEN_VALID_FILE(file_id);
    opal_list_t *list = pugi_retrieve_section(file_id,invalidKey,invalidName);
    EXPECT_TRUE(NULL == list);
    SAFE_OBJ_RELEASE(list);
    pugi_close(file_id);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_validFileId_validKey_invalidName)
{
    OPEN_VALID_FILE(file_id);
    opal_list_t *list = pugi_retrieve_section(file_id,validKey,invalidName);
    EXPECT_TRUE(NULL == list);
    SAFE_OBJ_RELEASE(list);
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
    SAFE_OBJ_RELEASE(list);
    pugi_close(file_id);
}

// Retrieve Section From List API Tests.

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSectionFromList_invalidFileId)
{
    opal_list_t *list = pugi_retrieve_section_from_list(ORCM_ERROR, NULL, "", "");
    EXPECT_TRUE(NULL == list);
    SAFE_OBJ_RELEASE(list);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSectionFromList_validFileId_validKey_noName)
{
    opal_list_t *rootList, *groupList;
    int file_id = pugi_open(validFile.c_str());
    ASSERT_TRUE(0 < file_id);
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
        SAFE_OBJ_RELEASE(rootList);
        return;
    }
    EXPECT_TRUE(numOfValidKey == opal_list_get_size(groupList));
    EXPECT_FALSE(pugi_close(file_id));
    SAFE_OBJ_RELEASE(groupList);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSectionFromList_validFileId_validKey_validName_attribute)
{
    opal_list_t *rootList, *groupList;
    int file_id = pugi_open(validFile.c_str());
    ASSERT_TRUE(0 < file_id);
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
        SAFE_OBJ_RELEASE(rootList);
        return;
    }
    EXPECT_TRUE(1 == opal_list_get_size(groupList));
    EXPECT_FALSE(pugi_close(file_id));
    SAFE_OBJ_RELEASE(groupList);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSectionFromList_validFileId_validKey_validName_tag)
{
    opal_list_t *rootList, *groupList;
    int file_id = pugi_open(validFile.c_str());
    ASSERT_TRUE(0 < file_id);
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
        SAFE_OBJ_RELEASE(rootList);
        return;
    }
    EXPECT_TRUE(1 == opal_list_get_size(groupList));
    EXPECT_FALSE(pugi_close(file_id));
    SAFE_OBJ_RELEASE(groupList);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSectionFromList_validFileId_validKey_invalidName)
{
    opal_list_t *rootList, *groupList;
    int file_id = pugi_open(validFile.c_str());
    ASSERT_TRUE(0 < file_id);
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
    SAFE_OBJ_RELEASE(rootList);
    EXPECT_FALSE(pugi_close(file_id));
    SAFE_OBJ_RELEASE(groupList);
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
