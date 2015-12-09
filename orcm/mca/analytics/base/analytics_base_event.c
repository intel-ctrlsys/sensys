/*
 * Copyright (c) 2014-2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/util/utils.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/evgen/evgen.h"

#define ARRAY_SIZE(a) sizeof(a)/sizeof(*a)

typedef struct {
    const char *cat_string;
    unsigned int cat_int;
} orcm_analytics_event_cat_types_t;

int orcm_analytics_base_event_set_reporter(orcm_ras_event_t *analytics_event_data, char *key,
                                           void *data, opal_data_type_t type, char *units)
{
    orcm_value_t *analytics_orcm_value = NULL;

    analytics_orcm_value = orcm_util_load_orcm_value(key, data, type, units);
    if (NULL == analytics_orcm_value) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    opal_list_append(&(analytics_event_data->reporter), (opal_list_item_t *)analytics_orcm_value);

    return ORCM_SUCCESS;
}

int orcm_analytics_base_event_set_description(orcm_ras_event_t *analytics_event_data, char *key,
                                              void *data, opal_data_type_t type, char *units)
{
    orcm_value_t *analytics_orcm_value = NULL;

    analytics_orcm_value = orcm_util_load_orcm_value(key, data, type, units);
    if (NULL == analytics_orcm_value) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    opal_list_append(&(analytics_event_data->description), (opal_list_item_t *)analytics_orcm_value);

    return ORCM_SUCCESS;
}

static void orcm_analytics_base_event_cleanup(void *cbdata)
{
    orcm_ras_event_t *analytics_event_data;

    if (NULL == cbdata) {
        return;
    }

    analytics_event_data = (orcm_ras_event_t *)cbdata;
    OBJ_RELEASE(analytics_event_data);
}

static time_t orcm_analytics_base_event_set_time(opal_list_t *src_list)
{
    orcm_value_t *list_item = NULL;

    OPAL_LIST_FOREACH(list_item, src_list, orcm_value_t) {
        if (NULL == list_item) {
            return 0;
        }
        if (0 == strcmp("ctime", list_item->value.key )) {
            return list_item->value.data.tv.tv_sec;
        }
    }
    return 0;
}


orcm_ras_event_t* orcm_analytics_base_event_create(orcm_analytics_value_t *analytics_data, int type, int severity)
{
    orcm_ras_event_t *analytics_event_data = OBJ_NEW(orcm_ras_event_t);

    if (NULL != analytics_event_data) {

        if (ORCM_SUCCESS != orcm_util_copy_list_items(analytics_data->key, &(analytics_event_data->reporter))) {
            OBJ_RELEASE(analytics_event_data);
            return NULL;
        }

        if (ORCM_SUCCESS != orcm_util_copy_list_items(analytics_data->compute_data, &(analytics_event_data->data))) {
            OBJ_RELEASE(analytics_event_data);
            return NULL;
        }

        analytics_event_data->timestamp = orcm_analytics_base_event_set_time(analytics_data->non_compute_data);

        /*Don't copy the timestamp again */
        if (!(analytics_event_data->timestamp > 0 && analytics_data->non_compute_data->opal_list_length == 1)) {
            if (ORCM_SUCCESS != orcm_util_copy_list_items(analytics_data->non_compute_data,
                                                          &(analytics_event_data->description))) {
                OBJ_RELEASE(analytics_event_data);
                return NULL;
            }
        }

        analytics_event_data->type = type;
        analytics_event_data->severity = severity;
        analytics_event_data->cbfunc = orcm_analytics_base_event_cleanup;
    }
    return analytics_event_data;
}


int orcm_analytics_base_event_set_category(orcm_ras_event_t *analytics_event_data, unsigned int event_category)
{
    unsigned int index = 0;
    const orcm_analytics_event_cat_types_t types[] = {
        {"SOFT_FAULT", ORCM_EVENT_SOFT_FAULT},
        {"HARD_FAULT", ORCM_EVENT_HARD_FAULT},
        {"UNKOWN_FAULT", ORCM_EVENT_UNKOWN_FAULT},
    };

    if (NULL == analytics_event_data) {
        return ORCM_ERROR;
    }

    for (index = 0; index < ARRAY_SIZE(types); index++) {
        if (types[index].cat_int== event_category) {
            return orcm_analytics_base_event_set_description(analytics_event_data, "event_category",
                                                             (void *) types[index].cat_string, OPAL_STRING, NULL);
        }
    }
    return ORCM_ERROR;
}

int orcm_analytics_base_event_set_storage(orcm_ras_event_t *analytics_event_data, unsigned int storage_type)
{
    if (NULL == analytics_event_data) {
        return ORCM_ERROR;
    }
    if (ORCM_STORAGE_TYPE_UNDEFINED <= storage_type) {
        return ORCM_ERROR;
    }

    return orcm_analytics_base_event_set_description(analytics_event_data, "storage_type",
                                                     &storage_type, OPAL_UINT, NULL);
}
