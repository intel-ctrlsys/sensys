
/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "gtest/gtest.h"
// OPAL
#include "opal/dss/dss.h"

extern "C" {
    #include "orcm/mca/analytics/base/analytics_private.h"
    #include "orcm/mca/analytics/analytics.h"
    #include "orcm/mca/analytics/analytics_types.h"
    #include "orcm/util/utils.h"
}


TEST(analytics_base, control_storage_positive_test_1)
{
    int rc;

    rc = orcm_analytics_base_control_storage(ORCM_SENSOR_STORAGE_ENVIRONMENT_ONLY);

    ASSERT_EQ(ORCM_SUCCESS, rc);
    ASSERT_EQ(orcm_analytics_base.store_raw_data, true);
}

TEST(analytics_base, control_storage_positive_test_2)
{
    int rc;

    rc = orcm_analytics_base_control_storage(ORCM_SENSOR_STORAGE_EXCEPTION_ONLY);

    ASSERT_EQ(ORCM_SUCCESS, rc);
    ASSERT_EQ(orcm_analytics_base.store_event_data, true);
}

TEST(analytics_base, control_storage_positive_test_3)
{
    int rc;

    rc = orcm_analytics_base_control_storage(ORCM_SENSOR_STORAGE_BOTH);

    ASSERT_EQ(ORCM_SUCCESS, rc);
    ASSERT_EQ(orcm_analytics_base.store_raw_data, true);
    ASSERT_EQ(orcm_analytics_base.store_event_data, true);
}

TEST(analytics_base, control_storage_positive_test_4)
{
    int rc;

    rc = orcm_analytics_base_control_storage(ORCM_SENSOR_STORAGE_NONE);

    ASSERT_EQ(ORCM_SUCCESS, rc);
    ASSERT_EQ(orcm_analytics_base.store_raw_data, false);
    ASSERT_EQ(orcm_analytics_base.store_event_data, false);
}

TEST(analytics_base, control_storage_negative_test_1)
{
    int rc;

    rc = orcm_analytics_base_control_storage(-1);

    ASSERT_NE(ORCM_SUCCESS, rc);
}

TEST(analytics_base, control_storage_negative_test_2)
{
    int rc;

    rc = orcm_analytics_base_control_storage(100);

    ASSERT_NE(ORCM_SUCCESS, rc);
}

