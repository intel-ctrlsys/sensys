/*
 * Copyright (c) 2017   Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#ifndef SENSORXMLMANAGERTESTS_H
#define SENSORXMLMANAGERTESTS_H
#include "gtest/gtest.h"
#include "xmlFiles.h"

class ut_sensorXmlManager : public testing::Test {
public:
    xmlFiles test_files;

    ut_sensorXmlManager() {
    }

    ~ut_sensorXmlManager() {
    }
};

#endif /* SENSORXMLMANAGERTESTS_H */

