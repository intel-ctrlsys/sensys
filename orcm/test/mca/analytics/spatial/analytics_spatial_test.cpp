/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "gtest/gtest.h"

#include "analytics_spatial_test.h"
#include "analytics_util.h"
#include <sstream>
#include <cfloat>

static void fill_caddy_with_attributes(orcm_workflow_caddy_t* caddy,
                                       string nodelist, string compute,
                                       string interval, string timeout, string db)
{
    if (NULL != caddy->wf_step) {
        analytics_util::fill_attribute(&(caddy->wf_step->attributes), "nodelist", nodelist);
        analytics_util::fill_attribute(&(caddy->wf_step->attributes), "compute", compute);
        analytics_util::fill_attribute(&(caddy->wf_step->attributes), "interval", interval);
        analytics_util::fill_attribute(&(caddy->wf_step->attributes), "timeout", timeout);
        analytics_util::fill_attribute(&(caddy->wf_step->attributes), "db", db);
    }
}

static orcm_workflow_caddy_t* create_caddy_with_valid_attribute(orcm_analytics_base_module_t* mod,
                                                                orcm_workflow_t* workflow,
                                                                orcm_workflow_step_t* workflow_step,
                                                                orcm_analytics_value_t* analytics_value,
                                                                void* data_store)
{
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy(mod, workflow, workflow_step,
                                                                analytics_value, data_store);
    if (NULL != caddy) {
        fill_caddy_with_attributes(caddy, "localhost", "average", "", "", "");
    }

    return caddy;
}

static void fill_time(struct timeval** time)
{
    struct timeval current_time;
    if (NULL == *time && NULL == (*time = (struct timeval*)malloc(sizeof(struct timeval)))) {
        return;
    }

    gettimeofday(&current_time, NULL);
    (*time)->tv_sec = (&current_time)->tv_sec;
    (*time)->tv_usec = (&current_time)->tv_usec;
}

static void fill_spatial_statistics(spatial_statistics_t* spatial_statistics, int interval, int size,
                                    int compute_type, opal_event_t* timeout_event, int timeout,
                                    int fill_size, int sample_size)
{
    int idx = 1;
    int sample_idx = 0;
    spatial_statistics->interval = interval;
    spatial_statistics->size = size;
    spatial_statistics->nodelist = (char**)malloc(sizeof(char*) * (size + 1));
    spatial_statistics->nodelist[0] = strdup(analytics_util::cppstr_to_cstr("localhost"));
    opal_list_t* compute_list = NULL;
    orcm_value_t* orcm_value = NULL;
    double value = 37.5;

    for (; idx < size; idx++) {
        stringstream tmp_str;
        tmp_str << "node" << idx;
        spatial_statistics->nodelist[idx] = strdup(analytics_util::cppstr_to_cstr(tmp_str.str()));
    }
    spatial_statistics->nodelist[size] = NULL;
    spatial_statistics->compute_type = compute_type;
    fill_time(&(spatial_statistics->last_round_time));

    if (NULL != (spatial_statistics->buckets = OBJ_NEW(opal_hash_table_t))) {
        opal_hash_table_init(spatial_statistics->buckets, spatial_statistics->size);
    }

    for (idx = 0; idx < fill_size; idx++) {
        if (NULL != (compute_list = OBJ_NEW(opal_list_t))) {
            for (sample_idx = 0; sample_idx < sample_size; sample_idx++) {
                orcm_value = analytics_util::load_orcm_value("core 1", &value, OPAL_DOUBLE, "C");
                if (NULL != orcm_value) {
                    opal_list_append(compute_list, (opal_list_item_t*)orcm_value);
                }
            }
            opal_hash_table_set_value_ptr(spatial_statistics->buckets,
                                          spatial_statistics->nodelist[idx],
                                          strlen(spatial_statistics->nodelist[idx]) + 1,
                                          compute_list);
        }
    }

    if (ORCM_ANALYTICS_COMPUTE_MIN == spatial_statistics->compute_type) {
        spatial_statistics->result = DBL_MAX;
    } else if (ORCM_ANALYTICS_COMPUTE_MAX == spatial_statistics->compute_type) {
        spatial_statistics->result = DBL_MIN;
    } else {
        spatial_statistics->result = 0.0;
    }

    spatial_statistics->num_data_point = 0;
    spatial_statistics->timeout_event = timeout_event;
    spatial_statistics->timeout = timeout;
}

TEST(analytics_spatial, init_null_input)
{
    int rc = orcm_analytics_spatial_module.api.init(NULL);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(analytics_spatial, init_norm_input)
{
    int rc = ORCM_SUCCESS;
    spatial_statistics_t* spatial_data_store = NULL;

    mca_analytics_spatial_module_t* mod = NULL;
    orcm_analytics_base_module_t* base_mod = (orcm_analytics_base_module_t*)malloc(
                                              sizeof(orcm_analytics_base_module_t));
    if (NULL != base_mod) {
        rc = orcm_analytics_spatial_module.api.init(base_mod);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        mod = (mca_analytics_spatial_module_t*)base_mod;
        spatial_data_store = (spatial_statistics_t*)(mod->api.orcm_mca_analytics_data_store);
        SAFE_RELEASE(spatial_data_store);
        SAFEFREE(mod);
    }
}

TEST(analytics_spatial, finalize_norm_input_null_data)
{
    orcm_analytics_base_module_t* base_mod = (orcm_analytics_base_module_t*)malloc(
                                              sizeof(orcm_analytics_base_module_t));
    if (NULL != base_mod) {
        base_mod->orcm_mca_analytics_data_store = NULL;
        orcm_analytics_spatial_module.api.finalize(base_mod);
    }
}

TEST(analytics_spatial, finalize_norm_input)
{
    orcm_analytics_base_module_t* base_mod = (orcm_analytics_base_module_t*)malloc(
                                              sizeof(orcm_analytics_base_module_t));
    if (NULL != base_mod) {
        base_mod->orcm_mca_analytics_data_store = OBJ_NEW(spatial_statistics_t);
        orcm_analytics_spatial_module.api.finalize(base_mod);
    }
}

TEST(analytics_spatial, analyze_null_caddy)
{
    int rc = orcm_analytics_spatial_module.api.analyze(0, 0, NULL);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(analytics_spatial, analyze_null_mod)
{
    int rc = ORCM_SUCCESS;

    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy(NULL, NULL, NULL, NULL, NULL);
    if (NULL != caddy) {
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
    }
}

TEST(analytics_spatial, analyze_null_wf)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
                        malloc(sizeof(orcm_analytics_base_module_t)), NULL, NULL, NULL, NULL);
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_null_wfstep)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t), NULL, NULL, NULL);
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_null_analytics_value)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), NULL, NULL);
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_null_spatial_data_null_attribute)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), NULL);
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_spatial_data_null_attribute)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_spatial_data_attribute_null_key)
{
    int rc = ORCM_SUCCESS;
    opal_value_t* attribute = NULL;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != caddy->wf_step) {
            if (NULL != (attribute = analytics_util::load_opal_value("", "", OPAL_STRING))) {
                opal_list_append(&(caddy->wf_step->attributes), (opal_list_item_t*)attribute);
            }
        }
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_spatial_data_attribute_null_value)
{
    int rc = ORCM_SUCCESS;
    opal_value_t* attribute = NULL;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != caddy->wf_step) {
            if (NULL != (attribute = analytics_util::load_opal_value("testkey", "", OPAL_STRING))) {
                opal_list_append(&(caddy->wf_step->attributes), (opal_list_item_t*)attribute);
            }
        }
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_unnull_nodelist_invalid_compute_type)
{
    int rc = ORCM_SUCCESS;
    int idx = 0;
    orcm_analytics_base_module_t* mod = NULL;
    spatial_statistics_t* spatial_statistics = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        spatial_statistics = (spatial_statistics_t*)mod->orcm_mca_analytics_data_store;
        if (NULL != spatial_statistics) {
            spatial_statistics->nodelist = (char**)malloc(sizeof(char*) * 2);
            for (; idx < 2; idx++) {
                spatial_statistics->nodelist[idx] = NULL;
            }
        }
        fill_caddy_with_attributes(caddy, "dummynode", "unknown", "", "", "");
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_no_nodelist)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "", "average", "", "", "");
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_invalid_nodelist)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "c[1:1-10]", "", "", "", "");
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_negative_interval)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "dummynode", "average", "-5", "", "");
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_zero_interval_negative_timeout)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "dummynode", "average", "0", "-5", "");
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_zero_timeout)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "dummynode", "average", "5", "0", "");
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_positive_timeout)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "dummynode", "average", "5", "10", "");
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_null_key_analytics_value)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = create_caddy_with_valid_attribute((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        if (NULL != caddy->analytics_value) {
            caddy->analytics_value->compute_data = OBJ_NEW(opal_list_t);
        }
        mod = caddy->imod;
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_null_noncompute_analytics_value)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = create_caddy_with_valid_attribute((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != caddy->analytics_value) {
            caddy->analytics_value->compute_data = OBJ_NEW(opal_list_t);
            caddy->analytics_value->key = OBJ_NEW(opal_list_t);
        }
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_empty_key_analytics_value)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = create_caddy_with_valid_attribute((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        caddy->analytics_value = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL);
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_empty_compute_analytics_value)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = create_caddy_with_valid_attribute((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != (caddy->analytics_value = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL))) {
            opal_list_append(caddy->analytics_value->key, (opal_list_item_t*)(OBJ_NEW(orcm_value_t)));
        }
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_sample_time_within_interval)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    spatial_statistics_t* spatial_statistics = NULL;
    orcm_workflow_caddy_t* caddy = create_caddy_with_valid_attribute((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != (caddy->analytics_value = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL))) {
            opal_list_append(caddy->analytics_value->key, (opal_list_item_t*)(OBJ_NEW(orcm_value_t)));
            opal_list_append(caddy->analytics_value->compute_data, (opal_list_item_t*)(OBJ_NEW(orcm_value_t)));
        }
        spatial_statistics = (spatial_statistics_t*)mod->orcm_mca_analytics_data_store;
        fill_time(&(spatial_statistics->last_round_time));
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_RECV_MORE_THAN_POSTED, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_key_item_null_key)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = create_caddy_with_valid_attribute((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != (caddy->analytics_value = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL))) {
            opal_list_append(caddy->analytics_value->key, (opal_list_item_t*)(OBJ_NEW(orcm_value_t)));
            opal_list_append(caddy->analytics_value->compute_data, (opal_list_item_t*)(OBJ_NEW(orcm_value_t)));
        }
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_key_item_no_hostname)
{
    int rc = ORCM_SUCCESS;
    orcm_value_t* orcm_value = NULL;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = create_caddy_with_valid_attribute((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != (caddy->analytics_value = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL))) {
            orcm_value = analytics_util::load_orcm_value("randomkey", NULL, OPAL_STRING, "");
            if (NULL != orcm_value) {
                opal_list_append(caddy->analytics_value->key, (opal_list_item_t*)orcm_value);
            }
            opal_list_append(caddy->analytics_value->compute_data, (opal_list_item_t*)(OBJ_NEW(orcm_value_t)));
        }
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_key_item_null_hostname)
{
    int rc = ORCM_SUCCESS;
    orcm_value_t* orcm_value = NULL;
    orcm_analytics_base_module_t* mod = NULL;
    orcm_workflow_caddy_t* caddy = create_caddy_with_valid_attribute((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != (caddy->analytics_value = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL))) {
            orcm_value = analytics_util::load_orcm_value("hostname", NULL , OPAL_STRING, "");
            if (NULL != orcm_value) {
                opal_list_append(caddy->analytics_value->key, (opal_list_item_t*)orcm_value);
            }
            opal_list_append(caddy->analytics_value->compute_data, (opal_list_item_t*)(OBJ_NEW(orcm_value_t)));
        }
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_key_item_hostname_unmatch)
{
    int rc = ORCM_SUCCESS;
    orcm_value_t* orcm_value = NULL;
    orcm_analytics_base_module_t* mod = NULL;
    string hostname = "dummynode";
    char *c_hostname = analytics_util::cppstr_to_cstr(hostname);
    orcm_workflow_caddy_t* caddy = create_caddy_with_valid_attribute((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != (caddy->analytics_value = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL))) {
            orcm_value = analytics_util::load_orcm_value("hostname", c_hostname , OPAL_STRING, "");
            if (NULL != orcm_value) {
                opal_list_append(caddy->analytics_value->key, (opal_list_item_t*)orcm_value);
            }
            opal_list_append(caddy->analytics_value->compute_data, (opal_list_item_t*)(OBJ_NEW(orcm_value_t)));
        }
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_1node_full_bucket)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    double value = 37.5;
    orcm_workflow_caddy_t* caddy = create_caddy_with_valid_attribute((orcm_analytics_base_module_t*)
        malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        analytics_util::fill_analytics_value(caddy, "localhost", &current_time, &value, 1);
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_multinodes_1stdata_empty_eventbase)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    double value = 37.5;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "localhost,node1", "average", "", "", "");
        analytics_util::fill_analytics_value(caddy, "localhost", &current_time, &value, 1);
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERROR, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_multinodes_1stdata_valid_eventbase)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    double value = 37.5;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "localhost,node1", "average", "", "", "");
        analytics_util::fill_analytics_value(caddy, "localhost", &current_time, &value, 1);
        if (NULL != caddy->wf) {
            caddy->wf->ev_active = true;
            caddy->wf->ev_base = opal_progress_thread_init(NULL);
        }
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_RECV_MORE_THAN_POSTED, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_multinodes_1stnode_twice)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    double value = 37.5;
    spatial_statistics_t* spatial_statistics = NULL;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != mod && NULL != (spatial_statistics =
            (spatial_statistics_t*)mod->orcm_mca_analytics_data_store)) {
            fill_spatial_statistics(spatial_statistics, 1, 2, ORCM_ANALYTICS_COMPUTE_AVE,
                                    (opal_event_t*)malloc(sizeof(opal_event_t)), 10, 1, 1);
        }
        sleep(1);
        fill_caddy_with_attributes(caddy, "localhost,node1", "average", "", "", "");
        analytics_util::fill_analytics_value(caddy, "localhost", &current_time, &value, 1);
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_RECV_MORE_THAN_POSTED, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_multinodes_2nd_new_sample)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    double value = 37.5;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != mod && NULL != mod->orcm_mca_analytics_data_store) {
            fill_spatial_statistics((spatial_statistics_t*)mod->orcm_mca_analytics_data_store,
                                    1, 3, ORCM_ANALYTICS_COMPUTE_AVE,
                                    (opal_event_t*)malloc(sizeof(opal_event_t)), 10, 1, 1);
        }
        sleep(1);
        fill_caddy_with_attributes(caddy, "localhost,node1", "average", "", "", "");
        analytics_util::fill_analytics_value(caddy, "node1", &current_time, &value, 1);
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_RECV_MORE_THAN_POSTED, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_multinodes_sd_db_no)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    double value = 37.5;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != mod && NULL != mod->orcm_mca_analytics_data_store) {
            fill_spatial_statistics((spatial_statistics_t*)mod->orcm_mca_analytics_data_store,
                                    1, 3, ORCM_ANALYTICS_COMPUTE_SD, NULL, 10, 2, 1);
        }
        sleep(1);
        fill_caddy_with_attributes(caddy, "localhost,node1,node2", "sd", "1", "10", "no");
        analytics_util::fill_analytics_value(caddy, "node2", &current_time, &value, 1);
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_multinodes_min_db_yes)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    double value = 37.5;
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != mod && NULL != mod->orcm_mca_analytics_data_store) {
            fill_spatial_statistics((spatial_statistics_t*)mod->orcm_mca_analytics_data_store,
                                    1, 3, ORCM_ANALYTICS_COMPUTE_MIN, NULL, 10, 2, 1);
        }
        sleep(1);
        fill_caddy_with_attributes(caddy, "localhost,node1,node2", "min", "1", "10", "yes");
        analytics_util::fill_analytics_value(caddy, "node2", &current_time, &value, 1);
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_multinodes_timeout_max)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != mod && NULL != mod->orcm_mca_analytics_data_store) {
            fill_spatial_statistics((spatial_statistics_t*)mod->orcm_mca_analytics_data_store,
                                    1, 3, ORCM_ANALYTICS_COMPUTE_MAX, NULL, 10, 2, 1);
        }
        fill_caddy_with_attributes(caddy, "localhost,node1,node2", "max", "1", "10", "yes");
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERROR, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}

TEST(analytics_spatial, analyze_multinodes_timeout_sd)
{
    int rc = ORCM_SUCCESS;
    orcm_analytics_base_module_t* mod = NULL;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    orcm_workflow_caddy_t* caddy = analytics_util::create_caddy((orcm_analytics_base_module_t*)
      malloc(sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
      OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(spatial_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != mod && NULL != mod->orcm_mca_analytics_data_store) {
            fill_spatial_statistics((spatial_statistics_t*)mod->orcm_mca_analytics_data_store,
                                    1, 3, ORCM_ANALYTICS_COMPUTE_SD, NULL, 10, 2, 1);
        }
        fill_caddy_with_attributes(caddy, "localhost,node1,node2", "sd", "1", "10", "yes");
        if (NULL != caddy->analytics_value) {
            caddy->analytics_value->key = OBJ_NEW(opal_list_t);
            caddy->analytics_value->non_compute_data = OBJ_NEW(opal_list_t);
        }
        rc = orcm_analytics_spatial_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_spatial_module.api.finalize(mod);
    }
}
