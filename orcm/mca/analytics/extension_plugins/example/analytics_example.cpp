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

    //Example to compute sample average
    dataContainer::iterator i = data_set.compute.begin();
    double avg, sum = 0.0;
    int n = 0;
    while(i != data_set.compute.end()) {
        sum += data_set.compute.getValue<double>(i);
        n++;
        i++;
    }
    avg = sum / n;
    data_set.results.put<double>("Example_key", avg, "C");

    //Example to create an event for a sample
    Event ev = {
    "crit",
    "syslog",
    "Test_example",
    avg,
    "C",
    };
    data_set.events.push_back(ev);
    return 0;
}

Analytics* Example::creator()
{
    Analytics* example = new Example();
    return example;
}


