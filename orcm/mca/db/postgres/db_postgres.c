/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2013-2016 Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 */

#include "orcm_config.h"
#include "orcm/constants.h"
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/time.h>
#include <time.h>

#include "opal_stdint.h"
#include "opal/util/argv.h"
#include "opal/util/error.h"
#include "opal/class/opal_bitmap.h"
#include "orcm/util/utils.h"

#include "orcm/runtime/orcm_globals.h"

#include "orcm/mca/db/base/base.h"
#include "db_postgres.h"

#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_URL

#include "libpq-fe.h"
#include "postgres_fe.h"
#include "catalog/pg_type.h"

#define ORCM_PG_MAX_LINE_LENGTH 4096
#define STRING_MAX_LEN 1024
#define TIMESTAMP_STR_LENGTH 40

extern bool is_supported_opal_int_type(opal_data_type_t type);
extern bool tv_to_str_time_stamp(const struct timeval *time, char *tbuf,
                                 size_t size);
extern bool time_stamp_str_to_tv(char* stamp, struct timeval* time_value);
extern void tm_to_str_time_stamp(const struct tm *time, char *tbuf,
                                 size_t size);

static int postgres_init(struct orcm_db_base_module_t *imod);
static void postgres_reconnect_if_needed(mca_db_postgres_module_t* mod);
static void postgres_finalize(struct orcm_db_base_module_t *imod);
static int postgres_store_sample(struct orcm_db_base_module_t *imod,
                                 const char *data_group,
                                 opal_list_t *kvs);
static int postgres_store(struct orcm_db_base_module_t *imod,
                          orcm_db_data_type_t data_type,
                          opal_list_t *input,
                          opal_list_t *ret);
static int postgres_update_node_features(struct orcm_db_base_module_t *imod,
                                         const char *hostname,
                                         opal_list_t *features);
static int postgres_record_diag_test(struct orcm_db_base_module_t *imod,
                                     const char *hostname,
                                     const char *diag_type,
                                     const char *diag_subtype,
                                     const struct tm *start_time,
                                     const struct tm *end_time,
                                     const int *component_index,
                                     const char *test_result,
                                     opal_list_t *test_params);
static int postgres_commit(struct orcm_db_base_module_t *imod);
static int postgres_rollback(struct orcm_db_base_module_t *imod);

/* Internal helper functions */
static int postgres_store_data_sample(mca_db_postgres_module_t *mod,
                                      opal_list_t *input,
                                      opal_list_t *ret);
static int postgres_store_node_features(mca_db_postgres_module_t *mod,
                                        opal_list_t *input,
                                        opal_list_t *ret);
static int postgres_store_diag_test(mca_db_postgres_module_t *mod,
                                    opal_list_t *input,
                                    opal_list_t *ret);
static int add_event(mca_db_postgres_module_t* mod,
                     opal_list_t *input,
                     opal_bitmap_t *item_bm,
                     long long int* event_id_once_added);
static int postgres_store_event(mca_db_postgres_module_t *mod,
                                opal_list_t *input,
                                opal_list_t *out);

static inline bool status_ok(PGresult *res);
static inline bool is_fatal(PGresult *res);

static int postgres_fetch(struct orcm_db_base_module_t *imod,
                          const char *view,
                          opal_list_t *filters,
                          opal_list_t *output);

static int postgres_get_num_rows(struct orcm_db_base_module_t *imod,
                                 int rshandle,
                                 int *num_rows);

static int postgres_get_next_row(struct orcm_db_base_module_t *imod,
                                 int rshandle,
                                 opal_list_t *row);

static int postgres_close_result_set(struct orcm_db_base_module_t *imod,
                                     int rshandle);

static int postgres_fetch_function(struct orcm_db_base_module_t *imod,
                          const char *function,
                          opal_list_t *arguments,
                          opal_list_t *output);

mca_db_postgres_module_t mca_db_postgres_module = {
    {
        postgres_init,
        postgres_finalize,
        postgres_store_sample,
        postgres_store,
        NULL,
        postgres_update_node_features,
        postgres_record_diag_test,
        postgres_commit,
        postgres_rollback,
        postgres_fetch,
        postgres_get_num_rows,
        postgres_get_next_row,
        postgres_close_result_set,
        NULL,
        postgres_fetch_function,
    },
};

#define ERROR_MSG_FMT_INIT(mod, msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:postgres: Connection failed: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "\tURI: %s", mod->pguri); \
    opal_output(0, "\tDB: %s", mod->dbname); \
    opal_output(0, "\tOptions: %s", \
                NULL == mod->pgoptions ? "NULL" : mod->pgoptions); \
    opal_output(0, "\tTTY: %s", NULL == mod->pgtty ? "NULL" : mod->pgtty); \
    opal_output(0, "***********************************************");

static int postgres_init(struct orcm_db_base_module_t *imod)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;
    char **login = NULL;
    char **uri_parts = NULL;
    char *host;
    char *port;
    int count;
    int i;
    PGconn *conn;

    mod->results_sets = OBJ_NEW(opal_pointer_array_t);

    if (NULL == mod->dbname) {
        opal_output(0, "db:postgres: No database name provided");
        return ORCM_ERR_BAD_PARAM;
    }

    /* break the user info into its login parts */
    login = opal_argv_split(mod->user, ':');
    if (2 != opal_argv_count(login)) {
        opal_output(0, "db:postgres: User info is invalid: %s",
                    mod->user);
        opal_argv_free(login);
        return ORCM_ERR_BAD_PARAM;
    }
    /* break the URI */
    uri_parts = opal_argv_split(mod->pguri, ':');
    count = opal_argv_count(uri_parts);
    if (2 < count || 1 > count) {
        opal_output(0, "db:postgres: URI info is invalid: %s",
                    mod->pguri);
        opal_argv_free(login);
        opal_argv_free(uri_parts);
        return ORCM_ERR_BAD_PARAM;
    }
    host = uri_parts[0];
    port = count > 1 ? uri_parts[1] : NULL;

    conn = PQsetdbLogin(host, port,
                        mod->pgoptions,
                        mod->pgtty,
                        mod->dbname,
                        login[0], login[1]);
    opal_argv_free(login);
    opal_argv_free(uri_parts);

    if (PQstatus(conn) != CONNECTION_OK) {
        ERROR_MSG_FMT_INIT(mod, "%s", PQerrorMessage(conn));

        return ORCM_ERR_CONNECTION_FAILED;
    }

    mod->conn = conn;

    for (i = 0; i < ORCM_DB_PG_STMT_NUM_STMTS; i++) {
        mod->prepared[i] = false;
    }

    opal_output_verbose(5, orcm_db_base_framework.framework_output,
                        "db:postgres: Connection established to %s",
                        mod->dbname);

    return ORCM_SUCCESS;
}

static void postgres_reconnect_if_needed(mca_db_postgres_module_t* mod) {
    int i = 0;
    if (CONNECTION_BAD == PQstatus(mod->conn)) {
        opal_output(0, "Attempt to reset the PostgreSQL connection.");
        PQreset(mod->conn);
        for (i = 0; i < ORCM_DB_PG_STMT_NUM_STMTS; i++) {
            mod->prepared[i] = false;
        }
    }
}

static void postgres_finalize(struct orcm_db_base_module_t *imod)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;

    SAFEFREE(mod->pguri);
    SAFEFREE(mod->dbname);
    SAFEFREE(mod->user);
    SAFEFREE(mod->pgoptions);
    SAFEFREE(mod->pgtty);
    if (NULL != mod->conn) {
        PQfinish(mod->conn);
    }
    ORCM_RELEASE(mod->results_sets);
    SAFEFREE(mod);
}

#define ERR_MSG_STORE(msg) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:postgres: Unable to record data sample: "); \
    opal_output(0, msg); \
    opal_output(0, "***********************************************");

#define ERR_MSG_FMT_STORE(msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:postgres: Unable to record data sample: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "***********************************************");

static int postgres_store(struct orcm_db_base_module_t *imod,
                          orcm_db_data_type_t data_type,
                          opal_list_t *input,
                          opal_list_t *ret)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;
    int rc = ORCM_SUCCESS;

    switch (data_type) {
    case ORCM_DB_ENV_DATA:
        rc = postgres_store_data_sample(mod, input, ret);
        break;
    case ORCM_DB_INVENTORY_DATA:
        rc = postgres_store_node_features(mod, input, ret);
        break;
    case ORCM_DB_DIAG_DATA:
        rc = postgres_store_diag_test(mod, input, ret);
        break;
    case ORCM_DB_EVENT_DATA:
        rc = postgres_store_event(mod, input, ret);
        break;
    default:
        return ORCM_ERR_NOT_IMPLEMENTED;
    }

    return rc;
}

int postgres_escape_string(char *str_src, char **str_dst)
{
    int rc = ORCM_SUCCESS;
    int str_pos = 0;
    int quotes = 0;
    size_t act_length = 0;
    char *chr_pos = NULL;

    if( NULL != str_src ) {
        act_length = strlen(str_src);
        *str_dst = strdup(str_src);

        if( NULL != *str_dst ) {
            while( ORCM_SUCCESS == rc
                   && NULL != (chr_pos = strchr(str_src + str_pos, '\'')) ) {
                *str_dst = (char *)realloc( *str_dst, act_length + 2 );
                if( NULL != *str_dst) {
                    str_pos = chr_pos - str_src + 1;
                    strcpy( *str_dst + str_pos + quotes + 1, chr_pos + 1 );
                    (*str_dst)[str_pos + quotes] = '\'';
                    act_length++;
                    quotes++;
                } else {
                    rc = ORCM_ERR_OUT_OF_RESOURCE;
                }
            }
        } else {
            rc = ORCM_ERR_OUT_OF_RESOURCE;
        }
    }

    return rc;
}

static int postgres_store_sample(struct orcm_db_base_module_t *imod,
                                 const char *data_group,
                                 opal_list_t *kvs)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;
    int rc = ORCM_SUCCESS;

    opal_value_t *kv;
    opal_value_t *timestamp_item = NULL;
    opal_value_t *hostname_item = NULL;

    char *escaped_str = NULL;
    char hostname[256];
    char time_stamp[TIMESTAMP_STR_LENGTH];
    char **data_item_parts=NULL;
    char *data_item;
    orcm_db_item_t item;
    char *units;
    int count;
    int ret;
    size_t num_items;
    char **rows = NULL;
    char *values = NULL;
    char *insert_stmt = NULL;
    size_t i;

    PGresult *res = NULL;

    if (NULL == data_group) {
        ERR_MSG_STORE("No data group specified");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == kvs) {
        ERR_MSG_STORE("No value list specified");
        return ORCM_ERR_BAD_PARAM;
    }

    /* First, retrieve the time stamp and the hostname from the list */
    OPAL_LIST_FOREACH(kv, kvs, opal_value_t) {
        if (!strcmp(kv->key, "ctime")) {
            switch (kv->type) {
            case OPAL_TIMEVAL:
            case OPAL_TIME:
                if (!tv_to_str_time_stamp(&kv->data.tv, time_stamp,
                                          sizeof(time_stamp))) {
                    ERR_MSG_STORE("Failed to convert time stamp value");
                    return ORCM_ERR_BAD_PARAM;
                }
                break;
            case OPAL_STRING:
                strncpy(time_stamp, kv->data.string, sizeof(time_stamp) - 1);
                time_stamp[sizeof(time_stamp) - 1] = '\0';
                break;
            default:
                ERR_MSG_STORE("Invalid value type specified for time stamp");
                return ORCM_ERR_BAD_PARAM;
            }
            timestamp_item = kv;
        } else if (!strcmp(kv->key, "hostname")) {
            if (OPAL_STRING == kv->type) {
                strncpy(hostname, kv->data.string, sizeof(hostname) - 1);
                hostname[sizeof(hostname) - 1] = '\0';
            } else {
                ERR_MSG_STORE("Invalid value type specified for hostname");
                return ORCM_ERR_BAD_PARAM;
            }
            hostname_item = kv;
        }

        if (NULL != timestamp_item && NULL != hostname_item) {
            break;
        }
    }

    if (NULL == timestamp_item) {
        ERR_MSG_STORE("No time stamp provided");
        return ORCM_ERR_BAD_PARAM;
    }
    if (NULL == hostname_item) {
        ERR_MSG_STORE("No hostname provided");
        return ORCM_ERR_BAD_PARAM;
    }

    /* Remove these from the list to avoid processing them again */
    opal_list_remove_item(kvs, (opal_list_item_t *)timestamp_item);
    opal_list_remove_item(kvs, (opal_list_item_t *)hostname_item);
    OBJ_RELEASE(timestamp_item);
    OBJ_RELEASE(hostname_item);

    num_items = opal_list_get_size(kvs);
    rows = (char **)malloc(sizeof(char *) * (num_items + 1));
    if (NULL == rows) {
        rc = ORCM_ERR_OUT_OF_RESOURCE;
        ERR_MSG_STORE("Unable to allocate memory");
        goto cleanup_and_exit;
    }
    for (i = 0; i < num_items + 1; i++) {
        rows[i] = NULL;
    }
    i = 0;
    OPAL_LIST_FOREACH(kv, kvs, opal_value_t) {
        /* kv->key will contain: <data_item>:<units> */
        data_item_parts = opal_argv_split(kv->key, ':');
        count = opal_argv_count(data_item_parts);
        if (count > 0) {
            data_item = data_item_parts[0];
            if (count > 1) {
                units = data_item_parts[1];
            } else {
                units = NULL;
            }
        } else {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_STORE("No data item specified");
            goto cleanup_and_exit;
        }

        ret = opal_value_to_orcm_db_item(kv, &item);
        if (ORCM_SUCCESS != ret) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_FMT_STORE("Unsupported data type: %s",
                              opal_dss.lookup_data_type(kv->type));
            goto cleanup_and_exit;
        }

        /* (hostname,
         *  data_item,
         *  time_stamp,
         *  value_int,
         *  value_real,
         *  value_str,
         *  units,
         *  data_type_id) */
        switch (item.item_type) {
        case ORCM_DB_ITEM_STRING:
            postgres_escape_string(item.value.value_str, &escaped_str);
            if (NULL != units) {
                asprintf(rows + i,
                         "('%s','%s_%s','%s',NULL,NULL,'%s','%s',%d)",
                         hostname, data_group, data_item, time_stamp,
                         escaped_str, units, kv->type);
            } else {
                asprintf(rows + i,
                         "('%s','%s_%s','%s',NULL,NULL,'%s',NULL,%d)",
                         hostname, data_group, data_item, time_stamp,
                         escaped_str, kv->type);
            }
            SAFEFREE(escaped_str);
            break;
        case ORCM_DB_ITEM_REAL:
            if (NULL != units) {
                asprintf(rows + i,
                         "('%s','%s_%s','%s',NULL,%f,NULL,'%s',%d)",
                         hostname, data_group, data_item, time_stamp,
                         item.value.value_real, units, kv->type);
            } else {
                asprintf(rows + i,
                         "('%s','%s_%s','%s',NULL,%f,NULL,NULL,%d)",
                         hostname, data_group, data_item, time_stamp,
                         item.value.value_real, kv->type);
            }
            break;
        default: /* ORCM_DB_ITEM_INTEGER */
            if (NULL != units) {
                asprintf(rows + i,
                         "('%s','%s_%s','%s',%lld,NULL,NULL,'%s',%d)",
                         hostname, data_group, data_item, time_stamp,
                         item.value.value_int, units, kv->type);
            } else {
                asprintf(rows + i,
                         "('%s','%s_%s','%s',%lld,NULL,NULL,NULL,%d)",
                         hostname, data_group, data_item, time_stamp,
                         item.value.value_int, kv->type);
            }
        }
        i++;
        opal_argv_free(data_item_parts);
        data_item_parts = NULL;
    }

    values = opal_argv_join(rows, ',');
    opal_argv_free(rows);
    rows = NULL;

    asprintf(&insert_stmt, "insert into data_sample_raw(hostname,data_item,"
             "time_stamp,value_int,value_real,value_str,units,data_type_id) "
             "values %s", values);
    SAFEFREE(values);

    /* If we're not in auto commit mode, let's start a new transaction (if
     * one hasn't already been started) */
    if (!mod->tran_started && !mod->autocommit) {
        res = PQexec(mod->conn, "begin");
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_STORE("Unable to start transaction: %s",
                              PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
        mod->tran_started = true;
    }

    res = PQexec(mod->conn, insert_stmt);
    if (!status_ok(res)) {
        rc = ORCM_ERROR;
        ERR_MSG_STORE(PQresultErrorMessage(res));
        postgres_reconnect_if_needed(mod);
        goto cleanup_and_exit;
    }
    PQclear(res);
    res = NULL;

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "postgres_store_sample succeeded");

cleanup_and_exit:
    if (NULL != res) {
        PQclear(res);
    }
    if (NULL != rows) {
        opal_argv_free(rows);
    }
    SAFEFREE(insert_stmt);
    if (NULL != data_item_parts){
        opal_argv_free(data_item_parts);
    }

    return rc;
}

static int postgres_store_data_sample(mca_db_postgres_module_t *mod,
                                      opal_list_t *input,
                                      opal_list_t *ret)
{
    int rc = ORCM_SUCCESS;

    const int NUM_PARAMS = 3;
    const char *params[] = {
        "data_group",
        "ctime",
        "hostname"
    };
    opal_value_t *param_items[] = {NULL, NULL, NULL};
    opal_bitmap_t item_bm;

    char *escaped_str = NULL;
    char *hostname = NULL;
    char *data_group = NULL;
    char time_stamp[TIMESTAMP_STR_LENGTH];
    char *data_item;
    orcm_db_item_t item;
    char *units;
    long long int event_id_once_added = -1;

    size_t num_items;
    char **rows = NULL;
    char *values = NULL;
    char *insert_stmt = NULL;
    size_t i, j;

    opal_value_t *kv;
    orcm_value_t *mv;

    PGresult *res = NULL;

    if (NULL == input) {
        ERR_MSG_STORE("No parameters provided");
        return ORCM_ERR_BAD_PARAM;
    }

    num_items = opal_list_get_size(input);
    OBJ_CONSTRUCT(&item_bm, opal_bitmap_t);
    opal_bitmap_init(&item_bm, (int)num_items);

    /* Get the main parameters form the list */
    orcm_util_find_items(params, NUM_PARAMS, input, param_items, &item_bm);

    /* Check the parameters */
    if (NULL == param_items[0]) {
        ERR_MSG_STORE("No data group provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    if (NULL == param_items[1]) {
        ERR_MSG_STORE("No time stamp provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }
    if (NULL == param_items[2]) {
        ERR_MSG_STORE("No hostname provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[0];
    if (OPAL_STRING == kv->type) {
        data_group = kv->data.string;
    } else {
        ERR_MSG_STORE("Invalid value type specified for data group");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[1];
    switch (kv->type) {
    case OPAL_TIMEVAL:
    case OPAL_TIME:
        if (!tv_to_str_time_stamp(&kv->data.tv, time_stamp,
                                  sizeof(time_stamp))) {
            ERR_MSG_STORE("Failed to convert time stamp value");
            rc = ORCM_ERR_BAD_PARAM;
            goto cleanup_and_exit;
        }
        break;
    case OPAL_STRING:
        strncpy(time_stamp, kv->data.string, sizeof(time_stamp) - 1);
        time_stamp[sizeof(time_stamp) - 1] = '\0';
        break;
    default:
        ERR_MSG_STORE("Invalid value type specified for time stamp");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[2];
    if (OPAL_STRING == kv->type) {
        hostname = kv->data.string;
    } else {
        ERR_MSG_STORE("Invalid value type specified for hostname");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    num_items -= NUM_PARAMS;
    if (num_items <= 0) {
        ERR_MSG_STORE("No data samples provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    rows = (char **)malloc(sizeof(char *) * (num_items + 1));
    if (NULL == rows) {
        rc = ORCM_ERR_OUT_OF_RESOURCE;
        ERR_MSG_STORE("Unable to allocate memory");
        goto cleanup_and_exit;
    }
    for (i = 0; i < num_items + 1; i++) {
        rows[i] = NULL;
    }

    /* If we're not in auto commit mode, let's start a new transaction (if
     * one hasn't already been started) */
    if (!mod->tran_started && !mod->autocommit) {
        res = PQexec(mod->conn, "begin");
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_STORE("Unable to start transaction: %s",
                              PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
        mod->tran_started = true;
    }

    rc = add_event(mod, input, &item_bm, &event_id_once_added);
    if (ORCM_SUCCESS != rc || 0 > event_id_once_added) {
        ERR_MSG_STORE("Failed to store event");
        goto cleanup_and_exit;
    }

    /* Build the SQL command with the data provided in the list */
    i = 0;
    j = 0;
    OPAL_LIST_FOREACH(mv, input, orcm_value_t) {
        /* Ignore the items that have already been processed */
        if (opal_bitmap_is_set_bit(&item_bm, i)) {
            i++;
            continue;
        }

        data_item = mv->value.key;
        units = mv->units;
        if (NULL == data_item) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_STORE("No data item specified");
            goto cleanup_and_exit;
        }

        if (ORCM_SUCCESS != opal_value_to_orcm_db_item(&mv->value, &item)) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_FMT_STORE("Unsupported data type: %s",
                              opal_dss.lookup_data_type(mv->value.type));
            goto cleanup_and_exit;
        }

        /* (hostname,
         *  data_item,
         *  time_stamp,
         *  value_int,
         *  value_real,
         *  value_str,
         *  units,
         *  data_type_id
         *  event_id) */
        switch (item.item_type) {
        case ORCM_DB_ITEM_STRING:
            postgres_escape_string(item.value.value_str, &escaped_str);
            if (NULL != units) {
                asprintf(rows + j,
                         "('%s','%s_%s','%s',NULL,NULL,'%s','%s',%d,%d,%lld)",
                         hostname, data_group, data_item, time_stamp,
                         escaped_str, units, mv->value.type, mv->value.type,
                         event_id_once_added);
            } else {
                asprintf(rows + j,
                         "('%s','%s_%s','%s',NULL,NULL,'%s',NULL,%d,%d,%lld)",
                         hostname, data_group, data_item, time_stamp,
                         escaped_str, mv->value.type, mv->value.type,
                         event_id_once_added);
            }
            SAFEFREE(escaped_str);
            break;
        case ORCM_DB_ITEM_REAL:
            if (NULL != units) {
                asprintf(rows + j,
                         "('%s','%s_%s','%s',NULL,%f,NULL,'%s',%d,%d,%lld)",
                         hostname, data_group, data_item, time_stamp,
                         item.value.value_real, units, mv->value.type,
                         mv->value.type, event_id_once_added);
            } else {
                asprintf(rows + j,
                         "('%s','%s_%s','%s',NULL,%f,NULL,NULL,%d,%d,%lld)",
                         hostname, data_group, data_item, time_stamp,
                         item.value.value_real, mv->value.type,
                         mv->value.type, event_id_once_added);
            }
            break;
        default: /* ORCM_DB_ITEM_INTEGER */
            if (NULL != units) {
                asprintf(rows + j,
                         "('%s','%s_%s','%s',%lld,NULL,NULL,'%s',%d,%d,%lld)",
                         hostname, data_group, data_item, time_stamp,
                         item.value.value_int, units, mv->value.type, mv->value.type,
                         event_id_once_added);
            } else {
                asprintf(rows + j,
                         "('%s','%s_%s','%s',%lld,NULL,NULL,NULL,%d,%d,%lld)",
                         hostname, data_group, data_item, time_stamp,
                         item.value.value_int, mv->value.type, mv->value.type,
                         event_id_once_added);
            }
        }
        i++;
        j++;
    }

    values = opal_argv_join(rows, ',');
    opal_argv_free(rows);
    rows = NULL;

    asprintf(&insert_stmt, "INSERT INTO data_sample_raw("
                                "hostname,"
                                "data_item,"
                                "time_stamp,"
                                "value_int,"
                                "value_real,"
                                "value_str,"
                                "units,"
                                "data_type_id,"
                                "app_value_type_id,"
                                "event_id) "
                           "VALUES %s", values);
    SAFEFREE(values);

    res = PQexec(mod->conn, insert_stmt);
    if (!status_ok(res)) {
        rc = ORCM_ERROR;
        ERR_MSG_STORE(PQresultErrorMessage(res));
        postgres_reconnect_if_needed(mod);
        goto cleanup_and_exit;
    }
    PQclear(res);
    res = NULL;

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "postgres_store_sample succeeded");

cleanup_and_exit:
    if (NULL != res) {
        PQclear(res);
    }
    if (NULL != rows) {
        opal_argv_free(rows);
    }
    SAFEFREE(insert_stmt);
    OBJ_DESTRUCT(&item_bm);
    return rc;
}

#define ERR_MSG_UNF(msg) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:postgres: Unable to update node features"); \
    opal_output(0, msg); \
    opal_output(0, "***********************************************");

#define ERR_MSG_FMT_UNF(msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:postgres: Unable to update node features"); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "***********************************************");

static int postgres_update_node_features(struct orcm_db_base_module_t *imod,
                                         const char *hostname,
                                         opal_list_t *features)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;
    int rc = ORCM_SUCCESS;

    orcm_value_t *mv;

    const int NUM_PARAMS = 7;
    const char *params[NUM_PARAMS];
    char *type_str = NULL;
    char *value_str = NULL;
    orcm_db_item_t item;
    int ret;

    PGresult *res = NULL;

    bool local_tran_started = false;

    if (NULL == hostname) {
        ERR_MSG_UNF("No hostname provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == features) {
        ERR_MSG_UNF("No node features provided");
        return ORCM_ERR_BAD_PARAM;
    }

    /* Prepare the statement only if it hasn't already been prepared */
    if (!mod->prepared[ORCM_DB_PG_STMT_SET_NODE_FEATURE_UPDATE]) {
        /*
         * $1: p_hostname character varying,
         * $2: p_feature character varying,
         * $3: p_data_type_id integer,
         * $4: p_value_int bigint,
         * $5: p_value_real double precision,
         * $6: p_value_str character varying,
         * $7: p_units character varying.
         * $8: p_time_stamp timestamp (NULL will default to current timestamp)
         * */
        res = PQprepare(mod->conn, "set_node_feature_update",
                        "select set_node_feature($1, $2, $3, $4, $5, $6, $7, NULL)",
                        0, NULL);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_UNF(PQresultErrorMessage(res));
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;

        mod->prepared[ORCM_DB_PG_STMT_SET_NODE_FEATURE_UPDATE] = true;
    }

    if (mod->autocommit || !mod->tran_started) {
        res = PQexec(mod->conn, "begin");
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_UNF("Unable to begin transaction: %s",
                            PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;

        local_tran_started = true;
    }

    params[0] = hostname;
    OPAL_LIST_FOREACH(mv, features, orcm_value_t) {
        if (NULL == mv->value.key || 0 == strlen(mv->value.key)) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_UNF("Key or node feature name not provided for value");
            goto cleanup_and_exit;
        }

        ret = opal_value_to_orcm_db_item(&mv->value, &item);
        if (ORCM_SUCCESS != ret) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_FMT_UNF("Unsupported data type: %s",
                            opal_dss.lookup_data_type(mv->value.type));
            goto cleanup_and_exit;
        }

        params[1] = mv->value.key;
        asprintf(&type_str, "%d", item.opal_type);
        params[2] = type_str;
        switch (item.item_type) {
        case ORCM_DB_ITEM_STRING:
            params[3] = NULL;
            params[4] = NULL;
            params[5] = item.value.value_str;
            break;
        case ORCM_DB_ITEM_REAL:
            params[3] = NULL;
            asprintf(&value_str, "%f", item.value.value_real);
            params[4] = value_str;
            params[5] = NULL;
            break;
        default:
            asprintf(&value_str, "%lld", item.value.value_int);
            params[3] = value_str;
            params[4] = NULL;
            params[5] = NULL;
        }
        params[6] = mv->units;

        res = PQexecPrepared(mod->conn, "set_node_feature_update", NUM_PARAMS,
                             params, NULL, NULL, 0);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_UNF(PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
        SAFEFREE(type_str);
        SAFEFREE(value_str);
    }

    if (mod->autocommit) {
        res = PQexec(mod->conn, "commit");
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_UNF("Unable to commit transaction: %s",
                            PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "postgres_update_node_features succeeded");

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        res = PQexec(mod->conn, "rollback");
        if (!status_ok(res)) {
            postgres_reconnect_if_needed(mod);
        }
    }
    if (NULL != res) {
        PQclear(res);
    }
    SAFEFREE(type_str);
    SAFEFREE(value_str);
    return rc;
}

static int postgres_store_node_features(mca_db_postgres_module_t *mod,
                                        opal_list_t *input,
                                        opal_list_t *ret)
{
    int rc = ORCM_SUCCESS;

    const int NUM_PARAMS = 2;
    const char *params[] = {
        "hostname",
        "ctime"
    };
    opal_value_t *param_items[] = {NULL, NULL};
    opal_bitmap_t item_bm;

    char *hostname = NULL;

    const int SP_NUM_PARAMS = 8;
    const char *sp_params[SP_NUM_PARAMS];
    char *type_str = NULL;
    char *value_str = NULL;
    char inventory_str_timestamp[TIMESTAMP_STR_LENGTH];
    orcm_db_item_t item;
    size_t num_items;

    orcm_value_t *mv;
    opal_value_t *kv;
    int i;

    PGresult *res = NULL;

    bool local_tran_started = false;

    if (NULL == input) {
        ERR_MSG_UNF("No parameters provided");
        return ORCM_ERR_BAD_PARAM;
    }

    num_items = opal_list_get_size(input);
    OBJ_CONSTRUCT(&item_bm, opal_bitmap_t);
    opal_bitmap_init(&item_bm, (int)num_items);

    /* Get the main parameters from the list */
    orcm_util_find_items(params, NUM_PARAMS, input, param_items, &item_bm);

    /* Check parameters */
    if (NULL == param_items[0]) {
        ERR_MSG_UNF("No hostname provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    if (NULL == param_items[1]) {
        ERR_MSG_UNF("No time stamp provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[0];
    if (OPAL_STRING == kv->type) {
        hostname = kv->data.string;
    } else {
        ERR_MSG_UNF("Invalid value type specified for hostname");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[1];
    if (OPAL_TIMEVAL == kv->type) {
        if (!tv_to_str_time_stamp(&kv->data.tv, inventory_str_timestamp,
                                  sizeof(inventory_str_timestamp))) {
            ERR_MSG_UNF("Failed to convert time stamp value");
            rc = ORCM_ERR_BAD_PARAM;
            goto cleanup_and_exit;
        }
    } else {
        ERR_MSG_UNF("Invalid value type specified for inventory time stamp");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    if (num_items <= (size_t)NUM_PARAMS) {
        ERR_MSG_UNF("No node features provided");
        goto cleanup_and_exit;
    }

    /* Prepare the statement only if it hasn't already been prepared */
    if (!mod->prepared[ORCM_DB_PG_STMT_SET_NODE_FEATURE_STORE]) {
        /*
         * $1: p_hostname character varying,
         * $2: p_feature character varying,
         * $3: p_data_type_id integer,
         * $4: p_value_int bigint,
         * $5: p_value_real double precision,
         * $6: p_value_str character varying,
         * $7: p_units character varying,
         * $8: p_time_stamp timestamp
         * */
        res = PQprepare(mod->conn, "set_node_feature_store",
                        "select set_node_feature($1, $2, $3, $4, $5, $6, $7, $8)",
                        0, NULL);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_UNF(PQresultErrorMessage(res));
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;

        mod->prepared[ORCM_DB_PG_STMT_SET_NODE_FEATURE_STORE] = true;
    }

    if (mod->autocommit || !mod->tran_started) {
        res = PQexec(mod->conn, "begin");
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_UNF("Unable to begin transaction: %s",
                            PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;

        local_tran_started = true;
    }

    /* Build and execute the SQL commands to store the data in the list */
    sp_params[0] = hostname;
    sp_params[7] = inventory_str_timestamp;
    i = 0;
    OPAL_LIST_FOREACH(mv, input, orcm_value_t) {
        /* Ignore the items that have already been processed. */
        if (opal_bitmap_is_set_bit(&item_bm, i)) {
            i++;
            continue;
        }

        if (NULL == mv->value.key || 0 == strlen(mv->value.key)) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_UNF("Key or node feature name not provided for value");
            goto cleanup_and_exit;
        }

        if (ORCM_SUCCESS != opal_value_to_orcm_db_item(&mv->value, &item)) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_FMT_UNF("Unsupported data type: %s",
                            opal_dss.lookup_data_type(mv->value.type));
            goto cleanup_and_exit;
        }

        sp_params[1] = mv->value.key;
        asprintf(&type_str, "%d", item.opal_type);
        sp_params[2] = type_str;
        switch (item.item_type) {
        case ORCM_DB_ITEM_STRING:
            sp_params[3] = NULL;
            sp_params[4] = NULL;
            sp_params[5] = item.value.value_str;
            break;
        case ORCM_DB_ITEM_REAL:
            sp_params[3] = NULL;
            asprintf(&value_str, "%f", item.value.value_real);
            sp_params[4] = value_str;
            sp_params[5] = NULL;
            break;
        default:
            asprintf(&value_str, "%lld", item.value.value_int);
            sp_params[3] = value_str;
            sp_params[4] = NULL;
            sp_params[5] = NULL;
        }
        sp_params[6] = mv->units;

        res = PQexecPrepared(mod->conn, "set_node_feature_store", SP_NUM_PARAMS,
                             sp_params, NULL, NULL, 0);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_UNF(PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
        SAFEFREE(type_str);
        SAFEFREE(value_str);
        i++;
    }

    if (mod->autocommit) {
        res = PQexec(mod->conn, "commit");
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_UNF("Unable to commit transaction: %s",
                            PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "postgres_update_node_features succeeded");

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        res = PQexec(mod->conn, "rollback");
        if (!status_ok(res)) {
            postgres_reconnect_if_needed(mod);
        }
    }

    if (NULL != res) {
        PQclear(res);
    }
    SAFEFREE(type_str);
    SAFEFREE(value_str);
    OBJ_DESTRUCT(&item_bm);
    return rc;
}

#define ERR_MSG_RDT(msg) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:postgres: Unable to record diagnostic test: "); \
    opal_output(0, msg); \
    opal_output(0, "***********************************************");

#define ERR_MSG_FMT_RDT(msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:postgres: Unable to record diagnostic test: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "***********************************************");

static int postgres_record_diag_test(struct orcm_db_base_module_t *imod,
                                     const char *hostname,
                                     const char *diag_type,
                                     const char *diag_subtype,
                                     const struct tm *start_time,
                                     const struct tm *end_time,
                                     const int *component_index,
                                     const char *test_result,
                                     opal_list_t *test_params)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;
    int rc = ORCM_SUCCESS;

    const int NUM_TEST_RESULT_PARAMS = 7;
    const int NUM_TEST_CONFIG_PARAMS = 10;

    const char *result_params[NUM_TEST_RESULT_PARAMS];
    const char *config_params[NUM_TEST_CONFIG_PARAMS];
    char *index_str = NULL;
    char *type_str = NULL;
    char *value_str = NULL;
    char start_time_str[TIMESTAMP_STR_LENGTH];
    char end_time_str[TIMESTAMP_STR_LENGTH];

    orcm_value_t *mv;
    orcm_db_item_t item;

    PGresult *res = NULL;
    int ret;

    bool local_tran_started = false;

    if (NULL == hostname) {
        ERR_MSG_RDT("No hostname provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == diag_type) {
        ERR_MSG_RDT("No diagnostic type provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == diag_subtype) {
        ERR_MSG_RDT("No diagnostic subtype provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == start_time) {
        ERR_MSG_RDT("No start time provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == test_result) {
        ERR_MSG_RDT("No test result provided");
        return ORCM_ERR_BAD_PARAM;
    }

    /* Prepare the statement only if it hasn't already been prepared */
    if (!mod->prepared[ORCM_DB_PG_STMT_RECORD_DIAG_TEST_RESULT]) {
        /*
         * $1: hostname
         * $2: diagnostic type
         * $3: diagnostic subtype
         * $4: start time
         * $5: end time
         * $6: component index
         * $7: test result
         */
        res = PQprepare(mod->conn, "record_diag_test_result",
                        "select record_diag_test_result("
                        "$1, $2, $3, $4, $5, $6, $7)",
                        0, NULL);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_RDT(PQresultErrorMessage(res));
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;

        mod->prepared[ORCM_DB_PG_STMT_RECORD_DIAG_TEST_RESULT] = true;
    }

    if (mod->autocommit || !mod->tran_started) {
        res = PQexec(mod->conn, "begin");
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_RDT("Unable to begin transaction: %s",
                            PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;

        local_tran_started = true;
    }

    tm_to_str_time_stamp(start_time, start_time_str, sizeof(start_time_str));

    if (NULL == component_index) {
        index_str = NULL;
    } else {
        asprintf(&index_str, "%d", *component_index);
    }

    result_params[0] = hostname;
    result_params[1] = diag_type;
    result_params[2] = diag_subtype;
    result_params[3] = start_time_str;
    if (NULL != end_time) {
        tm_to_str_time_stamp(end_time, end_time_str, sizeof(end_time_str));
        result_params[4] = end_time_str;
    } else {
        result_params[4] = NULL;
    }
    result_params[5] = index_str;
    result_params[6] = test_result;

    res = PQexecPrepared(mod->conn, "record_diag_test_result",
                         NUM_TEST_RESULT_PARAMS, result_params, NULL, NULL, 0);
    if (!status_ok(res)) {
        rc = ORCM_ERROR;
        ERR_MSG_RDT(PQresultErrorMessage(res));
        postgres_reconnect_if_needed(mod);
        goto cleanup_and_exit;
    }
    PQclear(res);
    res = NULL;
    SAFEFREE(index_str);

    if (NULL == test_params) {
        /* No test parameters provided, we're done! */
        if (mod->autocommit) {
            res = PQexec(mod->conn, "commit");
            if (!status_ok(res)) {
                rc = ORCM_ERROR;
                ERR_MSG_FMT_RDT("Unable to commit transaction: %s",
                                PQresultErrorMessage(res));
                postgres_reconnect_if_needed(mod);
                goto cleanup_and_exit;
            }
            PQclear(res);
            res = NULL;
        }

        opal_output_verbose(2, orcm_db_base_framework.framework_output,
                            "postgres_record_diag_test succeeded");
        goto cleanup_and_exit;
    }

    /* Prepare the statement only if it hasn't already been prepared */
    if (!mod->prepared[ORCM_DB_PG_STMT_RECORD_DIAG_TEST_CONFIG]) {
        /*
         * $1: hostname
         * $2: diagnostic type
         * $3: diagnostic subtype
         * $4: start time
         * $5: test parameter
         * $6: data type
         * $7: integer value
         * $8: real value
         * $9: string value
         * $10: units
         */
        res = PQprepare(mod->conn, "record_diag_test_config",
                        "select record_diag_test_config("
                        "$1, $2, $3, $4, $5, $6, $7, $8, $9, $10)",
                        0, NULL);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_RDT(PQresultErrorMessage(res));
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;

        mod->prepared[ORCM_DB_PG_STMT_RECORD_DIAG_TEST_CONFIG] = true;
    }

    config_params[0] = hostname;
    config_params[1] = diag_type;
    config_params[2] = diag_subtype;
    config_params[3] = start_time_str;
    OPAL_LIST_FOREACH(mv, test_params, orcm_value_t) {
        if (NULL == mv->value.key || 0 == strlen(mv->value.key)) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_RDT("Key or node feature name not provided for value");
            goto cleanup_and_exit;
        }

        ret = opal_value_to_orcm_db_item(&mv->value, &item);
        if (ORCM_SUCCESS != ret) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_FMT_RDT("Unsupported data type: %s",
                            opal_dss.lookup_data_type(mv->value.type));
            goto cleanup_and_exit;
        }

        config_params[4] = mv->value.key;
        asprintf(&type_str, "%d", item.opal_type);
        config_params[5] = type_str;
        switch (item.item_type) {
        case ORCM_DB_ITEM_STRING:
            config_params[6] = NULL;
            config_params[7] = NULL;
            config_params[8] = item.value.value_str;
            break;
        case ORCM_DB_ITEM_REAL:
            config_params[6] = NULL;
            asprintf(&value_str, "%f", item.value.value_real);
            config_params[7] = value_str;
            config_params[8] = NULL;
            break;
        default:
            asprintf(&value_str, "%lld", item.value.value_int);
            config_params[6] = value_str;
            config_params[7] = NULL;
            config_params[8] = NULL;
        }
        config_params[9] = mv->units;

        res = PQexecPrepared(mod->conn, "record_diag_test_config",
                             NUM_TEST_CONFIG_PARAMS, config_params, NULL,
                             NULL, 0);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_RDT(PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
        SAFEFREE(type_str);
        SAFEFREE(value_str);
    }

    if (mod->autocommit) {
        res = PQexec(mod->conn, "commit");
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_RDT("Unable to commit transaction: %s",
                            PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "postgres_record_diag_test succeeded");

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        res = PQexec(mod->conn, "rollback");
        if (!status_ok(res)) {
            postgres_reconnect_if_needed(mod);
        }
    }
    if (NULL != res) {
        PQclear(res);
    }
    SAFEFREE(index_str);
    SAFEFREE(type_str);
    SAFEFREE(value_str);
    return rc;
}

static int postgres_store_diag_test(mca_db_postgres_module_t *mod,
                                    opal_list_t *input,
                                    opal_list_t *ret)
{
    int rc = ORCM_SUCCESS;

    const int NUM_PARAMS = 7;
    const char *params[] = {
        "hostname",
        "diag_type",
        "diag_subtype",
        "start_time",
        "test_result",
        "end_time",
        "component_index"
    };
    opal_value_t *param_items[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    opal_bitmap_t item_bm;
    size_t num_params_found;
    size_t num_items;

    const int NUM_TEST_RESULT_PARAMS = 7;
    const int NUM_TEST_CONFIG_PARAMS = 10;

    const char *result_params[NUM_TEST_RESULT_PARAMS];
    const char *config_params[NUM_TEST_CONFIG_PARAMS];

    char *hostname = NULL;
    char *diag_type = NULL;
    char *diag_subtype = NULL;
    char *test_result = NULL;
    int *component_index = NULL;

    char *index_str = NULL;
    char *type_str = NULL;
    char *value_str = NULL;
    char start_time_str[TIMESTAMP_STR_LENGTH];
    char end_time_str[TIMESTAMP_STR_LENGTH];

    orcm_value_t *mv;
    opal_value_t *kv;
    orcm_db_item_t item;
    int i;

    PGresult *res = NULL;

    bool local_tran_started = false;

    num_items = opal_list_get_size(input);
    OBJ_CONSTRUCT(&item_bm, opal_bitmap_t);
    opal_bitmap_init(&item_bm, (int)num_items);

    /* Get the main parameters from the list */
    num_params_found = orcm_util_find_items(params, NUM_PARAMS, input, param_items,
                                  &item_bm);

    /* Check the parameters */
    if (NULL == param_items[0]) {
        ERR_MSG_RDT("No hostname provided");
        OBJ_DESTRUCT(&item_bm);
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == param_items[1]) {
        ERR_MSG_RDT("No diagnostic type provided");
        OBJ_DESTRUCT(&item_bm);
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == param_items[2]) {
        ERR_MSG_RDT("No diagnostic subtype provided");
        OBJ_DESTRUCT(&item_bm);
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == param_items[3]) {
        ERR_MSG_RDT("No start time provided");
        OBJ_DESTRUCT(&item_bm);
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == param_items[4]) {
        ERR_MSG_RDT("No test result provided");
        OBJ_DESTRUCT(&item_bm);
        return ORCM_ERR_BAD_PARAM;
    }

    kv = param_items[0];
    if (OPAL_STRING == kv->type) {
        hostname = kv->data.string;
    } else {
        ERR_MSG_RDT("Invalid value type specified for hostname");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[1];
    if (OPAL_STRING == kv->type) {
        diag_type = kv->data.string;
    } else {
        ERR_MSG_RDT("Invalid value type specified for diagnostic "
                      "type");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[2];
    if (OPAL_STRING == kv->type) {
        diag_subtype = kv->data.string;
    } else {
        ERR_MSG_RDT("Invalid value type specified for diagnostic "
                      "subtype");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[3];
    switch (kv->type) {
    case OPAL_TIMEVAL:
    case OPAL_TIME:
        if (!tv_to_str_time_stamp(&kv->data.tv, start_time_str,
                                  sizeof(start_time_str))) {
            ERR_MSG_RDT("Failed to convert start time stamp "
                          "value");
            rc = ORCM_ERR_BAD_PARAM;
            goto cleanup_and_exit;
        }
        break;
    case OPAL_STRING:
        strncpy(start_time_str, kv->data.string,
                sizeof(start_time_str) - 1);
        start_time_str[sizeof(start_time_str) - 1] = '\0';
        break;
    default:
        ERR_MSG_RDT("Invalid value type specified for start time");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[4];
    if (OPAL_STRING == kv->type) {
        test_result = kv->data.string;
    } else {
        ERR_MSG_RDT("Invalid value type specified for test result");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    if (NULL != (kv = param_items[5])) {
        switch (kv->type) {
        case OPAL_TIMEVAL:
        case OPAL_TIME:
            if (!tv_to_str_time_stamp(&kv->data.tv, end_time_str,
                                      sizeof(end_time_str))) {
                ERR_MSG_RDT("Failed to convert end time stamp value");
                rc = ORCM_ERR_BAD_PARAM;
                goto cleanup_and_exit;
            }
            break;
        case OPAL_STRING:
            strncpy(end_time_str, kv->data.string,
                    sizeof(end_time_str) - 1);
            end_time_str[sizeof(end_time_str) - 1] = '\0';
            break;
        default:
            ERR_MSG_RDT("Invalid value type specified for end time");
            rc = ORCM_ERR_BAD_PARAM;
            goto cleanup_and_exit;
        }
    }

    if (NULL != (kv = param_items[6])) {
        if (OPAL_INT == kv->type) {
            component_index = &kv->data.integer;
        } else {
            ERR_MSG_RDT("Invalid value type specified for component "
                          "index");
            rc = ORCM_ERR_BAD_PARAM;
            goto cleanup_and_exit;
        }
    }

    /* Prepare the statement only if it hasn't already been prepared */
    if (!mod->prepared[ORCM_DB_PG_STMT_RECORD_DIAG_TEST_RESULT]) {
        /*
         * $1: hostname
         * $2: diagnostic type
         * $3: diagnostic subtype
         * $4: start time
         * $5: end time
         * $6: component index
         * $7: test result
         */
        res = PQprepare(mod->conn, "record_diag_test_result",
                        "select record_diag_test_result("
                        "$1, $2, $3, $4, $5, $6, $7)",
                        0, NULL);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_RDT(PQresultErrorMessage(res));
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;

        mod->prepared[ORCM_DB_PG_STMT_RECORD_DIAG_TEST_RESULT] = true;
    }

    if (mod->autocommit || !mod->tran_started) {
        res = PQexec(mod->conn, "begin");
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_RDT("Unable to begin transaction: %s",
                            PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;

        local_tran_started = true;
    }

    if (NULL == component_index) {
        index_str = NULL;
    } else {
        asprintf(&index_str, "%d", *component_index);
    }


    /* Build and execute the SQL command to store the diagnostic test result */
    result_params[0] = hostname;
    result_params[1] = diag_type;
    result_params[2] = diag_subtype;
    result_params[3] = start_time_str;
    if (NULL != param_items[5]) {
        result_params[4] = end_time_str;
    } else {
        result_params[4] = NULL;
    }
    result_params[5] = index_str;
    result_params[6] = test_result;

    res = PQexecPrepared(mod->conn, "record_diag_test_result",
                         NUM_TEST_RESULT_PARAMS, result_params, NULL, NULL, 0);
    if (!status_ok(res)) {
        rc = ORCM_ERROR;
        ERR_MSG_RDT(PQresultErrorMessage(res));
        postgres_reconnect_if_needed(mod);
        goto cleanup_and_exit;
    }
    PQclear(res);
    res = NULL;
    SAFEFREE(index_str);

    if (num_items <= num_params_found) {
        /* No test parameters provided, we're done! */
        if (mod->autocommit) {
            res = PQexec(mod->conn, "commit");
            if (!status_ok(res)) {
                rc = ORCM_ERROR;
                ERR_MSG_FMT_RDT("Unable to commit transaction: %s",
                                PQresultErrorMessage(res));
                postgres_reconnect_if_needed(mod);
                goto cleanup_and_exit;
            }
            PQclear(res);
            res = NULL;
        }

        opal_output_verbose(2, orcm_db_base_framework.framework_output,
                            "postgres_record_diag_test succeeded");
        goto cleanup_and_exit;
    }

    /* Prepare the statement only if it hasn't already been prepared */
    if (!mod->prepared[ORCM_DB_PG_STMT_RECORD_DIAG_TEST_CONFIG]) {
        /*
         * $1: hostname
         * $2: diagnostic type
         * $3: diagnostic subtype
         * $4: start time
         * $5: test parameter
         * $6: data type
         * $7: integer value
         * $8: real value
         * $9: string value
         * $10: units
         */
        res = PQprepare(mod->conn, "record_diag_test_config",
                        "select record_diag_test_config("
                        "$1, $2, $3, $4, $5, $6, $7, $8, $9, $10)",
                        0, NULL);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_RDT(PQresultErrorMessage(res));
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;

        mod->prepared[ORCM_DB_PG_STMT_RECORD_DIAG_TEST_CONFIG] = true;
    }

    /* Build and execute the SQL commands to store the test parameters (if
     * provided in the list) */
    config_params[0] = hostname;
    config_params[1] = diag_type;
    config_params[2] = diag_subtype;
    config_params[3] = start_time_str;
    i = 0;
    OPAL_LIST_FOREACH(mv, input, orcm_value_t) {
        /* Ignore the items that have already been processed */
        if (opal_bitmap_is_set_bit(&item_bm, i)) {
            i++;
            continue;
        }

        if (NULL == mv->value.key || 0 == strlen(mv->value.key)) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_RDT("Key or node feature name not provided for value");
            goto cleanup_and_exit;
        }

        if (ORCM_SUCCESS != opal_value_to_orcm_db_item(&mv->value, &item)) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_FMT_RDT("Unsupported data type: %s",
                            opal_dss.lookup_data_type(mv->value.type));
            goto cleanup_and_exit;
        }

        config_params[4] = mv->value.key;
        asprintf(&type_str, "%d", item.opal_type);
        config_params[5] = type_str;
        switch (item.item_type) {
        case ORCM_DB_ITEM_STRING:
            config_params[6] = NULL;
            config_params[7] = NULL;
            config_params[8] = item.value.value_str;
            break;
        case ORCM_DB_ITEM_REAL:
            config_params[6] = NULL;
            asprintf(&value_str, "%f", item.value.value_real);
            config_params[7] = value_str;
            config_params[8] = NULL;
            break;
        default:
            asprintf(&value_str, "%lld", item.value.value_int);
            config_params[6] = value_str;
            config_params[7] = NULL;
            config_params[8] = NULL;
        }
        config_params[9] = mv->units;

        res = PQexecPrepared(mod->conn, "record_diag_test_config",
                             NUM_TEST_CONFIG_PARAMS, config_params, NULL,
                             NULL, 0);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_RDT(PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
        SAFEFREE(type_str);
        SAFEFREE(value_str);
        i++;
    }

    if (mod->autocommit) {
        res = PQexec(mod->conn, "commit");
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_RDT("Unable to commit transaction: %s",
                            PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "postgres_record_diag_test succeeded");

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        res = PQexec(mod->conn, "rollback");
        if (!status_ok(res)) {
            postgres_reconnect_if_needed(mod);
        }
    }
    if (NULL != res) {
        PQclear(res);
    }
    SAFEFREE(index_str);
    SAFEFREE(type_str);
    SAFEFREE(value_str);
    OBJ_DESTRUCT(&item_bm);
    return rc;
}

#define ERR_MSG_SE(msg) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:postgres: Unable to store event: "); \
    opal_output(0, msg); \
    opal_output(0, "***********************************************");

#define ERR_MSG_FMT_SE(msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:postgres: Unable to store event: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "***********************************************");


static int add_event(mca_db_postgres_module_t* mod,
        opal_list_t *input, opal_bitmap_t* item_bm,
        long long int* event_id_once_added) {
    int rc = ORCM_SUCCESS;

    const int NUM_PARAMS = 6;
    char event_str_timestamp[TIMESTAMP_STR_LENGTH];
    char *severity = NULL;
    char *event_type = NULL;
    char *version = NULL;
    char *vendor = NULL;
    char *description = NULL;
    opal_value_t *kv = NULL;
    const int NUM_ADD_EVENT_PARAMS = 6;
    const char *add_event_params[NUM_ADD_EVENT_PARAMS];

    const char *params[] = {
        "ctime",
        "severity",
        "type",
        "version",
        "vendor",
        "description"
    };
    opal_value_t *param_items[] = {NULL, NULL, NULL, NULL, NULL, NULL};
    PGresult *res = NULL;

    /* Get the main parameters from the list */
    orcm_util_find_items(params, NUM_PARAMS, input, param_items, item_bm);

    /* Check parameters */
    if (NULL == param_items[0]) {
        ERR_MSG_SE("No time stamp provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }
    if (NULL == param_items[1]) {
        ERR_MSG_SE("No severity level provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }
    if (NULL == param_items[2]) {
        ERR_MSG_SE("No even type provided");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }

    kv = param_items[0];
    if (OPAL_TIMEVAL == kv->type) {
        if (!tv_to_str_time_stamp(&kv->data.tv, event_str_timestamp,
                                  sizeof(event_str_timestamp))) {
            ERR_MSG_SE("Failed to convert time stamp value");
            rc = ORCM_ERR_BAD_PARAM;
            goto cleanup_and_exit;
        }
    } else {
        ERR_MSG_SE("Invalid value type specified for event timestamp");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }
    kv = param_items[1];
    if (OPAL_STRING == kv->type) {
        severity = kv->data.string;
    } else {
        ERR_MSG_SE("Invalid value type specified for event severity level");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }
    kv = param_items[2];
    if (OPAL_STRING == kv->type) {
        event_type = kv->data.string;
    } else {
        ERR_MSG_SE("Invalid value type specified for event type level");
        rc = ORCM_ERR_BAD_PARAM;
        goto cleanup_and_exit;
    }
    kv = param_items[3];
    if (NULL != kv && OPAL_STRING == kv->type) {
        version = kv->data.string;
    } else {
        version = "";
    }
    kv = param_items[4];
    if (NULL != kv && OPAL_STRING == kv->type) {
        vendor = kv->data.string;
    } else {
        vendor = "";
    }
    kv = param_items[5];
    if (NULL != kv && OPAL_STRING == kv->type) {
        description = kv->data.string;
    } else {
        description = "";
    }

    /* add_event parameters order
     * $1: time_stamp
     * $2: severity
     * $3: type
     * $4: version
     * $5: vendor
     * $6: description
     */
    add_event_params[0] = event_str_timestamp;
    add_event_params[1] = severity;
    add_event_params[2] = event_type;
    add_event_params[3] = version;
    add_event_params[4] = vendor;
    add_event_params[5] = description;
    /* Prepare the statement only if it hasn't already been prepared */
    if (!mod->prepared[ORCM_DB_PG_STMT_ADD_EVENT]) {
        res = PQprepare(mod->conn, "add_event",
                        "SELECT add_event($1, $2, $3, $4, $5, $6)",
                        0, NULL);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_SE(PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
        mod->prepared[ORCM_DB_PG_STMT_ADD_EVENT] = true;
    }
    res = PQexecPrepared(mod->conn, "add_event", NUM_ADD_EVENT_PARAMS,
                         add_event_params, NULL, NULL, 0);
    if (!status_ok(res)) {
        rc = ORCM_ERROR;
        ERR_MSG_SE(PQresultErrorMessage(res));
        postgres_reconnect_if_needed(mod);
        goto cleanup_and_exit;
    }
    // the add_event is expected to return only the event ID that just got added.
    *event_id_once_added = atoll(PQgetvalue(res, 0, 0));
    if (0 >= *event_id_once_added) {
        rc = ORCM_ERROR;
        ERR_MSG_SE("Add event failed to return the new event ID");
    }

cleanup_and_exit:
    if (NULL != res) {
        PQclear(res);
        res = NULL;
    }

    return rc;
}

static int postgres_store_event(mca_db_postgres_module_t *mod,
                                opal_list_t *input,
                                opal_list_t *out)
{
    int rc = ORCM_SUCCESS;

    opal_bitmap_t item_bm;
    size_t num_items;
    const int NUM_ADD_EVENT_DATA_PARAMS = 7;
    const char *add_event_data_params[NUM_ADD_EVENT_DATA_PARAMS];
    long long int event_id_once_added = -1;
    orcm_value_t *mv = NULL;
    orcm_db_item_t item;
    char *type_str = NULL;
    char *value_str = NULL;
    char *event_id_once_added_str = NULL;
    int i;

    PGresult *res = NULL;

    bool local_tran_started = false;

    if (NULL == input) {
        ERR_MSG_SE("No parameters provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (mod->autocommit || !mod->tran_started) {
        res = PQexec(mod->conn, "BEGIN");
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_SE("Unable to begin transaction: %s",
                    PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
        local_tran_started = true;
    }

    num_items = opal_list_get_size(input);
    OBJ_CONSTRUCT(&item_bm, opal_bitmap_t);
    opal_bitmap_init(&item_bm, (int)num_items);
    rc = add_event(mod, input, &item_bm, &event_id_once_added);
    if (ORCM_SUCCESS != rc || 0 > event_id_once_added) {
        ERR_MSG_SE("Failed to store event");
        goto cleanup_and_exit;
    }

    /* Prepare the statement only if it hasn't already been prepared */
    if (!mod->prepared[ORCM_DB_PG_STMT_ADD_EVENT_DATA]) {
        /* add_event_data parameters order
         *
         * $1: event_id,
         * $2: key_name
         * $3: app_value_type_id
         * $4: value_int
         * $5: value_real
         * $6: value_str
         * $7: units
         */
        res = PQprepare(mod->conn, "add_event_data",
                        "SELECT add_event_data($1, $2, $3, $4, $5, $6, $7)",
                        0, NULL);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_SE(PQresultErrorMessage(res));
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;

        mod->prepared[ORCM_DB_PG_STMT_ADD_EVENT_DATA] = true;
    }

    /* Build and execute the SQL commands to store the data in the list */
    asprintf(&event_id_once_added_str, "%lld", event_id_once_added);
    add_event_data_params[0] = event_id_once_added_str;
    i = 0;
    OPAL_LIST_FOREACH(mv, input, orcm_value_t) {
        /* Ignore the items that have already been processed. */
        if (opal_bitmap_is_set_bit(&item_bm, i)) {
            i++;
            continue;
        }

        if (NULL == mv->value.key || 0 == strlen(mv->value.key)) {
            rc = ORCM_ERR_BAD_PARAM;
            ERR_MSG_SE("Event data key not provided for value");
            goto cleanup_and_exit;
        }

        if (ORCM_SUCCESS != opal_value_to_orcm_db_item(&mv->value, &item)) {
            rc = ORCM_ERR_NOT_SUPPORTED;
            ERR_MSG_FMT_SE("Unsupported data type: %s",
                            opal_dss.lookup_data_type(mv->value.type));
            goto cleanup_and_exit;
        }

        add_event_data_params[1] = mv->value.key;
        asprintf(&type_str, "%d", item.opal_type);
        add_event_data_params[2] = type_str;
        switch (item.item_type) {
        case ORCM_DB_ITEM_STRING:
            add_event_data_params[3] = NULL;
            add_event_data_params[4] = NULL;
            add_event_data_params[5] = item.value.value_str;
            break;
        case ORCM_DB_ITEM_REAL:
            add_event_data_params[3] = NULL;
            asprintf(&value_str, "%f", item.value.value_real);
            add_event_data_params[4] = value_str;
            add_event_data_params[5] = NULL;
            break;
        default:
            asprintf(&value_str, "%lld", item.value.value_int);
            add_event_data_params[3] = value_str;
            add_event_data_params[4] = NULL;
            add_event_data_params[5] = NULL;
        }
        if (NULL != mv->units) {
            add_event_data_params[6] = mv->units;
        } else {
            add_event_data_params[6] = "";
        }

        res = PQexecPrepared(mod->conn, "add_event_data", NUM_ADD_EVENT_DATA_PARAMS,
                             add_event_data_params, NULL, NULL, 0);
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_SE(PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
        SAFEFREE(type_str);
        SAFEFREE(value_str);
        i++;
    }

    if (mod->autocommit) {
        res = PQexec(mod->conn, "COMMIT");
        if (!status_ok(res)) {
            rc = ORCM_ERROR;
            ERR_MSG_FMT_SE("Unable to commit transaction: %s",
                            PQresultErrorMessage(res));
            postgres_reconnect_if_needed(mod);
            goto cleanup_and_exit;
        }
        PQclear(res);
        res = NULL;
    }

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "postgres_add_event succeeded");

cleanup_and_exit:
    /* If we're in auto commit mode, then make sure our local changes
     * are either committed or canceled. */
    if (ORCM_SUCCESS != rc && mod->autocommit && local_tran_started) {
        res = PQexec(mod->conn, "ROLLBACK");
        if (!status_ok(res)) {
            postgres_reconnect_if_needed(mod);
        }
    }
    if (NULL != res) {
        PQclear(res);
        res = NULL;
    }
    SAFEFREE(type_str);
    SAFEFREE(value_str);
    SAFEFREE(event_id_once_added_str);
    OBJ_DESTRUCT(&item_bm);
    return rc;
}

#define ERR_MSG_COMMIT(msg) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:postgres: Unable to commit current transaction: "); \
    opal_output(0, msg); \
    opal_output(0, "***********************************************");

static int postgres_commit(struct orcm_db_base_module_t *imod)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;
    PGresult *res;

    res = PQexec(mod->conn, "commit");
    if (!status_ok(res)) {
        ERR_MSG_COMMIT(PQresultErrorMessage(res));
        PQclear(res);
        postgres_reconnect_if_needed(mod);
        return ORCM_ERROR;
    }
    PQclear(res);

    mod->tran_started = false;

    return ORCM_SUCCESS;
}

#define ERR_MSG_ROLLBACK(msg) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:postgres: Unable to cancel current transaction: "); \
    opal_output(0, msg); \
    opal_output(0, "***********************************************");

static int postgres_rollback(struct orcm_db_base_module_t *imod)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;
    PGresult *res;

    res = PQexec(mod->conn, "rollback");
    if (!status_ok(res)) {
        ERR_MSG_ROLLBACK(PQresultErrorMessage(res));
        PQclear(res);
        postgres_reconnect_if_needed(mod);
        return ORCM_ERROR;
    }
    PQclear(res);

    mod->tran_started = false;

    return ORCM_SUCCESS;
}

static inline bool status_ok(PGresult *res)
{
    ExecStatusType status = PQresultStatus(res);
    return (status == PGRES_COMMAND_OK
            || status == PGRES_TUPLES_OK
            || status == PGRES_NONFATAL_ERROR);
}

static inline bool is_fatal(PGresult *res)
{
    return (NULL == res || PGRES_FATAL_ERROR == PQresultStatus(res));
}

#define ERR_MSG_FMT_FETCH(msg, ...) \
    opal_output(0, "***********************************************"); \
    opal_output(0, "db:postgres: Unable to fetch data: "); \
    opal_output(0, msg, ##__VA_ARGS__); \
    opal_output(0, "***********************************************");

/**************************************************************************************************
 * If fetch features are added or column names change or opal types change for a column name
 * change here!!!
 **************************************************************************************************/
extern const char *opal_type_column_name;
extern const char *value_column_names[];
extern const opal_data_type_t value_column_types[];

static int get_str_column(PGresult* results)
{
    return PQfnumber(results, value_column_names[VALUE_STR_COLUMN_NUM]);
}

static int get_real_column(PGresult* results)
{
    return PQfnumber(results, value_column_names[VALUE_REAL_COLUMN_NUM]);
}

static int get_int_column(PGresult* results)
{
    return PQfnumber(results, value_column_names[VALUE_INT_COLUMN_NUM]);
}

static opal_data_type_t get_opal_type_from_postgres_type(Oid pg_type)
{
    switch(pg_type) {
    case BOOLOID:
        return OPAL_BOOL;
    case BYTEAOID:
        return OPAL_BYTE;
    case CHAROID:
        return OPAL_INT8;
    case NAMEOID:
    case BPCHAROID:
    case VARCHAROID:
    case TSVECTOROID:
    case CSTRINGOID:
        return OPAL_STRING;
    case INT8OID:
        return OPAL_INT64;
    case INT2OID:
        return OPAL_INT16;
    case INT2VECTOROID:
    case INT2ARRAYOID:
        return OPAL_INT16_ARRAY;
    case INT4OID:
        return OPAL_INT32;
    case TEXTOID:
        return OPAL_STRING;
    case OIDOID:
    case XIDOID:
    case CIDOID:
    case REGTYPEOID:
        return OPAL_UINT32;
    case TIDOID:
    case BITOID:
    case VARBITOID:
    case UUIDOID:
        return OPAL_UINT8_ARRAY;
    case OIDVECTOROID:
    case INT4ARRAYOID:
    case OIDARRAYOID:
    case REGTYPEARRAYOID:
        return OPAL_UINT32_ARRAY;
    case FLOAT4OID:
        return OPAL_FLOAT;
    case FLOAT8OID:
        return OPAL_DOUBLE;
    case ABSTIMEOID:
    case TIMESTAMPOID:
    case TIMESTAMPTZOID:
    case DATEOID:
        return OPAL_TIMEVAL;
    case TEXTARRAYOID:
    case CSTRINGARRAYOID:
        return OPAL_STRING_ARRAY;
    case FLOAT4ARRAYOID:
        return OPAL_FLOAT_ARRAY;
    case TIMETZOID:
    case INTERVALOID:
    case TIMEOID:
        return OPAL_TIME;
    default:
        return OPAL_UNDEF;
    }
}

/**************************************************************************************************/

#define NO_ROWS     (-1)
#define NO_COLUMN   NO_ROWS

static int postgres_fetch(struct orcm_db_base_module_t *imod,
                         const char *view,
                         opal_list_t *filters,
                         opal_list_t *kvs)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;
    int rc = ORCM_SUCCESS;
    PGresult *results = NULL;
    int handle = -1;
    opal_value_t *result = NULL;
    char* query = NULL;

    if(NULL == view || 0 == strlen(view)) {
        ERR_MSG_FMT_FETCH("database view passed was %s or empty!", "NULL");
        return ORCM_ERR_NOT_IMPLEMENTED;
    }

    if(NULL == kvs) {
        ERR_MSG_FMT_FETCH("Argument 'kvs' passed was NULL but was expected to be valid opal_list_t pointer: %d", 0);
        rc = ORCM_ERROR;
        goto cleanup_and_exit;
    }
    query = build_query_from_view_name_and_filters(view, filters);
    if(NULL == query) {
        ERR_MSG_FMT_FETCH("build_query_from_view_name_and_filters returned: %s", "NULL");
        rc = ORCM_ERROR;
        goto cleanup_and_exit;
    }
    results = PQexec(mod->conn, query);
    if(!status_ok(results)) {
        ERR_MSG_FMT_FETCH("PQexec returned: %s", "NULL");
        rc = ORCM_ERROR;
        postgres_reconnect_if_needed(mod);
        goto cleanup_and_exit;
    }

    /* Add new results set handle */
    handle = opal_pointer_array_add(mod->results_sets, (void*)results);
    if(0 > handle) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_FETCH("opal_pointer_array_add returned: %d", handle);
        goto cleanup_and_exit;
    }
    mod->current_row = 0; /* Simulate 'get_next_row' functionality. */

    /* Create returned handle object */
    result = OBJ_NEW(opal_value_t);
    result->type = OPAL_INT;
    result->data.integer = handle;
    opal_list_append(kvs, (void*)result); /* takes ownership */

cleanup_and_exit:
    SAFEFREE(query);
    return rc;
}

static int postgres_get_num_rows(struct orcm_db_base_module_t *imod,
                                 int rshandle,
                                 int *num_rows)
{
    int rc = ORCM_ERROR;
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;
    PGresult *results = (PGresult*)opal_pointer_array_get_item(mod->results_sets, rshandle);
    if(NULL != results) {
        int row_count = PQntuples(results);
        if(NO_ROWS <= row_count) { /* No rows (0) is OK */
            *num_rows = row_count;
            rc = ORCM_SUCCESS;
        }
    }
    return rc;
}

static opal_value_t *get_value_object(PGresult *results, int row, opal_data_type_t type)
{
    if(OPAL_STRING == type && NO_COLUMN == get_str_column(results)) {
        return NULL;
    }
    if((OPAL_FLOAT == type || OPAL_DOUBLE == type) && NO_COLUMN == get_real_column(results)) {
        return NULL;
    }
    if(true == is_supported_opal_int_type(type) && NO_COLUMN == get_real_column(results)) {
        return NULL;
    }
    opal_value_t *rv = OBJ_NEW(opal_value_t);
    rv->type = type;
    rv->key = strdup("value");
    switch(type) { /* all PQgetvalue buffers are owned by PQ */
        case OPAL_BYTE:
            rv->data.byte = *((uint8_t*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_BOOL:
            rv->data.flag = ((*((uint8_t*)PQgetvalue(results, row, get_int_column(results)))) == 0)?false:true;
            break;
        case OPAL_STRING:
            rv->data.string = strdup(PQgetvalue(results, row, get_str_column(results)));
            break;
        case OPAL_SIZE:
            rv->data.size = *((size_t*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_PID:
            rv->data.pid = *((pid_t*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_INT:
            rv->data.integer = *((int*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_INT8:
            rv->data.int8 = *((int8_t*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_INT16:
            rv->data.int16 = *((int16_t*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_INT32:
            rv->data.int32 = *((int32_t*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_INT64:
            rv->data.int64 = *((int64_t*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_UINT:
            rv->data.uint = *((unsigned int*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_UINT8:
            rv->data.uint8 = *((uint8_t*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_UINT16:
            rv->data.uint16 = *((uint16_t*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_UINT32:
            rv->data.uint32 = *((uint32_t*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_UINT64:
            rv->data.uint64 = *((uint64_t*)PQgetvalue(results, row, get_int_column(results)));
            break;
        case OPAL_FLOAT:
            rv->data.fval = (float)*((double*)PQgetvalue(results, row, get_real_column(results)));
            break;
        case OPAL_DOUBLE:
            rv->data.dval = *((double*)PQgetvalue(results, row, get_real_column(results)));
            break;
        default: /* No supported basic data type so ignore it */
            ERR_MSG_FMT_FETCH("An unsupported orcm value type was encountered: %d", type);
            OBJ_RELEASE(rv);
            return NULL;
    }

    return rv;
}

static int postgres_get_next_row(struct orcm_db_base_module_t *imod,
                                 int rshandle,
                                 opal_list_t *row)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;
    PGresult *results = (PGresult*)opal_pointer_array_get_item(mod->results_sets, rshandle);
    int cols_count = 0;
    int data_type_col = NO_COLUMN;
    int value_col = NO_COLUMN;
    opal_data_type_t data_type = OPAL_UNDEF;
    int data_length = -1;
    opal_value_t *value_object = NULL;
    bool ignore_column = false;

    if(NULL == results) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_FETCH("Bad results handle: %d", rshandle);
        goto next_cleanup;
    }
    data_type_col = PQfnumber(results, opal_type_column_name);
    if (NO_COLUMN != data_type_col) {
        data_type = atoi(PQgetvalue(results, mod->current_row, data_type_col));
    } else {
        for (int j = 0; NULL != value_column_names[j]; ++j) {
            value_col = PQfnumber(results, value_column_names[j]);
            if (NO_COLUMN != value_col) {
                data_type = value_column_types[j];
                data_length = PQgetlength(results, mod->current_row, value_col);
                if (0 < data_length) {
                    break;
                }
            }
        }
    }
    if (OPAL_UNDEF != data_type) {
        /* retrieve 1 correctly typed object for one of 'value_str', 'value_int', 'value_real' */
        value_object = get_value_object(results, mod->current_row, data_type);
        opal_list_append(row, &value_object->super);
        data_type_col = NO_COLUMN;
    }

    /* Get row general column data */
    cols_count = PQnfields(results);
    for(int i = 0; i < cols_count; ++i) {
        Oid pg_type;
        opal_value_t* kv = NULL;
        opal_data_type_t type = OPAL_UNDEF;
        char* name_ref = NULL;
        if(i == data_type_col) { /* skip data_type_id column */
            continue;
        }

        name_ref = PQfname(results, i);
        for (int k = 0; NULL != value_column_names[k]; ++k) {
            if (0 == strcmp(name_ref, value_column_names[k])) {
                ignore_column = true;
                break;
            }
        }
        if (ignore_column) {
            ignore_column = false;
            continue; /* skip column already processed above. */
        }

        pg_type = PQftype(results, i);
        pg_type = PQftype(results, i);
        type = get_opal_type_from_postgres_type(pg_type);
        if(OPAL_UNDEF == type) {
            ERR_MSG_FMT_FETCH("An unrecognized column name '%s' was encountered and skipped during a call to 'get_next_row'", name_ref);
            continue; /* skip unknown fields */
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup(name_ref);
        kv->type = type;
        switch(type) {
            case OPAL_STRING:
                kv->data.string = strdup(PQgetvalue(results, mod->current_row, i));
                break;
            case OPAL_INT:
                kv->data.integer = atoi(PQgetvalue(results, mod->current_row, i));
                break;
            case OPAL_TIMEVAL:
                time_stamp_str_to_tv(PQgetvalue(results, mod->current_row, i), &kv->data.tv);
                break;
        }
        opal_list_append(row, &kv->super);
    }
    ++(mod->current_row); /* move to next row */

next_cleanup:
    return rc;
}

static int postgres_close_result_set(struct orcm_db_base_module_t *imod,
                                     int rshandle)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;
    PGresult *results = (PGresult*)opal_pointer_array_get_item(mod->results_sets, rshandle);

    if(NULL == results) {
        ERR_MSG_FMT_FETCH("Bad results handle: %d", rshandle);
        PQclear(results);
        return ORCM_ERROR;
    }

    PQclear(results);

    opal_pointer_array_set_item(mod->results_sets, rshandle, NULL);

    return ORCM_SUCCESS;
}



char* format_string_to_sql(opal_data_type_t type, char* raw_string){
    char* formated_string = NULL;
    char* escaped_raw_string = NULL;

    switch(type){
        case ORCM_DB_QRY_DATE:
            if(NULL == raw_string)
                asprintf(&formated_string,"\'%s\'::TIMESTAMP","INFINITY");
            else
                asprintf(&formated_string,"\'%s\'::TIMESTAMP",raw_string);
            break;
        case ORCM_DB_QRY_FLOAT:
            if(NULL == raw_string)
                asprintf(&formated_string,"%s::DOUBLE","NULL");
            else
                asprintf(&formated_string,"%s::DOUBLE",raw_string);
            break;
        case ORCM_DB_QRY_INTEGER:
            if(NULL == raw_string)
                asprintf(&formated_string,"%s::INT","NULL");
            else
                asprintf(&formated_string,"%s::INT",raw_string);
            break;
        case ORCM_DB_QRY_INTERVAL:
            if(NULL == raw_string)
                formated_string = strdup("NULL::INTERVAL");
            else
                asprintf(&formated_string,"\'%s\'::INTERVAL",raw_string);
            break;
        case ORCM_DB_QRY_OCOMMA_LIST:
        case ORCM_DB_QRY_STRING:
            if(NULL == raw_string)
                formated_string = strdup("NULL");
            else{
                postgres_escape_string(raw_string, &escaped_raw_string);
                asprintf(&formated_string,"\'%s\'",escaped_raw_string);
                SAFEFREE(escaped_raw_string);
            }
            break;
        default:
            formated_string = strdup("");
    }

    return formated_string;
}

char* format_opal_value_as_sql_string(opal_value_t *value){
    char* raw_string = NULL;
    char* formated_string = NULL;

    if(NULL != value->data.string)
        raw_string = strdup(value->data.string);

    formated_string = format_string_to_sql(value->type, raw_string);

    SAFEFREE(raw_string);
    return formated_string;
}

bool build_argument_string(char **argument_string, opal_list_t *argument_list)
{

    bool use_comma = false;
    if(NULL == argument_list) {
        *argument_string = strdup("");
        return false;
    } else {
        opal_value_t* argument = NULL;
        OPAL_LIST_FOREACH(argument, argument_list, opal_value_t) {
            char *old_argument_string = *argument_string;
            char* val = format_opal_value_as_sql_string(argument);
            if(use_comma){
                asprintf(argument_string,"%s,%s",old_argument_string,val);
                SAFEFREE(old_argument_string);
            }
            else{
                asprintf(argument_string,"%s",val);
                use_comma = true;
            }
            SAFEFREE(val);
        }
        return true;
    }
}

char* build_query_from_function_name_and_arguments(const char* function_name, opal_list_t* arguments)
{
    char* query = NULL;
    char* argument_string = NULL;

    if(NULL != function_name && 0 < strlen(function_name)) {
        build_argument_string(&argument_string,arguments);
        asprintf(&query, "select * from %s(%s);", function_name,argument_string);
        SAFEFREE(argument_string);
    }
    return query;
}

int check_for_invalid_characters(const char *function){
    char *invalid_chars = "(;\"\'";
    int function_size, invalid_chars_size;

    function_size = strlen(function);
    invalid_chars_size = strlen(invalid_chars);

    for(int i=0; i < function_size; i++)
        for(int j=0; j < invalid_chars_size; j++)
            if(function[i] == invalid_chars[j])
                return ORCM_ERR_BAD_PARAM;

    return ORCM_SUCCESS;
}


static int postgres_fetch_function(struct orcm_db_base_module_t *imod,
                         const char *function,
                         opal_list_t *arguments,
                         opal_list_t *kvs)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;
    int rc = ORCM_SUCCESS;
    PGresult *results = NULL;
    int handle = -1;
    opal_value_t *result = NULL;
    char* query = NULL;

    if(NULL == function || 0 == strlen(function)) {
        ERR_MSG_FMT_FETCH("database function passed was %s or empty!", "NULL");
        return ORCM_ERR_NOT_IMPLEMENTED;
    }

    if( ORCM_SUCCESS != check_for_invalid_characters(function)){
        ERR_MSG_FMT_FETCH("Function name \"%s\" contains invalid characters",function);
        return ORCM_ERR_BAD_PARAM;
    }

    if(NULL == kvs) {
        ERR_MSG_FMT_FETCH("Argument 'kvs' passed was NULL but was expected to be valid opal_list_t pointer: %d", 0);
        rc = ORCM_ERROR;
        goto cleanup_and_exit;
    }

    query = build_query_from_function_name_and_arguments(function, arguments);

    results = PQexec(mod->conn, query);
    if(!status_ok(results)) {
        ERR_MSG_FMT_FETCH("PQexec returned: %s", "NULL");
        rc = ORCM_ERROR;
        postgres_reconnect_if_needed(mod);
        goto cleanup_and_exit;
    }

    /* Add new results set handle */
    handle = opal_pointer_array_add(mod->results_sets, (void*)results);
    if(0 > handle) {
        rc = ORCM_ERROR;
        ERR_MSG_FMT_FETCH("opal_pointer_array_add returned: %d", handle);
        goto cleanup_and_exit;
    }
    mod->current_row = 0; /* Simulate 'get_next_row' functionality. */

    /* Create returned handle object */
    result = OBJ_NEW(opal_value_t);
    result->type = OPAL_INT;
    result->data.integer = handle;
    opal_list_append(kvs, (void*)result); /* takes ownership */

cleanup_and_exit:
    SAFEFREE(query);
    return rc;
}
