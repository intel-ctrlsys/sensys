/*
 * Copyright (c) 2013-2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/analytics/base/analytics_private.h"

bool orcm_analytics_base_db_check(orcm_workflow_step_t *wf_step)
{
    bool load_to_db = false;
    bool db_exist = false;
    opal_value_t *attribute = NULL;
    char *db_attribute_key = "db";
    char *db_attribute_value_check = "yes";

    OPAL_LIST_FOREACH(attribute, &(wf_step->attributes), opal_value_t) {
        if (NULL == attribute) {
            break;
        }
        if (0 == strncmp(attribute->key, db_attribute_key, strlen(attribute->key) + 1)) {
            if (0 == strncmp(attribute->data.string, db_attribute_value_check,
                strlen(attribute->data.string) + 1)) {
                load_to_db = true;
            } else {
                load_to_db = false;
            }
            db_exist = true;
            break;
        }
    }

    if (!db_exist) {
        if (true == orcm_analytics_base.store_event_data) {
            load_to_db = true;
        } else {
            load_to_db = false;
        }
    }
    return load_to_db;
}
