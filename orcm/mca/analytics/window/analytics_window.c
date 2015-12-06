/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdio.h>
#include <ctype.h>
#include <float.h>

#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/analytics/base/analytics_private.h"
#include "analytics_window.h"
#include "orcm/util/utils.h"
#include "math.h"

/* constructor of the win_statistics_t data structure */
static void win_statistics_con(win_statistics_t *win_stat)
{
    win_stat->win_size = 0;
    win_stat->win_unit = NULL;
    win_stat->win_left = 0;
    win_stat->win_right = 0;
    win_stat->num_sample_recv = 0;
    win_stat->sum_min_max = 0.0;
    win_stat->sum_square = 0.0;
    win_stat->compute_type = NULL;
    win_stat->win_type = NULL;
}

/* destructor of the win_statistics_t data structure */
static void win_statistics_des(win_statistics_t *win_stat)
{
    SAFEFREE(win_stat->win_unit);
    SAFEFREE(win_stat->compute_type);
    SAFEFREE(win_stat->win_type);
}

OBJ_CLASS_INSTANCE(win_statistics_t, opal_object_t,
                   win_statistics_con, win_statistics_des);

static int init(orcm_analytics_base_module_t *imod);
static void finalize(orcm_analytics_base_module_t *imod);
static int analyze(int sd, short args, void *cbdata);

mca_analytics_window_module_t orcm_analytics_window_module = {
    {
        init,
        finalize,
        analyze,
        NULL
    }
};

/* function to reset the window statistics */
static void reset_window(win_statistics_t *win_statistics, uint64_t left, uint64_t incre);

/* function to assert that the window statistics data structure is filled right */
static int fill_win_statistics(win_statistics_t *win_statistics);

/* function to fill the attributes of the window plugin to the window statistics */
static int fill_attributes(win_statistics_t *win_statistics, opal_list_t *attributes);

/* function to accumulate the data sample to compute average */
static void accumulate_data_average(win_statistics_t *win_statistics, double num);

/* function to accumulate the data sample to get the min */
static void accumulate_data_min(win_statistics_t *win_statistics, double num);

/* function to accumulate the data sample to get the max */
static void accumulate_data_max(win_statistics_t *win_statistics, double num);

/* function to accumulate the data sample to compute standard deviation */
static void accumulate_data_sd(win_statistics_t *win_statistics, double num);

/* function to accumulate the data samples from a list to do computation */
static int accumulate_data(win_statistics_t *win_statistics,
                           opal_list_t *data_list, int start, int end);

/* function to compute the results based on the compute type */
static double computing(win_statistics_t *win_statistics);

/* function to print out the data statistics */
static void print_out_results(win_statistics_t *win_statistics,
                              orcm_workflow_caddy_t *caddy, double result);

/* function to handle the full window: do computation, send data to the next plugin */
static int handle_full_window(win_statistics_t *win_statistics, orcm_workflow_caddy_t *caddy);

/* function to do the computation for the time window */
static int do_compute_time_window(win_statistics_t *win_statistics,
                                  orcm_workflow_caddy_t *caddy);

/* function to do the computation for the counter window */
static int do_compute_counter_window(win_statistics_t *win_statistics,
                                     orcm_workflow_caddy_t *caddy);

/* function to do the computation */
static int do_compute(win_statistics_t *win_statistics, orcm_workflow_caddy_t *caddy);

/* function to fill the window statistics for the first data sample */
static int fill_first_sample(win_statistics_t *win_statistics, orcm_workflow_caddy_t *caddy);

static int init(orcm_analytics_base_module_t *imod)
{
    mca_analytics_window_module_t *mod = NULL;

    if (NULL == imod) {
        return ORCM_ERR_BAD_PARAM;
    }

    mod = (mca_analytics_window_module_t *)imod;
    if (NULL == (mod->api.orcm_mca_analytics_data_store = OBJ_NEW(win_statistics_t))) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    return ORCM_SUCCESS;
}

static void finalize(orcm_analytics_base_module_t *imod)
{
    mca_analytics_window_module_t *mod = NULL;
    win_statistics_t *win_data_store = NULL;
    if (NULL != imod) {
        mod = (mca_analytics_window_module_t *)imod;
        win_data_store = (win_statistics_t*)(mod->api.orcm_mca_analytics_data_store);
        if (NULL != win_data_store) {
            OBJ_RELEASE(win_data_store);
        }
        free(mod);
    }
}

static void reset_window(win_statistics_t *win_statistics, uint64_t left, uint64_t incre)
{
    win_statistics->win_left = left;
    win_statistics->win_right = left + incre;
    win_statistics->num_sample_recv = 0;

    if (0 == strncmp(win_statistics->compute_type, "min", strlen("min"))) {
        win_statistics->sum_min_max = DBL_MAX;
    } else if (0 == strncmp(win_statistics->compute_type, "max", strlen("max"))) {
        win_statistics->sum_min_max = DBL_MIN;
    } else {
        win_statistics->sum_min_max = 0.0;
    }
    win_statistics->sum_square = 0.0;
}

static int fill_win_statistics(win_statistics_t *win_statistics)
{
    if (0 >= win_statistics->win_size) {
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == win_statistics->win_type) {
        win_statistics->win_type = strdup("time");
    } else if (0 != strncmp(win_statistics->win_type, "time", strlen("time")) &&
               0 != strncmp(win_statistics->win_type, "counter", strlen("counter"))) {
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == win_statistics->compute_type ||
        (0 != strncmp(win_statistics->compute_type, "average", strlen("average")) &&
         0 != strncmp(win_statistics->compute_type, "min", strlen("min")) &&
         0 != strncmp(win_statistics->compute_type, "max", strlen("max")) &&
         0 != strncmp(win_statistics->compute_type, "sd", strlen("sd")))) {
        return ORCM_ERR_BAD_PARAM;
    }

    if (0 == strncmp(win_statistics->win_type, "time", strlen("time"))) {
        if (NULL != win_statistics->win_unit) {
            if (0 == strncmp(win_statistics->win_unit, "min", strlen("min"))) {
                win_statistics->win_size *= 60;
            } else if (0 == strncmp(win_statistics->win_unit, "hour", strlen("hour"))) {
                win_statistics->win_size *= 3600;
            } else if (0 == strncmp(win_statistics->win_unit, "day", strlen("day"))) {
                win_statistics->win_size *= 86400;
            } else if (0 != strncmp(win_statistics->win_unit, "sec", strlen("sec"))){
                return ORCM_ERR_BAD_PARAM;
            }
        }
        reset_window(win_statistics, 0, 0);
    } else {
        reset_window(win_statistics, 0, win_statistics->win_size);
    }

    return ORCM_SUCCESS;
}

static int fill_attributes(win_statistics_t *win_statistics, opal_list_t *attributes)
{
    int rc = ORCM_SUCCESS;
    opal_value_t *one_attribute = NULL;

    if (NULL == attributes) {
        return ORCM_ERR_BAD_PARAM;
    }

    OPAL_LIST_FOREACH(one_attribute, attributes, opal_value_t) {
        if (NULL == one_attribute || NULL == one_attribute->key ||
            NULL == one_attribute->data.string) {
            return ORCM_ERR_BAD_PARAM;
        } else if (0 == strncmp(one_attribute->key, "win_size", strlen(one_attribute->key) + 1)) {
            win_statistics->win_size = (int)strtol(one_attribute->data.string, NULL, 10);
        } else if (0 == strncmp(one_attribute->key, "unit", strlen(one_attribute->key) + 1)) {
            if (NULL == win_statistics->win_unit) {
                win_statistics->win_unit = strdup(one_attribute->data.string);
            }
        } else if (0 == strncmp(one_attribute->key, "compute", strlen(one_attribute->key) + 1)) {
            if (NULL == win_statistics->compute_type) {
                win_statistics->compute_type = strdup(one_attribute->data.string);
            }
        } else if (0 == strncmp(one_attribute->key, "type", strlen(one_attribute->key) + 1)) {
            if (NULL == win_statistics->win_type) {
                win_statistics->win_type = strdup(one_attribute->data.string);
            }
        }
    }

    rc = fill_win_statistics(win_statistics);

    return rc;
}

static void accumulate_data_average(win_statistics_t *win_statistics, double num)
{
    win_statistics->sum_min_max += num;
}

static void accumulate_data_min(win_statistics_t *win_statistics, double num)
{
    if (num < win_statistics->sum_min_max) {
        win_statistics->sum_min_max = num;
    }
}

static void accumulate_data_max(win_statistics_t *win_statistics, double num)
{
    if (num > win_statistics->sum_min_max) {
        win_statistics->sum_min_max = num;
    }
}

static void accumulate_data_sd(win_statistics_t *win_statistics, double num)
{
    accumulate_data_average(win_statistics, num);
    win_statistics->sum_square += pow(num, 2);
}

static int accumulate_data(win_statistics_t *win_statistics,
                           opal_list_t *data_list, int start, int end)
{
    int index = -1;
    double num = 0.0;
    orcm_value_t *data_item = NULL;

    OPAL_LIST_FOREACH(data_item, data_list, orcm_value_t) {
        if (NULL == data_item) {
            return ORCM_ERR_BAD_PARAM;
        }
        index++;
        if (index < start) {
            continue;
        }
        if (index > end) {
            break;
        }
        win_statistics->num_sample_recv++;
        num = orcm_util_get_number_orcm_value(data_item);
        if (0 == strncmp(win_statistics->compute_type, "average", strlen("average"))) {
            accumulate_data_average(win_statistics, num);
        } else if (0 == strncmp(win_statistics->compute_type, "min", strlen("min"))) {
            accumulate_data_min(win_statistics, num);
        } else if (0 == strncmp(win_statistics->compute_type, "max", strlen("max"))) {
            accumulate_data_max(win_statistics, num);
        } else {
            accumulate_data_sd(win_statistics, num);
        }
    }

    return ORCM_SUCCESS;
}

static double computing(win_statistics_t *win_statistics)
{
    double result = 0.0, temp = 0.0;

    if (0 == strncmp(win_statistics->compute_type, "average", strlen("average"))) {
        result = win_statistics->sum_min_max / win_statistics->num_sample_recv;
    } else if (0 == strncmp(win_statistics->compute_type, "min", strlen("min")) ||
               0 == strncmp(win_statistics->compute_type, "max", strlen("max"))) {
        result = win_statistics->sum_min_max;
    } else if (1 < win_statistics->num_sample_recv) {
        temp = win_statistics->num_sample_recv * win_statistics->sum_square -
               pow(win_statistics->sum_min_max, 2);
        if (0 <= temp) {
            result = sqrt(temp / (win_statistics->num_sample_recv *
                          (win_statistics->num_sample_recv - 1)));
        }
    }

    return result;
}

static void print_out_results(win_statistics_t *win_statistics,
                              orcm_workflow_caddy_t *caddy, double result)
{
    printf("\n\nThe window is full, below are the statistics:\n");
    printf("Workflow id:\tworkflow %d\n", caddy->wf->workflow_id);
    printf("Window size:\t%d\n", win_statistics->win_size);
    printf("Window type:\t%s\n", win_statistics->win_type);
    printf("Window compute type:\t%s\n", win_statistics->compute_type);
    printf("Window results:\t%f\n\n\n", result);
}

static int handle_full_window(win_statistics_t *win_statistics, orcm_workflow_caddy_t *caddy)
{
    orcm_analytics_value_t *analytics_value_to_next = NULL;
    opal_list_t *compute_list_to_next = NULL;
    orcm_value_t *compute_list_item_to_next = NULL;
    orcm_value_t *compute_list_item_current = NULL;
    double result = 0.0;

    if (0 >= win_statistics->num_sample_recv) {
        return ORCM_SUCCESS;
    }

    compute_list_item_current = (orcm_value_t*)opal_list_get_first(
                                caddy->analytics_value->compute_data);
    if (NULL == (compute_list_to_next = OBJ_NEW(opal_list_t))) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    result = computing(win_statistics);

    print_out_results(win_statistics, caddy, result);

    compute_list_item_to_next = orcm_util_load_orcm_value(compute_list_item_current->value.key,
              &result, compute_list_item_current->value.type, compute_list_item_current->units);
    if (NULL == compute_list_item_to_next) {
        OPAL_LIST_RELEASE(compute_list_to_next);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    opal_list_append(compute_list_to_next, (opal_list_item_t *)compute_list_item_to_next);
    analytics_value_to_next = orcm_util_load_orcm_analytics_value_compute(
                              caddy->analytics_value->key,
                              caddy->analytics_value->non_compute_data, compute_list_to_next);
    if (NULL != analytics_value_to_next) {
        if(ORCM_SUCCESS != orcm_analytics_base_log_to_database_event(analytics_value_to_next)){
            return ORCM_ERROR;
        }
        ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(caddy->wf, caddy->wf_step, caddy->hash_key,
                                         analytics_value_to_next);
        return ORCM_SUCCESS;
    }
    OPAL_LIST_RELEASE(compute_list_to_next);
    return ORCM_ERR_OUT_OF_RESOURCE;
}

static int do_compute_time_window(win_statistics_t *win_statistics,
                                  orcm_workflow_caddy_t *caddy)
{
    int rc = ORCM_SUCCESS;
    uint64_t sample_time = 0;
    opal_list_t *compute_data = caddy->analytics_value->compute_data;

    rc = orcm_analytics_base_get_sample_time(caddy->analytics_value->non_compute_data, &sample_time);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    if (sample_time < win_statistics->win_left) {
        return ORCM_ERR_RECV_MORE_THAN_POSTED;
    } else if (win_statistics->win_left == win_statistics->win_right) {
        win_statistics->win_left = sample_time;
        win_statistics->win_right = sample_time + win_statistics->win_size;
    } else if (sample_time >= win_statistics->win_right) {
        if (ORCM_SUCCESS != (rc = handle_full_window(win_statistics, caddy))) {
            return rc;
        }
        if (sample_time >= win_statistics->win_right + win_statistics->win_size) {
            reset_window(win_statistics, sample_time - (sample_time % win_statistics->win_size),
                         win_statistics->win_size);
        } else {
            reset_window(win_statistics, win_statistics->win_right, win_statistics->win_size);
        }
    }

    return accumulate_data(win_statistics, compute_data, 0, opal_list_get_size(compute_data) - 1);
}

static int do_compute_counter_window(win_statistics_t *win_statistics,
                                     orcm_workflow_caddy_t *caddy)
{
    int rc = -1, start = 0, end = 0, count = 0, more = 0;
    opal_list_t *compute_data = caddy->analytics_value->compute_data;
    count = opal_list_get_size(compute_data);

    while (end < count) {
        more = win_statistics->win_right - win_statistics->num_sample_recv;
        start = end;
        if (more >= count - end) {
            end = count;
        } else {
            end += more;
        }
        rc = accumulate_data(win_statistics, compute_data, start, end - 1);
        if (ORCM_SUCCESS != rc) {
            return rc;
        }
        if (win_statistics->num_sample_recv >= win_statistics->win_right) {
            if (ORCM_SUCCESS != (rc = handle_full_window(win_statistics, caddy))) {
                return rc;
            }
            reset_window(win_statistics, 0, win_statistics->win_size);
        }
    }

    return ORCM_SUCCESS;
}

static int do_compute(win_statistics_t *win_statistics, orcm_workflow_caddy_t *caddy)
{
    orcm_analytics_value_t *analytics_value = caddy->analytics_value;

    if (NULL == analytics_value->key || NULL == analytics_value->non_compute_data ||
        NULL == analytics_value->compute_data || 0 == opal_list_get_size(analytics_value->key) ||
        0 == opal_list_get_size(analytics_value->compute_data)) {
        return ORCM_ERR_BAD_PARAM;
    }

    if (0 == strncmp(win_statistics->win_type, "time", strlen("time"))) {
        return do_compute_time_window(win_statistics, caddy);
    }

    return do_compute_counter_window(win_statistics, caddy);
}

static int fill_first_sample(win_statistics_t *win_statistics, orcm_workflow_caddy_t *caddy)
{
    int rc = ORCM_SUCCESS;
    if (ORCM_SUCCESS != (rc = fill_attributes(win_statistics, &(caddy->wf_step->attributes)))) {
        return rc;
    }

    return do_compute(win_statistics, caddy);
}

static int analyze(int sd, short args, void *cbdata)
{
    int rc = -1;
    orcm_workflow_caddy_t *caddy = (orcm_workflow_caddy_t *)cbdata;
    mca_analytics_window_module_t *mod = NULL;
    win_statistics_t *win_statistics = NULL;

    if (ORCM_SUCCESS != (rc = orcm_analytics_base_assert_caddy_data(cbdata))) {
        goto cleanup;
    }

    mod = (mca_analytics_window_module_t *)caddy->imod;
    if (NULL == (win_statistics = (win_statistics_t *)(mod->api.orcm_mca_analytics_data_store))) {
        mod->api.orcm_mca_analytics_data_store = OBJ_NEW(win_statistics_t);
        if (NULL == mod->api.orcm_mca_analytics_data_store) {
            rc = ORCM_ERR_OUT_OF_RESOURCE;
            goto cleanup;
        }
        win_statistics = (win_statistics_t *)(mod->api.orcm_mca_analytics_data_store);
    }
    if (0 == win_statistics->num_sample_recv) {
        rc = fill_first_sample(win_statistics, caddy);
    } else {
        rc = do_compute(win_statistics, caddy);
    }

cleanup:
    if (NULL != caddy) {
        OBJ_RELEASE(caddy);
    }
    return rc;
}
