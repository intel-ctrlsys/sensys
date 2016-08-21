/*
 * Copyright (c) 2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "analytics_example.h"
#include <iostream>

extern "C" {
    void initPlugin(void) {
        AnalyticsFactory* factory = AnalyticsFactory::getInstance();
        factory->setPluginCreator("example", Example::creator);
    }
}

Example::Example()
{

}

Example::~Example()
{

}

int Example::analyze(DataSet& data_set)
{
    std::cout << "This is an example plugin!" << std::endl;
    return 0;
}

Analytics* Example::creator()
{
    Analytics* example = new Example();
    return example;
}


