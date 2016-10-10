/*
 * Copyright (c) 2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "../average/analytics_average.h"

#include <iostream>

extern "C" {
    void initPlugin(void) {
        AnalyticsFactory* factory = AnalyticsFactory::getInstance();
        factory->setPluginCreator("average", Average::creator);
    }
}

Average::Average()
{

}

Average::~Average()
{

}

int Average::analyze(DataSet& data_set)
{
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
    data_set.results.put<double>("Average_key", avg, "C");

    //Example to create an event for a sample
    Event ev = {
    "crit",
    "syslog",
    "Test_Average",
    avg,
    "C",
    };
    data_set.events.push_back(ev);
    return 0;
}

Analytics* Average::creator()
{
    Analytics* average = new Average();
    return average;
}


