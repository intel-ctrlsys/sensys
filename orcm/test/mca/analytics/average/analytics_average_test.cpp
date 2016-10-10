/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/analytics/extension_plugins/average/analytics_average.h"
#include "gtest/gtest.h"

TEST(analytics_average, analyze_test)
{
    AnalyticsFactory* analytics_factory = AnalyticsFactory::getInstance();
    Analytics* average = Average::creator();
    dataContainer dc;
    std::list<Event> events;
    DataSet data_set(dc, events);
    int rc = average->analyze(data_set);
    delete average;
    ASSERT_EQ(0, rc);
}
