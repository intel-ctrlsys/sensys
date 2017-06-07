/*
 * Copyright (c) 2017   Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#ifndef SENSORCONFIGTESTS_H
#define SENSORCONFIGTESTS_H
#include "gtest/gtest.h"
#include "xmlFiles.h"

class ut_sensorConfig : public testing::Test {
public:
    xmlFiles test_files;

    ut_sensorConfig() {
    }

    ~ut_sensorConfig() {
    }
};

#endif /* SENSORCONFIGTESTS_H */

