/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "gtest/gtest.h"
#include "analytics_window_test.h"

char* cppstr_to_cstr(string str)
{
    char *cstr = NULL;

    if (!str.empty()) {
        cstr = (char*)str.c_str();
    }

    return cstr;
}

opal_value_t* load_opal_value(string key, string value, opal_data_type_t type)
{
    char *key_c = cppstr_to_cstr(key);
    void *value_c = (void*)cppstr_to_cstr(value);
    opal_value_t *opal_value = orcm_util_load_opal_value(key_c, value_c, type);
    return opal_value;
}

orcm_value_t* load_orcm_value(string key, void *data, opal_data_type_t type, string units)
{
    char *key_c = cppstr_to_cstr(key);
    char *units_c = cppstr_to_cstr(units);
    orcm_value_t *orcm_value = orcm_util_load_orcm_value(key_c, data, type, units_c);
    return orcm_value;
}

orcm_workflow_caddy_t *create_caddy(orcm_analytics_base_module_t *mod,
                                    orcm_workflow_t *workflow,
                                    orcm_workflow_step_t *workflow_step,
                                    orcm_analytics_value_t *analytics_value,
                                    void *data_store)
{
    orcm_workflow_caddy_t *caddy = OBJ_NEW(orcm_workflow_caddy_t);
    if (NULL != caddy) {
        caddy->imod = mod;
        caddy->wf = workflow;
        caddy->wf_step = workflow_step;
        caddy->analytics_value = analytics_value;
        if (NULL != caddy->imod) {
            caddy->imod->orcm_mca_analytics_data_store = data_store;
        }
    }
    return caddy;
}

static void fill_attribute(opal_list_t *list, string key, string value)
{
    opal_value_t *attribute = NULL;
    if (!value.empty()) {
        if (NULL != (attribute = load_opal_value(key, value, OPAL_STRING))) {
            opal_list_append(list, (opal_list_item_t*)attribute);
        }
    }
}

static void fill_caddy_with_attributes(orcm_workflow_caddy_t *caddy,
                                       string win_size, string compute,
                                       string unit, string type)
{
    opal_value_t *attribute = NULL;
    if (NULL != caddy->wf_step) {
        fill_attribute(&(caddy->wf_step->attributes), "win_size", win_size);
        fill_attribute(&(caddy->wf_step->attributes), "compute", compute);
        fill_attribute(&(caddy->wf_step->attributes), "unit", unit);
        fill_attribute(&(caddy->wf_step->attributes), "type", type);
    }
}

static
orcm_workflow_caddy_t *create_caddy_with_valid_attribute(orcm_analytics_base_module_t *mod,
                                                         orcm_workflow_t *workflow,
                                                         orcm_workflow_step_t *workflow_step,
                                                         orcm_analytics_value_t *analytics_value,
                                                         void *data_store)
{
    orcm_workflow_caddy_t *caddy = create_caddy(mod, workflow, workflow_step,
                                                analytics_value, data_store);
    opal_value_t *attribute = NULL;

    if (NULL != caddy) {
        fill_caddy_with_attributes(caddy, "10", "average", "", "");
    }

    return caddy;
}

static void fill_win_statistics(win_statistics_t *win_statistics, string compute_type,
                                string win_type, uint64_t num_sample_recv,
                                uint64_t num_data_point, double sum_min_max,
                                uint64_t win_left, uint64_t win_right, int win_size)
{
    char *c_compute_type = NULL;
    char *c_win_type = NULL;
    if (NULL != (c_compute_type = cppstr_to_cstr(compute_type))) {
        win_statistics->compute_type = strdup(c_compute_type);
    }
    if (NULL != (c_win_type = cppstr_to_cstr(win_type))) {
        win_statistics->win_type = strdup(c_win_type);
    }
    win_statistics->num_sample_recv = num_sample_recv;
    win_statistics->num_data_point = num_data_point;
    win_statistics->sum_min_max = sum_min_max;
    win_statistics->win_left = win_left;
    win_statistics->win_right = win_right;
    win_statistics->win_size = win_size;
}

static void fill_analytics_value(orcm_workflow_caddy_t *caddy,
                                 struct timeval *time, double *value, int count)
{
    int index = -1;
    orcm_value_t *orcm_value = NULL;
    string hostname = "localhost";
    char *c_hostname = cppstr_to_cstr(hostname);
    caddy->analytics_value = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL);

    if (NULL != caddy->analytics_value) {
        if (NULL != (orcm_value = load_orcm_value("hostname", c_hostname, OPAL_STRING, ""))) {
            opal_list_append(caddy->analytics_value->key, (opal_list_item_t*)orcm_value);
        }
        if (NULL != (orcm_value = load_orcm_value("ctime", time, OPAL_TIMEVAL, ""))) {
            opal_list_append(caddy->analytics_value->non_compute_data,
                             (opal_list_item_t*)orcm_value);
        }

        for (index = 0; index < count; index++) {
            if (NULL != (orcm_value = load_orcm_value("core 1", value, OPAL_DOUBLE, "C"))) {
                opal_list_append(caddy->analytics_value->compute_data,
                                 (opal_list_item_t*)orcm_value);
            }
        }
    }
}

TEST(analytics_window, init_null_input)
{
    int rc = -1;

    rc = orcm_analytics_window_module.api.init(NULL);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(analytics_window, init_norm_input)
{
    int rc = -1;
    win_statistics_t *win_data_store = NULL;

    mca_analytics_window_module_t *mod = NULL;
    orcm_analytics_base_module_t *base_mod = (orcm_analytics_base_module_t*)malloc(
                                          sizeof(orcm_analytics_base_module_t));
    if (NULL != base_mod) {
        rc = orcm_analytics_window_module.api.init(base_mod);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        mod = (mca_analytics_window_module_t*)base_mod;
        win_data_store = (win_statistics_t*)(mod->api.orcm_mca_analytics_data_store);
        if (NULL != win_data_store) {
            OBJ_RELEASE(win_data_store);
        }
        free(mod);
    }
}

TEST(analytics_window, finalize_null_input)
{
    orcm_analytics_window_module.api.finalize(NULL);
}

TEST(analytics_window, finalize_norm_input_null_data)
{
    orcm_analytics_base_module_t *base_mod = (orcm_analytics_base_module_t*)malloc(
                                                  sizeof(orcm_analytics_base_module_t));
    if (NULL != base_mod) {
        base_mod->orcm_mca_analytics_data_store = NULL;
        orcm_analytics_window_module.api.finalize(base_mod);
    }
}

TEST(analytics_window, finalize_norm_input)
{
    int rc = -1;
    orcm_analytics_base_module_t *base_mod = (orcm_analytics_base_module_t*)malloc(
                                                  sizeof(orcm_analytics_base_module_t));

    if (NULL != base_mod) {
        base_mod->orcm_mca_analytics_data_store = OBJ_NEW(win_statistics_t);
        orcm_analytics_window_module.api.finalize(base_mod);
    }
}

TEST(analytics_window, analyze_null_caddy)
{
    int rc = orcm_analytics_window_module.api.analyze(0, 0, NULL);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(analytics_window, analyze_null_mod)
{
    int rc = -1;

    orcm_workflow_caddy_t *caddy = create_caddy(NULL, NULL, NULL, NULL, NULL);
    if (NULL != caddy) {
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
    }
}

TEST(analytics_window, analyze_null_wf)
{
    int rc = -1;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                           sizeof(orcm_analytics_base_module_t)), NULL, NULL, NULL, NULL);
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_null_wfstep)
{
    int rc = -1;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
       sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t), NULL, NULL, NULL);
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_null_analytics_value)
{
    int rc = -1;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                           sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                           OBJ_NEW(orcm_workflow_step_t), NULL, NULL);
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_null_win_data_null_attribute)
{
    int rc = -1;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                       sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                       OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), NULL);
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_win_data_null_attribute)
{
    int rc = -1;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
        sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_win_data_attribute_null_key)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
        sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != caddy->wf_step) {
            if (NULL != (attribute = load_opal_value("", "", OPAL_STRING))) {
                opal_list_append(&(caddy->wf_step->attributes), (opal_list_item_t*)attribute);
            }
        }
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_win_data_attribute_null_value)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
        sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != caddy->wf_step) {
            if (NULL != (attribute = load_opal_value("testkey", "", OPAL_STRING))) {
                opal_list_append(&(caddy->wf_step->attributes), (opal_list_item_t*)attribute);
            }
        }
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_zero_win_size)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
        sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "0", "", "", "");
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_null_compute_type)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
        sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "10", "", "", "");
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_unknown_compute_type)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
        sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "10", "unknown", "", "");
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_unknown_win_type)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
        sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "10", "", "", "unknown");
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_unknown_win_unit)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
        sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
        OBJ_NEW(orcm_workflow_step_t), OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        fill_caddy_with_attributes(caddy, "10", "", "weeks", "");
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_null_key_analytics_value)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy_with_valid_attribute(
        (orcm_analytics_base_module_t*)malloc(sizeof(orcm_analytics_base_module_t)),
        OBJ_NEW(orcm_workflow_t), OBJ_NEW(orcm_workflow_step_t),
        OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_null_noncompute_analytics_value)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy_with_valid_attribute(
        (orcm_analytics_base_module_t*)malloc(sizeof(orcm_analytics_base_module_t)),
        OBJ_NEW(orcm_workflow_t), OBJ_NEW(orcm_workflow_step_t),
        OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != caddy->analytics_value) {
            caddy->analytics_value->key = OBJ_NEW(opal_list_t);
        }
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_null_compute_analytics_value)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy_with_valid_attribute(
        (orcm_analytics_base_module_t*)malloc(sizeof(orcm_analytics_base_module_t)),
        OBJ_NEW(orcm_workflow_t), OBJ_NEW(orcm_workflow_step_t),
        OBJ_NEW(orcm_analytics_value_t), OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != caddy->analytics_value) {
            caddy->analytics_value->key = OBJ_NEW(opal_list_t);
            caddy->analytics_value->non_compute_data = OBJ_NEW(opal_list_t);
        }
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_empty_key_analytics_value)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy_with_valid_attribute(
        (orcm_analytics_base_module_t*)malloc(sizeof(orcm_analytics_base_module_t)),
        OBJ_NEW(orcm_workflow_t), OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        caddy->analytics_value = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL);
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_empty_compute_analytics_value)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy_with_valid_attribute(
        (orcm_analytics_base_module_t*)malloc(sizeof(orcm_analytics_base_module_t)),
        OBJ_NEW(orcm_workflow_t), OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));
    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != (caddy->analytics_value = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL))) {
            opal_list_append(caddy->analytics_value->key, (opal_list_item_t*)(OBJ_NEW(orcm_value_t)));
        }
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_time_win_notime)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy_with_valid_attribute(
        (orcm_analytics_base_module_t*)malloc(sizeof(orcm_analytics_base_module_t)),
        OBJ_NEW(orcm_workflow_t), OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));

    if (NULL != caddy) {
        mod = caddy->imod;
        if (NULL != (caddy->analytics_value = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL))) {
            opal_list_append(caddy->analytics_value->key, (opal_list_item_t*)(OBJ_NEW(orcm_value_t)));
            caddy->analytics_value->non_compute_data = OBJ_NEW(opal_list_t);
            opal_list_append(caddy->analytics_value->compute_data, (opal_list_item_t*)(OBJ_NEW(orcm_value_t)));
        }
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_time_average_first)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                          sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));
    win_statistics_t *win_statistics = NULL;
    orcm_value_t *orcm_value = NULL;
    double value = 37.5;
    struct timeval time;
    gettimeofday(&time, NULL);

    if (NULL != caddy) {
        if (NULL != caddy->imod) {
            win_statistics = (win_statistics_t*)(caddy->imod->orcm_mca_analytics_data_store);
            mod = caddy->imod;
        }
        fill_caddy_with_attributes(caddy, "10", "average", "min", "");
        fill_analytics_value(caddy, &time, &value, 1);
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(win_statistics->sum_min_max, 37.5);
        ASSERT_EQ(win_statistics->num_sample_recv, 1);
        ASSERT_EQ(win_statistics->win_size, 600);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_time_min_first)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                          sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));
    win_statistics_t *win_statistics = NULL;
    orcm_value_t *orcm_value = NULL;
    double value = 37.5;
    struct timeval time;
    gettimeofday(&time, NULL);

    if (NULL != caddy) {
        if (NULL != caddy->imod) {
            win_statistics = (win_statistics_t*)(caddy->imod->orcm_mca_analytics_data_store);
            mod = caddy->imod;
        }
        fill_caddy_with_attributes(caddy, "10", "min", "hour", "");
        fill_analytics_value(caddy, &time, &value, 1);
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(win_statistics->sum_min_max, 37.5);
        ASSERT_EQ(win_statistics->num_sample_recv, 1);
        ASSERT_EQ(win_statistics->win_size, 36000);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_time_max_first)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                          sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));
    win_statistics_t *win_statistics = NULL;
    orcm_value_t *orcm_value = NULL;
    double value = 37.5;
    struct timeval time;
    gettimeofday(&time, NULL);

    if (NULL != caddy) {
        if (NULL != caddy->imod) {
            win_statistics = (win_statistics_t*)(caddy->imod->orcm_mca_analytics_data_store);
            mod = caddy->imod;
        }
        fill_caddy_with_attributes(caddy, "10", "max", "day", "");
        fill_analytics_value(caddy, &time, &value, 1);
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(win_statistics->sum_min_max, 37.5);
        ASSERT_EQ(win_statistics->num_sample_recv, 1);
        ASSERT_EQ(win_statistics->win_size, 864000);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_time_sd_first)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                          sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));
    win_statistics_t *win_statistics = NULL;
    orcm_value_t *orcm_value = NULL;
    double value = 37.5;
    struct timeval time;
    gettimeofday(&time, NULL);

    if (NULL != caddy) {
        if (NULL != caddy->imod) {
            win_statistics = (win_statistics_t*)(caddy->imod->orcm_mca_analytics_data_store);
            mod = caddy->imod;
        }
        fill_caddy_with_attributes(caddy, "10", "sd", "", "");
        fill_analytics_value(caddy, &time, &value, 1);
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(win_statistics->sum_min_max, 37.5);
        ASSERT_EQ(win_statistics->sum_square, 37.5 * 37.5);
        ASSERT_EQ(win_statistics->num_sample_recv, 1);
        ASSERT_EQ(win_statistics->win_size, 10);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_time_average_follow_inbound)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                          sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));
    win_statistics_t *win_statistics = NULL;
    orcm_value_t *orcm_value = NULL;
    double value = 37.5;
    struct timeval time = {1500, 0};

    if (NULL != caddy) {
        if (NULL != caddy->imod && NULL != (win_statistics = (win_statistics_t*)
            (caddy->imod->orcm_mca_analytics_data_store))) {
            fill_win_statistics(win_statistics, "average", "time",
                                1000, 1000, 123456.7, 1000, 2000, 1000);
            mod = caddy->imod;
        }
        fill_analytics_value(caddy, &time, &value, 1);
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(win_statistics->sum_min_max, 123494.2);
        ASSERT_EQ(win_statistics->num_sample_recv, 1001);
        ASSERT_EQ(win_statistics->num_data_point, 1001);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_time_average_follow_outlowerbound)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                          sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));
    win_statistics_t *win_statistics = NULL;
    orcm_value_t *orcm_value = NULL;
    double value = 37.5;
    struct timeval time = {900, 0};

    if (NULL != caddy) {
        if (NULL != caddy->imod && NULL != (win_statistics = (win_statistics_t*)
            (caddy->imod->orcm_mca_analytics_data_store))) {
            fill_win_statistics(win_statistics, "average", "time",
                                1000, 1000, 123456.7, 1000, 2000, 1000);
            mod = caddy->imod;
        }
        fill_analytics_value(caddy, &time, &value, 1);
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(win_statistics->sum_min_max, 123456.7);
        ASSERT_EQ(win_statistics->num_sample_recv, 1000);
        ASSERT_EQ(win_statistics->num_data_point, 1000);
        ASSERT_EQ(ORCM_ERR_RECV_MORE_THAN_POSTED, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_time_average_follow_outupperbound1)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                          sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));
    win_statistics_t *win_statistics = NULL;
    orcm_value_t *orcm_value = NULL;
    double value = 37.5;
    struct timeval time = {2500, 0};

    if (NULL != caddy) {
        if (NULL != caddy->imod && NULL != (win_statistics = (win_statistics_t*)
            (caddy->imod->orcm_mca_analytics_data_store))) {
            fill_win_statistics(win_statistics, "average", "time",
                                1000, 1000, 123456.7, 1000, 2000, 1000);
            mod = caddy->imod;
        }
        fill_analytics_value(caddy, &time, &value, 1);
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(win_statistics->sum_min_max, 37.5);
        ASSERT_EQ(win_statistics->num_sample_recv, 1);
        ASSERT_EQ(win_statistics->num_data_point, 1);
        ASSERT_EQ(win_statistics->win_left, 2000);
        ASSERT_EQ(win_statistics->win_right, 3000);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_time_average_follow_outupperbound2)
{
    int rc = -1;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                          sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));
    win_statistics_t *win_statistics = NULL;
    orcm_value_t *orcm_value = NULL;
    double value = 37.5;
    struct timeval time = {3100, 0};

    if (NULL != caddy) {
        if (NULL != caddy->imod && NULL != (win_statistics = (win_statistics_t*)
            (caddy->imod->orcm_mca_analytics_data_store))) {
            fill_win_statistics(win_statistics, "average", "time",
                                1000, 1000, 123456.7, 1000, 2000, 1000);
            mod = caddy->imod;
        }
        fill_analytics_value(caddy, &time, &value, 1);
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(win_statistics->sum_min_max, 37.5);
        ASSERT_EQ(win_statistics->num_sample_recv, 1);
        ASSERT_EQ(win_statistics->num_data_point, 1);
        ASSERT_EQ(win_statistics->win_left, 3000);
        ASSERT_EQ(win_statistics->win_right, 4000);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_counter_average_follow_allinbound)
{
    int rc = -1, index = 0;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                          sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));
    win_statistics_t *win_statistics = NULL;
    orcm_value_t *orcm_value = NULL;
    double value = 37.5;
    struct timeval time = {3100, 0};

    if (NULL != caddy) {
        if (NULL != caddy->imod && NULL != (win_statistics = (win_statistics_t*)
            (caddy->imod->orcm_mca_analytics_data_store))) {
            fill_win_statistics(win_statistics, "average", "counter",
                                1000, 1000, 123456.7, 0, 2000, 2000);
            mod = caddy->imod;
        }
        fill_analytics_value(caddy, &time, &value, 1000);
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(win_statistics->sum_min_max, 160956.7);
        ASSERT_EQ(win_statistics->num_sample_recv, 1001);
        ASSERT_EQ(win_statistics->num_data_point, 2000);
        ASSERT_EQ(win_statistics->win_left, 0);
        ASSERT_EQ(win_statistics->win_right, 2000);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

TEST(analytics_window, analyze_counter_average_follow_someoutbound)
{
    int rc = -1, index = 0;
    opal_value_t *attribute = NULL;
    orcm_analytics_base_module_t *mod = NULL;
    orcm_workflow_caddy_t *caddy = create_caddy((orcm_analytics_base_module_t*)malloc(
                          sizeof(orcm_analytics_base_module_t)), OBJ_NEW(orcm_workflow_t),
                          OBJ_NEW(orcm_workflow_step_t), NULL, OBJ_NEW(win_statistics_t));
    win_statistics_t *win_statistics = NULL;
    orcm_value_t *orcm_value = NULL;
    double value = 37.5;
    struct timeval time = {3100, 0};

    if (NULL != caddy) {
        if (NULL != caddy->imod && NULL != (win_statistics = (win_statistics_t*)
            (caddy->imod->orcm_mca_analytics_data_store))) {
            fill_win_statistics(win_statistics, "average", "counter",
                                1999, 1999, 123456.7, 0, 2000, 2000);
            mod = caddy->imod;
        }
        fill_analytics_value(caddy, &time, &value, 1500);
        rc = orcm_analytics_window_module.api.analyze(0, 0, caddy);
        ASSERT_EQ(win_statistics->sum_min_max, 0);
        ASSERT_EQ(win_statistics->num_sample_recv, 0);
        ASSERT_EQ(win_statistics->num_data_point, 0);
        ASSERT_EQ(win_statistics->win_left, 0);
        ASSERT_EQ(win_statistics->win_right, 2000);
        ASSERT_EQ(ORCM_SUCCESS, rc);
        orcm_analytics_window_module.api.finalize(mod);
    }
}

