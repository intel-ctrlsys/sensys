/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
*/
#include "db_base_select_test.h"

// Placeholder for future tests
#include "opal/dss/dss_types.h"
#include "opal/class/opal_list.h"
#include "opal/class/opal_hash_table.h"
//#include <sys/time.h>

using namespace std;

extern "C" {
    #include "orcm/mca/db/base/base.h"
    #include "opal/mca/base/mca_base_framework.h"
    #include "orcm/include/orcm/constants.h"

    extern bool orcm_db_base_selected;
    extern void bucket_con(orcm_db_bucket_t *bucket);
    extern void bucket_des(orcm_db_bucket_t *bucket);
    extern int orcm_db_base_select(void);
    extern int orcm_db_base_frame_open(mca_base_open_flag_t flags);
    extern int orcm_db_base_init_storages(void);
    extern int orcm_db_base_init_thread_count(void);
}

void ut_db_base_cleanup()
{
    if(NULL != orcm_db_base.thread_counts)
        OBJ_RELEASE(orcm_db_base.thread_counts);

    if(NULL != orcm_db_base.buckets)
    {
        opal_hash_table_remove_all(orcm_db_base.buckets);
        OBJ_RELEASE(orcm_db_base.buckets);
    }
}

void ut_db_base_select_test::SetUpTestCase()
{
    return;
}

void ut_db_base_select_test::TearDownTestCase()
{
    return;
}


TEST_F(ut_db_base_select_test, db_base_select_test)
{
    orcm_db_base_selected = true;
    EXPECT_EQ(ORCM_SUCCESS, orcm_db_base_select());
    orcm_db_base_selected = false;
}

TEST_F(ut_db_base_select_test, bucket_con_des_test)
{
    orcm_db_bucket_t *bucket;
    bucket = OBJ_NEW(orcm_db_bucket_t);
    OBJ_RELEASE(bucket);
}

TEST_F(ut_db_base_select_test, db_base_get_storage)
{
    putenv("ORCM_MCA_db=print,postgres");
    int rc = orcm_db_base_init_storages();
    EXPECT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_db_base_select_test, db_base_get_storage_pos_neg)
{
    putenv("ORCM_MCA_db=odbc,postgres");
    int rc = orcm_db_base_init_storages();
    EXPECT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(ut_db_base_select_test, db_base_get_storage_neg)
{
    putenv("ORCM_MCA_db=^odbc,postgres");
    int rc = orcm_db_base_init_storages();
    EXPECT_EQ(ORCM_SUCCESS, rc);
}


TEST_F(ut_db_base_select_test, db_base_get_thread_positive)
{
    orcm_db_base.thread_counts = strdup("4,4");
    int rc = orcm_db_base_init_thread_count();
    EXPECT_EQ(ORCM_SUCCESS, rc);
    ut_db_base_cleanup();
}


TEST_F(ut_db_base_select_test, db_base_get_thread)
{
    orcm_db_base.thread_counts = strdup("11.4,5.4");
    int rc = orcm_db_base_init_thread_count();
    EXPECT_EQ(ORCM_ERR_BAD_PARAM, rc);
    ut_db_base_cleanup();
}


TEST_F(ut_db_base_select_test, db_base_get_thread_negative)
{
    orcm_db_base.thread_counts = strdup("-2,-3");
    int rc = orcm_db_base_init_thread_count();
    EXPECT_EQ(ORCM_ERR_BAD_PARAM, rc);
    ut_db_base_cleanup();
}

TEST_F(ut_db_base_select_test, db_base_get_thread_non_number)
{
    orcm_db_base.thread_counts = strdup("o,p");
    int rc = orcm_db_base_init_thread_count();
    EXPECT_EQ(ORCM_ERR_BAD_PARAM, rc);
    ut_db_base_cleanup();
}

TEST_F(ut_db_base_select_test, db_base_get_thread_more_than_4)
{
    orcm_db_base.thread_counts = strdup("2,3,4,5,6,6");
    int rc = orcm_db_base_init_thread_count();
    EXPECT_EQ(ORCM_SUCCESS, rc);
    ut_db_base_cleanup();
}

TEST_F(ut_db_base_select_test, db_base_get_event_data)
{
    orcm_db_base.thread_counts = strdup("2,2");
    putenv("ORCM_MCA_db=print,postgres");
    int rc=-1;
    OBJ_CONSTRUCT(&orcm_db_base.actives, opal_list_t);
    OBJ_CONSTRUCT(&orcm_db_base.handles, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_db_base.handles, 4, INT_MAX, 1);
    orcm_db_base_init_thread_count();

    orcm_db_base_init_storages();
    orcm_db_base.buckets = OBJ_NEW(opal_hash_table_t);
    opal_hash_table_init(orcm_db_base.buckets, NUM_DATA_TYPE);
    orcm_db_base_commit_multi_thread_select(ORCM_DB_EVENT_DATA, 0, NULL, NULL);
    orcm_db_base_open_multi_thread_select(ORCM_DB_EVENT_DATA, NULL, NULL, NULL);
    orcm_db_base_open_multi_thread_select(ORCM_DB_EVENT_DATA, NULL, NULL, NULL);
    EXPECT_EQ(2, orcm_db_base.num_storage);

    orcm_db_base_close_multi_thread_select(ORCM_DB_EVENT_DATA, NULL, NULL);
    EXPECT_EQ(4, opal_pointer_array_get_size(&orcm_db_base.handles));
    ut_db_base_cleanup();
}

TEST_F(ut_db_base_select_test, db_base_get_inventory_data)
{
    orcm_db_base.thread_counts = strdup("2,2");
    putenv("ORCM_MCA_db=print,postgres");
    int rc=-1;
    OBJ_CONSTRUCT(&orcm_db_base.actives, opal_list_t);
    OBJ_CONSTRUCT(&orcm_db_base.handles, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_db_base.handles, 4, INT_MAX, 1);
    orcm_db_base_init_thread_count();

    orcm_db_base_init_storages();
    orcm_db_base.buckets = OBJ_NEW(opal_hash_table_t);
    opal_hash_table_init(orcm_db_base.buckets, NUM_DATA_TYPE);
    orcm_db_base_commit_multi_thread_select(ORCM_DB_INVENTORY_DATA, 0, NULL, NULL);
    orcm_db_base_open_multi_thread_select(ORCM_DB_INVENTORY_DATA, NULL, NULL, NULL);
    orcm_db_base_open_multi_thread_select(ORCM_DB_INVENTORY_DATA, NULL, NULL, NULL);
    EXPECT_EQ(2, orcm_db_base.num_storage);

    orcm_db_base_close_multi_thread_select(ORCM_DB_INVENTORY_DATA, NULL, NULL);
    EXPECT_EQ(4, opal_pointer_array_get_size(&orcm_db_base.handles));
    ut_db_base_cleanup();
}

TEST_F(ut_db_base_select_test, db_base_get_diag_data)
{
    orcm_db_base.thread_counts = strdup("2,2");
    putenv("ORCM_MCA_db=print,postgres");
    int rc=-1;
    OBJ_CONSTRUCT(&orcm_db_base.actives, opal_list_t);
    OBJ_CONSTRUCT(&orcm_db_base.handles, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_db_base.handles, 4, INT_MAX, 1);
    orcm_db_base_init_thread_count();

    orcm_db_base_init_storages();
    orcm_db_base.buckets = OBJ_NEW(opal_hash_table_t);
    opal_hash_table_init(orcm_db_base.buckets, NUM_DATA_TYPE);
    orcm_db_base_commit_multi_thread_select(ORCM_DB_DIAG_DATA, 0, NULL, NULL);
    orcm_db_base_open_multi_thread_select(ORCM_DB_DIAG_DATA, NULL, NULL, NULL);
    orcm_db_base_open_multi_thread_select(ORCM_DB_DIAG_DATA, NULL, NULL, NULL);
    EXPECT_EQ(2, orcm_db_base.num_storage);

    orcm_db_base_close_multi_thread_select(ORCM_DB_DIAG_DATA, NULL, NULL);
    EXPECT_EQ(4, opal_pointer_array_get_size(&orcm_db_base.handles));
    ut_db_base_cleanup();
}

TEST_F(ut_db_base_select_test, db_base_multi_thread_rollback)
{
    orcm_db_base_rollback_multi_thread_select(-1, NULL, NULL);
}
