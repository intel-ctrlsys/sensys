/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "parser_tests.h"

using namespace std;

TEST_F(ut_parser_tests, test_createParser_emptyConstructor){
    xmlparser p;
    xmlparser *ptr = new xmlparser();
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}

TEST_F(ut_parser_tests, test_createParser_emptyStringConstructor){
    xmlparser p(string(""));
    xmlparser *ptr = new xmlparser(string(""));
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}

TEST_F(ut_parser_tests, test_createParser_emptyCharptrConstructor){
    xmlparser p("");
    xmlparser *ptr = new xmlparser("");
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}


TEST_F(ut_parser_tests, test_createParser_stringConstructor){
    xmlparser p(string(invalidFile));
    xmlparser *ptr = new xmlparser(string(invalidFile));
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}


TEST_F(ut_parser_tests, test_createParser_charptrConstructor){
    char *str=strdup(invalidFile);
    xmlparser p(str);
    xmlparser *ptr = new xmlparser(str);
    if (str) free(str);
    ASSERT_TRUE(ptr != NULL);
    delete ptr;
}

TEST_F(ut_parser_tests, test_API_open_invalidFile){
    int file_id = parser_open_file(invalidFile);
    ASSERT_TRUE(ORCM_ERR_FILE_OPEN_FAILURE == file_id);
}

TEST_F(ut_parser_tests, test_API_openAndClose_validFile){
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(0 < file_id);
    parser_close_file(file_id);
}

TEST_F(ut_parser_tests, test_API_openTwice_validFile){
    int file_id_1 = parser_open_file(validFile);
    int file_id_2 = parser_open_file(validFile);
    ASSERT_TRUE(0 < file_id_1  && 0 < file_id_2);
    parser_close_file(file_id_1);
    parser_close_file(file_id_2);
    ASSERT_NE(file_id_1,file_id_2);
}

TEST_F(ut_parser_tests, test_API_closeTwice_validFile){
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(0 < file_id);
    parser_close_file(file_id);
    try {
        parser_close_file(file_id);
    } catch (exception e){
        ASSERT_TRUE(0);
    }
}

TEST_F(ut_parser_tests, test_API_close_invalidFileId){
    try {
        parser_close_file(-1);
    } catch (exception e) {
        ASSERT_TRUE(0);
    }
}

TEST_F(ut_parser_tests, test_API_retrieveValue_invalidKey){
    char *value;
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(0 < file_id);
    ASSERT_TRUE(ORCM_ERR_NOT_FOUND == parser_retrieve_value(&value,
                                      file_id,invalidKey));
    ASSERT_TRUE(NULL == value);
    parser_free_value(value);
    parser_close_file(file_id);
}

TEST_F(ut_parser_tests, test_API_retrieveValue_validKeyWithName){
    char *value;
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(file_id > 0);
    ASSERT_FALSE(ORCM_ERR_NOT_FOUND == parser_retrieve_value(&value,
                                       file_id,"root:group=group_name2"));
    ASSERT_STRNE(NULL,value);
    EXPECT_STREQ("groupname2_value",value);
    parser_free_value(value);
    parser_close_file(file_id);
}

TEST_F(ut_parser_tests, test_API_retrieveValue_validKeyNoName){
    char *value;
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(file_id > 0);
    ASSERT_FALSE(ORCM_ERR_NOT_FOUND == parser_retrieve_value(&value,file_id,
                                       "root:group=group_name3:tagnoname"));
    ASSERT_STRNE(NULL,value);
    EXPECT_STREQ("tagnoname_value",value);
    parser_free_value(value);
    parser_close_file(file_id);
}

TEST_F(ut_parser_tests, test_API_retrieveValue_validKeyWithNameIsChild){
    char *value;
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(file_id > 0);
    ASSERT_FALSE(ORCM_ERR_NOT_FOUND == parser_retrieve_value(&value,file_id,
                        "root:group=group_name1:tagnoname:tagname=tag_name1"));
    ASSERT_STRNE(NULL,value);
    EXPECT_STREQ("tagname_value",value);
    parser_free_value(value);
    parser_close_file(file_id);
}


TEST_F(ut_parser_tests, test_API_retrieveValue_validKeyCannotFindWithoutName){
    char *value;
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(file_id > 0);
    ASSERT_TRUE(ORCM_ERR_NOT_FOUND == parser_retrieve_value(&value,
                                      file_id,"root:group"));
    ASSERT_STREQ(NULL,value);
    parser_free_value(value);
    parser_close_file(file_id);
}

TEST_F(ut_parser_tests, test_API_retrieveArray_invalidKey){
    char **array;
    int size;
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(file_id > 0);
    ASSERT_TRUE(ORCM_ERR_NOT_FOUND == parser_retrieve_array(&array,
                                      &size,file_id,invalidKey));
    ASSERT_TRUE(NULL == array);
    EXPECT_TRUE(0 == size);
    parser_free_array(array);
    parser_close_file(file_id);
}

TEST_F(ut_parser_tests, test_API_retrieveArray_validKey){
    char **array;
    int size;
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(file_id > 0);
    ASSERT_FALSE(ORCM_ERR_NOT_FOUND == parser_retrieve_array(&array,
                                       &size,file_id,"root:group"));
    ASSERT_FALSE(NULL == array);
    ASSERT_TRUE(3 == size);
    EXPECT_STREQ(array[0],"group_name1");
    EXPECT_STREQ(array[1],"group_name2");
    EXPECT_STREQ(array[2],"group_name3");
    parser_free_array(array);
    parser_close_file(file_id);
}

TEST_F(ut_parser_tests, test_API_retrieveArray_validKeyWithName){
    char **array;
    int size;
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(file_id > 0);
    ASSERT_FALSE(ORCM_ERR_NOT_FOUND==parser_retrieve_array(&array,&size,file_id,
                 "root:group=group_name1:tagnoname:tagname=tag_name2"));
    ASSERT_FALSE(NULL == array);
    ASSERT_TRUE(2 == size);
    EXPECT_STREQ(array[0],"anothertag_name1");
    EXPECT_STREQ(array[1],"anothertag_name2");
    parser_free_array(array);
    parser_close_file(file_id);
}

TEST_F(ut_parser_tests, test_API_retrieveArray_validKeyIsChild){
    char **array;
    int size;
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(file_id > 0);
    ASSERT_FALSE(ORCM_ERR_NOT_FOUND==parser_retrieve_array(&array,&size,file_id,
                 "root:group=group_name1:tagnoname:tagname"));
    ASSERT_FALSE(NULL == array);
    ASSERT_TRUE(2 == size);
    EXPECT_STREQ(array[0],"tag_name1");
    EXPECT_STREQ(array[1],"tag_name2");
    parser_free_array(array);
    parser_close_file(file_id);
}

TEST_F(ut_parser_tests, test_API_retrieveArray_validKeyIsEmptyChild){
    char **array;
    int size;
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(file_id > 0);
    ASSERT_FALSE(ORCM_ERR_NOT_FOUND==parser_retrieve_array(&array,&size,file_id,
             "root:group=group_name1:tagnoname:tagname=tag_name2:anothertag"));
    ASSERT_FALSE(NULL == array);
    ASSERT_TRUE(2 == size);
    EXPECT_STREQ(array[0],"anothertag_name1");
    EXPECT_STREQ(array[1],"anothertag_name2");
    parser_free_array(array);
    parser_close_file(file_id);
}

TEST_F(ut_parser_tests, test_API_retrieveArray_invalidKeyNoNames){
    char **array;
    int size;
    int file_id = parser_open_file(validFile);
    ASSERT_TRUE(file_id > 0);
    ASSERT_TRUE(ORCM_ERR_NOT_FOUND==parser_retrieve_array(&array,&size,file_id,
                "root:group=group_name3:tagnoname"));
    ASSERT_TRUE(NULL == array);
    ASSERT_TRUE(0 == size);
    parser_free_array(array);
    parser_close_file(file_id);
}

