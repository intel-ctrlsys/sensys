/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "db_print_test.h"
#include "gtest/gtest.h"
#include "orcm/test/mca/analytics/util/analytics_util.h"

TEST(db_print, init_normal_file)
{
    int rc = ORCM_SUCCESS;
    const char *file_path = "/tmp/print_test.txt";
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->file = strdup((char*)file_path);
    rc = mca_db_print_module.api.init((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_SUCCESS, rc);
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST(db_print, finalize_stdout_fp)
{
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stdout;
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST(db_print, finalize_stderr_fp)
{
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stderr;
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST(db_print, commit)
{
    int rc = ORCM_SUCCESS;
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stdout;
    rc = mca_db_print_module.api.commit((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_SUCCESS, rc);
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST(db_print, rollback)
{
    int rc = ORCM_SUCCESS;
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stderr;
    rc = mca_db_print_module.api.rollback((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_SUCCESS, rc);
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST(db_print, record_diag_test_valid_diag_type_null_others)
{
    int rc = ORCM_SUCCESS;
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stdout;
    const char *diag_type = "cpu";
    rc = mca_db_print_module.api.record_diag_test((orcm_db_base_module_t*)mod, NULL, diag_type,
                                                  NULL, NULL, NULL, NULL, NULL, NULL);
    ASSERT_EQ(ORCM_SUCCESS, rc);
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST(db_print, record_diag_test_valid_result_null_others)
{
    int rc = ORCM_SUCCESS;
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stdout;
    const char *test_result = "Success";
    rc = mca_db_print_module.api.record_diag_test((orcm_db_base_module_t*)mod, NULL, NULL, NULL,
                                                  NULL, NULL, NULL, test_result, NULL);
    ASSERT_EQ(ORCM_SUCCESS, rc);
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST(db_print, record_data_samples_valid_hostname_null_others)
{
    int rc = ORCM_SUCCESS;
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stdout;
    const char *hostname = "RamdomeName";
    rc = mca_db_print_module.api.record_data_samples((orcm_db_base_module_t*)mod, hostname, NULL,
                                                     NULL, NULL);
    ASSERT_EQ(ORCM_SUCCESS, rc);
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST(db_print, record_data_samples_valid_data_groups_null_others)
{
    int rc = ORCM_SUCCESS;
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stdout;
    const char *data_groups = "coretemp";
    rc = mca_db_print_module.api.record_data_samples((orcm_db_base_module_t*)mod, NULL, NULL,
                                                     data_groups, NULL);
    ASSERT_EQ(ORCM_SUCCESS, rc);
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST(db_print, record_data_samples_negative_time_usec)
{
    int rc = ORCM_SUCCESS;
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stdout;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    current_time.tv_usec = -1;
    rc = mca_db_print_module.api.record_data_samples((orcm_db_base_module_t*)mod, NULL,
                                                     &current_time, NULL, NULL);
    ASSERT_EQ(ORCM_SUCCESS, rc);
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST(db_print, record_data_samples_larger_time_usec)
{
    int rc = ORCM_SUCCESS;
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stdout;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    current_time.tv_usec = 2000000;
    rc = mca_db_print_module.api.record_data_samples((orcm_db_base_module_t*)mod, NULL,
                                                     &current_time, NULL, NULL);
    ASSERT_EQ(ORCM_SUCCESS, rc);
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST(db_print, record_data_samples_sample_null_key_null_unit)
{
    int rc = ORCM_SUCCESS;
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stdout;
    opal_list_t *list = OBJ_NEW(opal_list_t);
    orcm_value_t *mv = NULL;
    double d_value = 37.5;
    mv = analytics_util::load_orcm_value("", (void*)(&d_value), OPAL_DOUBLE, "");
    if (NULL != mv) {
        opal_list_append(list, (opal_list_item_t*)mv);
    }

    rc = mca_db_print_module.api.record_data_samples((orcm_db_base_module_t*)mod,
                                                      NULL,NULL, NULL, list);
    ASSERT_EQ(ORCM_SUCCESS, rc);
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(list);
}

TEST(db_print, update_node_features_valid_hostname_null_features)
{
    int rc = ORCM_SUCCESS;
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stdout;
    const char *hostname = "RandomName";

    rc = mca_db_print_module.api.update_node_features((orcm_db_base_module_t*)mod, hostname, NULL);

    ASSERT_EQ(ORCM_SUCCESS, rc);
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST(db_print, update_node_features_null_hostname_valid_features)
{
    int rc = ORCM_SUCCESS;
    mca_db_print_module_t *mod = (mca_db_print_module_t*)malloc(sizeof(mca_db_print_module_t));
    memset(mod, 0, sizeof(mca_db_print_module_t));
    mod->fp = stdout;
    opal_list_t *list = OBJ_NEW(opal_list_t);
    orcm_value_t *mv = NULL;

    // cover the opal size branch of the print_value function
    size_t size = 10;
    mv = analytics_util::load_orcm_value("Key", (void*)(&size), OPAL_SIZE, "Unit");
    if (NULL != mv) {
        opal_list_append(list, (opal_list_item_t*)mv);
    }

    // cover the uint8 branch of the print_value function
    uint8_t uint8_value = 10;
    mv = analytics_util::load_orcm_value("Key", (void*)(&uint8_value), OPAL_UINT8, "Unit");
    if (NULL != mv) {
        opal_list_append(list, (opal_list_item_t*)mv);
    }

    // cover the uint16 branch of the print_value function
    uint16_t uint16_value = 10;
    mv = analytics_util::load_orcm_value("Key", (void*)(&uint16_value), OPAL_UINT16, "Unit");
    if (NULL != mv) {
        opal_list_append(list, (opal_list_item_t*)mv);
    }

    // cover the uint32 branch of the print_value function
    uint32_t uint32_value = 10;
    mv = analytics_util::load_orcm_value("Key", (void*)(&uint32_value), OPAL_UINT32, "Unit");
    if (NULL != mv) {
        opal_list_append(list, (opal_list_item_t*)mv);
    }

    // cover the boolean true branch of the print_value function
    bool b_value_t = true;
    mv = analytics_util::load_orcm_value("Key", (void*)(&b_value_t), OPAL_BOOL, "Unit");
    if (NULL != mv) {
        opal_list_append(list, (opal_list_item_t*)mv);
    }

    // cover the boolean false branch of the print_value function
    bool b_value_f = false;
    mv = analytics_util::load_orcm_value("Key", (void*)(&b_value_f), OPAL_BOOL, "Unit");
    if (NULL != mv) {
        opal_list_append(list, (opal_list_item_t*)mv);
    }

    // cover the undefined type of the print_value function
    uint8_t type = 100;
    mv = analytics_util::load_orcm_value("Key", (void*)(&b_value_f), OPAL_BOOL, "Unit");
    if (NULL != mv) {
        mv->value.type = type;
        opal_list_append(list, (opal_list_item_t*)mv);
    }

    rc = mca_db_print_module.api.update_node_features((orcm_db_base_module_t*)mod, NULL, list);
    ASSERT_EQ(ORCM_SUCCESS, rc);
    mca_db_print_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(list);
}
