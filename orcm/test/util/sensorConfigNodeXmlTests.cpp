/*
 * Copyright (c) 2017   Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <list>

#include "sensorConfigNodeXmlTests.h"
#include "orcm/common/sensorConfigNodeXml.hpp"

TEST_F(ut_sensorConfigNodeXml, get_attribute_missing) {
    test_files.create_test_file(test_files.DEFAULT_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    ASSERT_EQ("", config->get_attribute("missingattribute"));
}

TEST_F(ut_sensorConfigNodeXml, get_attribute) {
    test_files.create_test_file(test_files.ROOT_ATTRIBUTES_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    ASSERT_EQ("one", config->get_attribute("test_attribute_one"));
    ASSERT_EQ("two", config->get_attribute("test_attribute_two"));
}

TEST_F(ut_sensorConfigNodeXml, get_attributes_list) {
    test_files.create_test_file(test_files.ROOT_ATTRIBUTES_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    attributes_list_t attributes = config->get_attributes_list();
    ASSERT_EQ(2, attributes.size());
    ASSERT_EQ("test_attribute_one", attributes.front().first);
    ASSERT_EQ("one", attributes.front().second);
    ASSERT_EQ("test_attribute_two", attributes.back().first);
    ASSERT_EQ("two", attributes.back().second);
}

TEST_F(ut_sensorConfigNodeXml, get_attributes_list_attribute_tag) {
    test_files.create_test_file(test_files.ROOT_ATTRIBUTES_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    string_list_t attributes =
            config->get_attributes_list("test_attribute_one");

    ASSERT_EQ(1, attributes.size());
    ASSERT_EQ("one", attributes.front());
}

TEST_F(ut_sensorConfigNodeXml, get_attributes_list_attribute_empty_tag) {
    test_files.create_test_file(test_files.ROOT_ATTRIBUTES_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    string_list_t attributes =
            config->get_attributes_list("");

    ASSERT_EQ(0, attributes.size());
}

TEST_F(ut_sensorConfigNodeXml, get_attributes_list_object_tag) {
    test_files.create_test_file(test_files.ROOT_ATTRIBUTES_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    string_list_t attributes =
            config->get_attributes_list("test_attribute_two");

    ASSERT_EQ(1, attributes.size());
    ASSERT_EQ("two", attributes.front());
}

TEST_F(ut_sensorConfigNodeXml, get_attributes_list_object_empty_tag) {
    test_files.create_test_file(test_files.ROOT_ATTRIBUTES_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    string_list_t attributes =
            config->get_attributes_list("");

    ASSERT_EQ(0, attributes.size());
}

TEST_F(ut_sensorConfigNodeXml, get_attributes_list_object_tag_no_attribute) {
    test_files.create_test_file(test_files.ROOT_ATTRIBUTES_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    string_list_t attributes = config->get_attributes_list("dfx");
    ASSERT_EQ(0, attributes.size());
}

TEST_F(ut_sensorConfigNodeXml, get_object_list) {
    test_files.create_test_file(test_files.ROOT_ATTRIBUTES_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    objects_list_t objects = config->get_object_list();
    ASSERT_EQ(1, objects.size());
    ASSERT_EQ("dfx", objects.front()->get_tag());
}

TEST_F(ut_sensorConfigNodeXml, get_object_list_tag) {
    test_files.create_test_file(test_files.ROOT_ATTRIBUTES_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    objects_list_t objects = config->get_object_list("dfx");
    ASSERT_EQ(1, objects.size());
}

TEST_F(ut_sensorConfigNodeXml, get_object_list_tag_not_found) {
    test_files.create_test_file(test_files.ROOT_ATTRIBUTES_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    objects_list_t objects = config->get_object_list("not_found");
    ASSERT_EQ(0, objects.size());
    objects = config->get_object_list("test_attribute_two");
    ASSERT_EQ(0, objects.size());
}

TEST_F(ut_sensorConfigNodeXml, get_object_list_by_attribute) {
    test_files.create_test_file(test_files.DEFAULT_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    objects_list_t objects =
            config->get_object_list_by_attribute("dfx", "name", "bky");

    ASSERT_EQ(1, objects.size());
    ASSERT_EQ("bky", objects.front()->get_attribute("name"));

    objects = config->get_object_list_by_attribute("dfx", "id", "myid");
    ASSERT_EQ(1, objects.size());
    ASSERT_EQ("sku", objects.front()->get_attribute("name"));

    objects_list_t inner_objects =
            objects.front()->get_object_list_by_attribute("complicated_thing",
                                                          "name",
                                                          "unknown name");
    ASSERT_EQ(1, inner_objects.size());
}

TEST_F(ut_sensorConfigNodeXml, get_object_list_by_attribute_empty) {
    test_files.create_test_file(test_files.DEFAULT_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    objects_list_t objects =
            config->get_object_list_by_attribute("", "", "");

    ASSERT_EQ(0, objects.size());
}

TEST_F(ut_sensorConfigNodeXml,
       get_object_list_by_attribute_wrong_attribute_object) {

    test_files.create_test_file(test_files.DEFAULT_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    objects_list_t objects =
            config->get_object_list_by_attribute("dfx",
                                                 "complicated_thing",
                                                 "myid");
    ASSERT_EQ(0, objects.size());

    objects = config->get_object_list_by_attribute("dfx", "id", "myid");
    ASSERT_EQ(1, objects.size());

    objects_list_t inner_objects =
            objects.front()->get_object_list_by_attribute("injection_rate",
                                                          "name",
                                                          "unknown name");
    ASSERT_EQ(0, inner_objects.size());
}

TEST_F(ut_sensorConfigNodeXml, get_tag) {
    test_files.create_test_file(test_files.DEFAULT_FILE);

    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);

    ASSERT_EQ("udsensors", config->get_tag());
}
