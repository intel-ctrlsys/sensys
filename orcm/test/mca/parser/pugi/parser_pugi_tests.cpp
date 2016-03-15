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

#define SAFE_OBJ_RELEASE(x) if(NULL != x){ OBJ_RELEASE(x); x = NULL;}
#define TEST_RETRIEVE_SECTION_EQ(expected_value,listptr,start,key,name) \
    int file_id = pugi_open(validFile);                  \
    ASSERT_TRUE(0 < file_id);                            \
    listptr = pugi_retrieve_section(file_id,start,key,name); \
    EXPECT_TRUE(expected_value == listptr); \
    EXPECT_FALSE(pugi_close(file_id));
#define TEST_RETRIEVE_SECTION_NE(expected_value,listptr,start,key,name) \
    int file_id = pugi_open(validFile);                  \
    ASSERT_TRUE(0 < file_id);                            \
    listptr = pugi_retrieve_section(file_id,start,key,name); \
    EXPECT_TRUE(expected_value != listptr); \
    EXPECT_FALSE(pugi_close(file_id));

using namespace std;

typedef pugi_impl parser_t;

void print_opal_list(opal_list_t* list);

TEST_F(ut_parser_pugi_tests, test_createParser_emptyConstructor)
{
    parser_t p;
    parser_t *ptr = new parser_t();
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}


TEST_F(ut_parser_pugi_tests, test_createParser_emptyStringConstructor)
{
    parser_t p(string(""));
    parser_t *ptr = new parser_t(string(""));
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}

TEST_F(ut_parser_pugi_tests, test_createParser_emptyCharptrConstructor)
{
    parser_t p("");
    parser_t *ptr = new parser_t("");
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}


TEST_F(ut_parser_pugi_tests, test_createParser_stringConstructor)
{
    parser_t p(string(invalidFile));
    parser_t *ptr = new parser_t(string(invalidFile));
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}


TEST_F(ut_parser_pugi_tests, test_createParser_charptrConstructor)
{
    parser_t p(invalidFile);
    parser_t *ptr = new parser_t(invalidFile);
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}

TEST_F(ut_parser_pugi_tests, test_loadFile_invalidFile)
{
    parser_t p(invalidFile);
    ASSERT_TRUE(p.loadFile());
}

TEST_F(ut_parser_pugi_tests, test_loadFile_validFile)
{
    parser_t p(validFile);
    ASSERT_FALSE(p.loadFile());
}

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
    int file_id = pugi_open(validFile);
    ASSERT_LT(0, file_id);
    EXPECT_EQ(++openedFiles, file_id);
    ASSERT_EQ(ORCM_SUCCESS,pugi_close(file_id));
}

TEST_F(ut_parser_pugi_tests, test_API_openTwice_validFile)
{
    int file_id_1 = pugi_open(validFile);
    int file_id_2 = pugi_open(validFile);
    ASSERT_TRUE(0 < file_id_1  && 0 < file_id_2);
    EXPECT_EQ(++openedFiles, file_id_1);
    EXPECT_EQ(++openedFiles, file_id_2);
    EXPECT_EQ(ORCM_SUCCESS, pugi_close(file_id_1));
    EXPECT_EQ(ORCM_SUCCESS, pugi_close(file_id_2));
}

TEST_F(ut_parser_pugi_tests, test_API_closeTwice_validFile)
{
    int file_id = pugi_open(validFile);
    ASSERT_EQ(++openedFiles, file_id);
    EXPECT_EQ(ORCM_SUCCESS, pugi_close(file_id));
    EXPECT_EQ(ORCM_ERROR, pugi_close(file_id));
}

TEST_F(ut_parser_pugi_tests, test_API_retrieveSection_invalidFileId)
{
    opal_list_t *list = pugi_retrieve_section(ORCM_ERROR, NULL, "", "");
    EXPECT_TRUE(NULL == list);
    SAFE_OBJ_RELEASE(list);
}

TEST_F(ut_parser_pugi_tests, test_API_retrieveSection_getFullDocument_allNull){
    opal_list_t *list;
    TEST_RETRIEVE_SECTION_NE(NULL, list, NULL, NULL, NULL);
    EXPECT_TRUE(1 == opal_list_get_size(list));
    SAFE_OBJ_RELEASE(list);
}

TEST_F(ut_parser_pugi_tests, test_API_retrieveSection_getFullDocument_emptyStrings)
{
    opal_list_t *list;
    TEST_RETRIEVE_SECTION_NE(NULL, list, NULL, "", "");
    EXPECT_TRUE(1 == opal_list_get_size(list));
    SAFE_OBJ_RELEASE(list);
}

TEST_F(ut_parser_pugi_tests, test_API_retrieveSection_getFullDocument_invalidKeyNoName)
{
    opal_list_t *list;
    TEST_RETRIEVE_SECTION_EQ(NULL, list, NULL, "group" , "");
    SAFE_OBJ_RELEASE(list);
}

TEST_F(ut_parser_pugi_tests, test_API_retrieveSection_getFullDocument_validKeyNoName)
{
    opal_list_t *list;
    TEST_RETRIEVE_SECTION_NE(NULL, list, NULL, "root" , "");
    EXPECT_TRUE(1 == opal_list_get_size(list));
    SAFE_OBJ_RELEASE(list);
}

TEST_F(ut_parser_pugi_tests, test_API_retrieveSection_FromGivenStart_validKeyNoName)
{
    opal_list_t *rootList, *groupList;
    int file_id = pugi_open(validFile);
    ASSERT_TRUE(0 < file_id);
    rootList = pugi_retrieve_section(file_id,NULL,"root","");
    EXPECT_TRUE(NULL != rootList);
    if (NULL == rootList) {
        pugi_close(file_id);
        return;
    }
    EXPECT_TRUE(1 == opal_list_get_size(rootList));
    groupList = pugi_retrieve_section(file_id, opal_list_get_first(rootList),
                                      "group","");
    EXPECT_TRUE(NULL != groupList);
    if (NULL == groupList) {
        pugi_close(file_id);
        SAFE_OBJ_RELEASE(rootList);
        return;
    }
    EXPECT_TRUE(4 == opal_list_get_size(groupList));
    EXPECT_FALSE(pugi_close(file_id));
    SAFE_OBJ_RELEASE(groupList);
}

TEST_F(ut_parser_pugi_tests,
       test_API_retrieveSection_FromGivenStart_validKeyValidName_Attribute)
{
    opal_list_t *rootList, *groupList;
    int file_id = pugi_open(validFile);
    ASSERT_TRUE(0 < file_id);
    rootList = pugi_retrieve_section(file_id,NULL,"root","");
    EXPECT_TRUE(NULL != rootList);
    if (NULL == rootList) {
        pugi_close(file_id);
        return;
    }
    EXPECT_TRUE(1 == opal_list_get_size(rootList));
    groupList = pugi_retrieve_section(file_id, opal_list_get_first(rootList),
                                      "group","group_name2");
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
       test_API_retrieveSection_FromGivenStart_validKeyValidName_Tag)
{
    opal_list_t *rootList, *groupList;
    int file_id = pugi_open(validFile);
    ASSERT_TRUE(0 < file_id);
    rootList = pugi_retrieve_section(file_id,NULL,"root","");
    EXPECT_TRUE(NULL != rootList);
    if (NULL == rootList) {
        pugi_close(file_id);
        return;
    }
    EXPECT_TRUE(1 == opal_list_get_size(rootList));
    groupList = pugi_retrieve_section(file_id, opal_list_get_first(rootList),
                                      "group","group_name3");
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

TEST_F(ut_parser_pugi_tests, test_API_retrieveSection_FromGivenStart_validKeyInvalidName)
{
    opal_list_t *rootList, *groupList;
    int file_id = pugi_open(validFile);
    ASSERT_TRUE(0 < file_id);
    rootList = pugi_retrieve_section(file_id,NULL,"root","");
    EXPECT_TRUE(NULL != rootList);
    if (NULL == rootList) {
        pugi_close(file_id);
        return;
    }
    EXPECT_TRUE(1 == opal_list_get_size(rootList));
    groupList = pugi_retrieve_section(file_id, opal_list_get_first(rootList),
                                      "group","group");
    EXPECT_TRUE(NULL == groupList);
    SAFE_OBJ_RELEASE(rootList);
    EXPECT_FALSE(pugi_close(file_id));
    SAFE_OBJ_RELEASE(groupList);
}

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
