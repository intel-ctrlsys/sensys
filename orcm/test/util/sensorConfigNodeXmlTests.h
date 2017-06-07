/*
 * Copyright (c) 2017   Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#ifndef SENSORCONFIGNODEXMLTESTS_H
#define SENSORCONFIGNODEXMLTESTS_H
#include "gtest/gtest.h"
#include "xmlFiles.h"

class ut_sensorConfigNodeXml : public testing::Test {
public:
    xmlFiles test_files;

    ut_sensorConfigNodeXml() {
    }

    ~ut_sensorConfigNodeXml() {
    }
};

#endif /* SENSORCONFIGNODEXMLTESTS_H */

