/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "gtest/gtest.h"
#include "orcm/mca/analytics/extension_plugins/example/analytics_example.h"

TEST(analytics_example, analyze_test)
{
    AnalyticsFactory* analytics_factory = AnalyticsFactory::getInstance();
    Analytics* example = Example::creator();
    dataContainer dc;
    std::list<Event> events;
    DataSet data_set(dc, events);
    int rc = example->analyze(data_set);
    delete example;
    ASSERT_EQ(0, rc);
}
