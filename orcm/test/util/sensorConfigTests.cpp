/*
 * Copyright (c) 2017   Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "sensorConfigTests.h"
#include "orcm/common/sensorconfig.hpp"

TEST_F(ut_sensorConfig, getConfigFromFile_non_existent) {
    sensorConfigNodePointer config =
            getConfigFromFile("missingfile.xml", XML, UDSENSORS_TAG);
    ASSERT_TRUE(NULL == config);
}

TEST_F(ut_sensorConfig, getConfigFromFile_bad_filetype) {
    sensorConfigNodePointer config =
            getConfigFromFile("missingfile.xml", UNKNOWN, UDSENSORS_TAG);
    ASSERT_TRUE(NULL == config);
}

TEST_F(ut_sensorConfig, getConfigFromFile_no_sensors_tag) {
    test_files.create_test_file(test_files.NO_UDSENSOR_TAG_FILE);
    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);
    ASSERT_TRUE(NULL == config);
}

TEST_F(ut_sensorConfig, getConfigFromFile_valid) {
    test_files.create_test_file(test_files.DEFAULT_FILE);
    sensorConfigNodePointer config =
            getConfigFromFile(XML_TEST_FILE, XML, UDSENSORS_TAG);
    ASSERT_TRUE(NULL != config);
}
