/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/mca/db/db.h"


static void orcm_analytics_db_cleanup(int db_handle, int status, opal_list_t *list,
                                      opal_list_t *ret, void *cbdata);

orcm_analytics_base_db_t orcm_analytics_base_db = {-1, false};

void orcm_analytics_base_db_open_cb(int handle, int status, opal_list_t *props,
                                    opal_list_t *ret, void *cbdata)
{
    if (0 == status) {
        /* database open successfully! */
        orcm_analytics_base_db.db_handle = handle;
        orcm_analytics_base_db.db_handle_acquired = true;
    } else {
        opal_output(0, "Analytics framework: DB Open failed");
        orcm_analytics_base_db.db_handle_acquired = false;
    }

    if (NULL != props) {
        OPAL_LIST_RELEASE(props);
    }
}

void orcm_analytics_base_open_db(void)
{
    if (false == orcm_analytics_base_db.db_handle_acquired) {
        orcm_db.open("analytics", NULL, orcm_analytics_base_db_open_cb, NULL);
    }
}

void orcm_analytics_base_close_db(void)
{
    if (true == orcm_analytics_base_db.db_handle_acquired) {
        orcm_db.close(orcm_analytics_base_db.db_handle, NULL, NULL);
        orcm_analytics_base_db.db_handle = -1;
        orcm_analytics_base_db.db_handle_acquired = false;
    }
}

bool orcm_analytics_base_db_check(orcm_workflow_step_t *wf_step)
{
    bool load_to_db = false;
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
            }
            break;
        }
    }

    return load_to_db;
}

static void orcm_analytics_db_cleanup(int db_handle, int status, opal_list_t *list,
                                      opal_list_t *ret, void *cbdata)
{
    OPAL_LIST_RELEASE(list);
}

int orcm_analytics_base_store(opal_list_t *data_results)
{
    if (NULL == data_results) {
        return ORCM_ERR_BAD_PARAM;
    }

    /* Database connection is open */
    if (true == orcm_analytics_base_db.db_handle_acquired) {
        /* try to load the data to database */
        if (0 <= orcm_analytics_base_db.db_handle) {
            orcm_db.store_new(orcm_analytics_base_db.db_handle, ORCM_DB_ENV_DATA,
                              data_results, NULL, orcm_analytics_db_cleanup, NULL);
            return ORCM_SUCCESS;
        }
    }

    /* Database connection is not allowed */
    return ORCM_ERR_NO_CONNECTION_ALLOWED;
}
