/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc.  All rights reserved.
 * Copyright (c) 2014-2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/mca.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"
#include "opal/dss/dss_types.h"
#include "opal/runtime/opal_progress_threads.h"

#include "orte/runtime/orte_globals.h"

#include "orcm/mca/db/base/base.h"


/*
 * The following file was created by configure.  It contains extern
 * dbments and the definition of an array of pointers to each
 * module's public mca_base_module_t struct.
 */

#include "orcm/mca/db/base/static-components.h"

orcm_db_API_module_t orcm_db = {
    orcm_db_base_open,
    orcm_db_base_close,
    orcm_db_base_store,
    orcm_db_base_store_new,
    orcm_db_base_record_data_samples,
    orcm_db_base_update_node_features,
    orcm_db_base_record_diag_test,
    orcm_db_base_commit,
    orcm_db_base_rollback,
    orcm_db_base_fetch,
    orcm_db_base_get_num_rows,
    orcm_db_base_get_next_row,
    orcm_db_base_close_result_set,
    orcm_db_base_remove_data
};
orcm_db_base_t orcm_db_base;

static bool orcm_db_base_create_evbase;

static int orcm_db_base_register(mca_base_register_flag_t flags)
{
    orcm_db_base_create_evbase = true;
    mca_base_var_register("orcm", "db", "base", "create_evbase",
                          "Create a separate event base for processing db operations",
                          MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                          OPAL_INFO_LVL_9,
                          MCA_BASE_VAR_SCOPE_READONLY,
                          &orcm_db_base_create_evbase);
    return ORCM_SUCCESS;
}

static int orcm_db_base_frame_close(void)
{
    orcm_db_base_active_component_t *active;
    int i;
    orcm_db_handle_t *hdl;

    /* cleanup the globals */
    for (i=0; i < orcm_db_base.handles.size; i++) {
        if (NULL != (hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles, i))) {
            opal_pointer_array_set_item(&orcm_db_base.handles, i, NULL);
            OBJ_RELEASE(hdl);
        }
    }
    OBJ_DESTRUCT(&orcm_db_base.handles);

    /* cycle across all the active db components and let them cleanup - order
     * doesn't matter in this case
     */
    while (NULL != (active = (orcm_db_base_active_component_t*)opal_list_remove_first(&orcm_db_base.actives))) {
        if (NULL != active->component->finalize) {
            active->component->finalize();
        }
    }
    OPAL_LIST_DESTRUCT(&orcm_db_base.actives);

    if (orcm_db_base_create_evbase && orcm_db_base.ev_base_active) {
        orcm_db_base.ev_base_active = false;
        opal_progress_thread_finalize("db");
    }

    return mca_base_framework_components_close(&orcm_db_base_framework, NULL);
}

static int orcm_db_base_frame_open(mca_base_open_flag_t flags)
{
    OBJ_CONSTRUCT(&orcm_db_base.actives, opal_list_t);
    OBJ_CONSTRUCT(&orcm_db_base.handles, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_db_base.handles, 3, INT_MAX, 1);

    if (orcm_db_base_create_evbase) {
        /* create our own event base */
        orcm_db_base.ev_base_active = true;
        if (NULL == (orcm_db_base.ev_base =
                opal_progress_thread_init("db"))) {
            orcm_db_base.ev_base_active = false;
            return ORCM_ERROR;
        }
    } else {
        /* tie us to the orte_event_base */
        orcm_db_base.ev_base = orte_event_base;
    }

    /* Open up all available components */
    return mca_base_framework_components_open(&orcm_db_base_framework, flags);
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, db, "ORCM Database Framework",
                           orcm_db_base_register,
                           orcm_db_base_frame_open,
                           orcm_db_base_frame_close,
                           mca_db_base_static_components, 0);

static void req_con(orcm_db_request_t *p)
{
    p->cbdata = NULL;

    p->properties = NULL;

    p->hostname = NULL;
    p->time_stamp = NULL;
    p->data_group = NULL;
    p->primary_key = NULL;
    p->key = NULL;

    p->diag_subtype = NULL;
    p->diag_subtype = NULL;
    p->start_time = NULL;
    p->end_time = NULL;
    p->component_index = NULL;
    p->test_result = NULL;
    p->view_name = NULL;

    p->kvs = NULL;
}
OBJ_CLASS_INSTANCE(orcm_db_request_t,
                   opal_object_t,
                   req_con, NULL);

OBJ_CLASS_INSTANCE(orcm_db_handle_t,
                   opal_object_t,
                   NULL, NULL);

OBJ_CLASS_INSTANCE(orcm_db_base_active_component_t,
                   opal_list_item_t,
                   NULL, NULL);

static void filter_con(orcm_db_filter_t *p)
{
    p->op = NONE;
}

OBJ_CLASS_INSTANCE(orcm_db_filter_t,
                   opal_value_t,
                   filter_con, NULL);

static char* get_opal_value_as_sql_string(opal_value_t *value)
{
    char* new_str = NULL;
    if(NULL != value) {
        switch(value->type) {
        case OPAL_STRING:
            new_str = strdup(value->data.string);
            break;
        case OPAL_INT:
            asprintf(&new_str, "%d", value->data.integer);
            break;
        case OPAL_INT8:
            asprintf(&new_str, "%d", value->data.int8);
            break;
        case OPAL_INT16:
            asprintf(&new_str, "%d", value->data.int16);
            break;
        case OPAL_INT32:
            asprintf(&new_str, "%d", value->data.int32);
            break;
        case OPAL_INT64:
            asprintf(&new_str, "%ld", value->data.int64);
            break;
        case OPAL_UINT:
            asprintf(&new_str, "%d", value->data.uint);
            break;
        case OPAL_UINT8:
            asprintf(&new_str, "%d", value->data.uint8);
            break;
        case OPAL_UINT16:
            asprintf(&new_str, "%d", value->data.uint16);
            break;
        case OPAL_UINT32:
            asprintf(&new_str, "%d", value->data.uint32);
            break;
        case OPAL_UINT64:
            asprintf(&new_str, "%ld", value->data.uint64);
            break;
        case OPAL_FLOAT:
            asprintf(&new_str, "%f", value->data.fval);
            break;
        case OPAL_DOUBLE:
            asprintf(&new_str, "%f", value->data.dval);
            break;
        case OPAL_TIMEVAL:
        case OPAL_TIME:
            { /* ISO8601: 'YYYY-MM-DDThh:mm:ss.nnn' */
                time_t tt;
                struct tm tms;
                int len1 = strlen("YYYY-MM-DDThh:mm:ss");
                int len2 = strlen("nnn");
                char tmp1[len1+1];
                char tmp2[len2+1];
                int milliseconds = (int)(value->data.tv.tv_usec / 1000);

                tt = value->data.tv.tv_sec;
                gmtime_r(&tt, &tms); /* use re-entrant version of gmtime; chosen so no localization occurs */

                strftime(tmp1, len1+1, "%FT%T", &tms);
                sprintf(tmp2, "%03d", milliseconds);

                asprintf(&new_str, "%s.%s", tmp1, tmp2);
                break;
            }
        default: /* Unsupported opal type; leave new_str == NULL */
            break;
        }
    }
    return new_str;
}

static char *op_strings[] = {
    NULL,
    " < ",
    " > ",
    " = ",
    " <> ",
    " <= ",
    " >= ",
    " like ",
    " like ",
    " like ",
    " in "
};

static bool add_where_clauses(char **original_query, orcm_db_filter_t *filter, bool *first_clause)
{
    if(NULL == filter || filter->op == NONE || NULL == first_clause || NULL == original_query || NULL == *original_query) {
        return false;
    } else {
        char* old_query = *original_query;
        char c1 = (IN != filter->op)?'\'':'(';
        char c2 = (IN != filter->op)?'\'':')';
        char* val = get_opal_value_as_sql_string(&filter->value);
        char* and = (char*)((*first_clause)?" where ":" and ");

        *first_clause = false; /* we already used this so mark it as */

        asprintf(original_query, "%s%s%s%s%c%s%c", old_query, and, filter->value.key, op_strings[(int)filter->op], c1, val, c2);

        free(val);
        free(old_query);

        return true;
    }
}

char* build_query_from_view_name_and_filters(const char* view_name, opal_list_t* filters)
{
    char* query = NULL;

    if(NULL != view_name && 0 < strlen(view_name)) {
        if(NULL == filters || 0 == opal_list_get_size(filters)) {
            asprintf(&query, "select * from %s;", view_name);
        } else {
            orcm_db_filter_t* filter = NULL;
            char* old_query = NULL;
            bool first_clause = true;

            asprintf(&query, "select * from %s", view_name);

            OPAL_LIST_FOREACH(filter, filters, orcm_db_filter_t) {
                if(false == add_where_clauses(&query, filter, &first_clause)) {
                    free(query);
                    return NULL;
                }
            }
            old_query = query;
            asprintf(&query, "%s;", old_query);
            free(old_query);
        }
    }
    return query;
}
