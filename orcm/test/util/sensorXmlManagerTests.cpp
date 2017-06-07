/*
 * Copyright (c) 2017   Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "sensorXmlManagerTests.h"
#include "orcm/common/xmlManager.cpp"

TEST_F(ut_sensorXmlManager, open_file_valid) {
    xmlManager file_handler;
    test_files.create_test_file(test_files.DEFAULT_FILE);
    ASSERT_EQ(0, file_handler.open_file(XML_TEST_FILE));
}

TEST_F(ut_sensorXmlManager, open_file_non_parsable) {
    xmlManager file_handler;
    test_files.create_test_file(test_files.NON_PARSABLE_FILE);
    ASSERT_NE(0, file_handler.open_file(XML_TEST_FILE));
}

TEST_F(ut_sensorXmlManager, open_file_non_existent) {
    xmlManager file_handler;
    ASSERT_NE(0, file_handler.open_file("missingfile.xml"));
}

TEST_F(ut_sensorXmlManager, get_root_node_valid) {
    xmlManager file_handler;
    test_files.create_test_file(test_files.DEFAULT_FILE);
    file_handler.open_file(XML_TEST_FILE);
    pugi::xml_node *root = file_handler.get_root_node("udsensors");
    ASSERT_EQ(true, NULL != root);
    file_handler.print_node(*root, "");
    root = file_handler.get_root_node("lustre");
    ASSERT_EQ(true, NULL != root);
}

TEST_F(ut_sensorXmlManager, get_root_node_missing_tag) {
    xmlManager file_handler;
    test_files.create_test_file(test_files.DEFAULT_FILE);
    file_handler.open_file(XML_TEST_FILE);
    pugi::xml_node *root = file_handler.get_root_node("bky_list");
    ASSERT_EQ(true, NULL == root);
}
