/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
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

#include "libpq-fe.h"

#include "opal_stdint.h"
#include "opal/util/argv.h"
#include "opal/util/error.h"

#include "orcm/runtime/orcm_globals.h"

#include "orcm/mca/db/base/base.h"
#include "db_postgres.h"

#define ORCM_PG_MAX_LINE_LENGTH 4096

static int postgres_init(struct orcm_db_base_module_t *imod);
static void postgres_finalize(struct orcm_db_base_module_t *imod);
static int postgres_store_sample(struct orcm_db_base_module_t *imod,
                                 const char *data_group,
                                 opal_list_t *kvs);
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

/* Internal helper functions */
static void tv_to_str_time_stamp(const struct timeval *time, char *tbuf,
                                 size_t size);
static void tm_to_str_time_stamp(const struct tm *time, char *tbuf,
                                 size_t size);
static inline bool status_ok(PGresult *res);

mca_db_postgres_module_t mca_db_postgres_module = {
    {
        postgres_init,
        postgres_finalize,
        postgres_store_sample,
        NULL,
        postgres_update_node_features,
        postgres_record_diag_test,
        NULL,
        NULL,
        NULL
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
    PGconn *conn;

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
    opal_output_verbose(5, orcm_db_base_framework.framework_output,
                        "db:postgres: Connection established to %s",
                        mod->dbname);

    return ORCM_SUCCESS;
}

static void postgres_finalize(struct orcm_db_base_module_t *imod)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;

    if (NULL != mod->pguri) {
        free(mod->pguri);
    }
    if (NULL != mod->dbname) {
        free(mod->dbname);
    }
    if (NULL != mod->user) {
        free(mod->user);
    }
    if (NULL != mod->pgoptions) {
        free(mod->pgoptions);
    }
    if (NULL != mod->pgtty) {
        free(mod->pgtty);
    }
    if (NULL != mod->conn) {
        PQfinish(mod->conn);
    }
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

static int postgres_store_sample(struct orcm_db_base_module_t *imod,
                                 const char *data_group,
                                 opal_list_t *kvs)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)imod;

    opal_value_t *kv;
    opal_value_t *timestamp_item = NULL;
    opal_value_t *hostname_item = NULL;

    char hostname[256];
    char time_stamp[40];
    char **data_item_parts;
    char *data_item;
    orcm_db_item_t item;
    char *units;
    int count;
    int ret;

    size_t num_items;
    char **rows;
    char *values;
    char *insert_stmt;
    size_t i;

    PGresult *res;

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
                tv_to_str_time_stamp(&kv->data.tv, time_stamp,
                                     sizeof(time_stamp));
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
            opal_argv_free(rows);
            ERR_MSG_STORE("No data item specified");
            return ORCM_ERR_BAD_PARAM;
        }

        ret = opal_value_to_orcm_db_item(kv, &item);
        if (ORCM_SUCCESS != ret) {
            ERR_MSG_FMT_STORE("Unsupported data type: %s",
                              opal_dss.lookup_data_type(kv->type));
            opal_argv_free(rows);
            return ORCM_ERR_NOT_SUPPORTED;
        }

        /* (node,
         *  data_item,
         *  time_stamp,
         *  value_int,
         *  value_real,
         *  value_str,
         *  units,
         *  data_type_id) */
        switch (item.item_type) {
        case ORCM_DB_ITEM_STRING:
            if (NULL != units) {
                asprintf(rows + i,
                         "('%s','%s_%s','%s',NULL,NULL,'%s','%s',%d)",
                         hostname, data_group, data_item, time_stamp,
                         item.value.value_str, units, kv->type);
            } else {
                asprintf(rows + i,
                         "('%s','%s_%s','%s',NULL,NULL,'%s',NULL,%d)",
                         hostname, data_group, data_item, time_stamp,
                         item.value.value_str, kv->type);
            }
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
    }

    values = opal_argv_join(rows, ',');
    opal_argv_free(rows);

    asprintf(&insert_stmt, "insert into data_sample_raw(node,data_item,"
             "time_stamp,value_int,value_real,value_str,units,data_type_id) "
             "values %s", values);
    free(values);

    res = PQexec(mod->conn, insert_stmt);
    free(insert_stmt);

    if (!status_ok(res)) {
        PQclear(res);
        ERR_MSG_FMT_STORE("%s", PQresultErrorMessage(res));
        return ORCM_ERROR;
    }
    PQclear(res);

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "postgres_store_sample succeeded");

    return ORCM_SUCCESS;
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

    orcm_metric_value_t *mv;

    const int NUM_PARAMS = 7;
    const char *params[NUM_PARAMS];
    char *type_str;
    char *value_str;
    orcm_db_item_t item;
    int ret;

    PGresult *res;

    if (NULL == hostname) {
        ERR_MSG_UNF("No hostname provided");
        return ORCM_ERR_BAD_PARAM;
    }

    if (NULL == features) {
        ERR_MSG_UNF("No node features provided");
        return ORCM_ERR_BAD_PARAM;
    }

    /*
     * $1: p_hostname character varying,
     * $2: p_feature character varying,
     * $3: p_data_type_id integer,
     * $4: p_value_int bigint,
     * $5: p_value_real double precision,
     * $6: p_value_str character varying,
     * $7: p_units character varying
     * */
    res = PQprepare(mod->conn, "set_node_feature",
                    "select set_node_feature($1, $2, $3, $4, $5, $6, $7)",
                    0, NULL);
    if (!status_ok(res)) {
        PQclear(res);
        ERR_MSG_FMT_UNF("%s", PQresultErrorMessage(res));
        return ORCM_ERROR;
    }
    PQclear(res);

    res = PQexec(mod->conn, "begin");
    if (!status_ok(res)) {
        PQclear(res);
        ERR_MSG_FMT_UNF("Unable to begin transaction: %s",
                        PQresultErrorMessage(res));
        return ORCM_ERROR;
    }
    PQclear(res);

    params[0] = hostname;
    OPAL_LIST_FOREACH(mv, features, orcm_metric_value_t) {
        if (NULL == mv->value.key || 0 == strlen(mv->value.key)) {
            PQexec(mod->conn, "rollback");
            ERR_MSG_UNF("Key or node feature name not provided for value");
            return ORCM_ERR_BAD_PARAM;
        }

        ret = opal_value_to_orcm_db_item(&mv->value, &item);
        if (ORCM_SUCCESS != ret) {
            PQexec(mod->conn, "rollback");
            ERR_MSG_FMT_UNF("Unsupported data type: %s",
                            opal_dss.lookup_data_type(mv->value.type));
            return ORCM_ERR_NOT_SUPPORTED;
        }

        params[1] = mv->value.key;
        asprintf(&type_str, "%d", item.opal_type);
        params[2] = type_str;
        switch (item.item_type) {
        case ORCM_DB_ITEM_STRING:
            params[3] = NULL;
            params[4] = NULL;
            params[5] = item.value.value_str;
            value_str = NULL;
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

        res = PQexecPrepared(mod->conn, "set_node_feature", NUM_PARAMS,
                             params, NULL, NULL, 0);
        free(type_str);
        if (NULL != value_str) {
            free(value_str);
        }

        if (!status_ok(res)) {
            ERR_MSG_FMT_UNF("%s", PQresultErrorMessage(res));
            return ORCM_ERROR;
        }
        PQclear(res);
    }
    res = PQexec(mod->conn, "commit");
    if (!status_ok(res)) {
        PQclear(res);
        PQexec(mod->conn, "rollback");
        ERR_MSG_FMT_UNF("Unable to commit transaction: %s",
                        PQresultErrorMessage(res));
        return ORCM_ERROR;
    }
    PQclear(res);

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "postgres_update_node_features succeeded");

    return ORCM_SUCCESS;
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

    const int NUM_TEST_RESULT_PARAMS = 7;
    const int NUM_TEST_CONFIG_PARAMS = 10;

    const char *result_params[NUM_TEST_RESULT_PARAMS];
    const char *config_params[NUM_TEST_CONFIG_PARAMS];
    char *index_str;
    char *type_str;
    char *value_str;
    char start_time_str[40];
    char end_time_str[40];

    orcm_metric_value_t *mv;
    orcm_db_item_t item;

    PGresult *res;
    int ret;

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
        PQclear(res);
        ERR_MSG_FMT_RDT("%s", PQresultErrorMessage(res));
        return ORCM_ERROR;
    }
    PQclear(res);

    res = PQexec(mod->conn, "begin");
    if (!status_ok(res)) {
        PQclear(res);
        ERR_MSG_FMT_RDT("Unable to begin transaction: %s",
                        PQresultErrorMessage(res));
        return ORCM_ERROR;
    }
    PQclear(res);

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
    free(index_str);
    if (!status_ok(res)) {
        PQclear(res);
        ERR_MSG_FMT_RDT("%s", PQresultErrorMessage(res));
        return ORCM_ERROR;
    }

    if (NULL == test_params) {
        /* No test parameters provided, we're done! */
        res = PQexec(mod->conn, "commit");
        if (!status_ok(res)) {
            PQclear(res);
            ERR_MSG_FMT_RDT("Unable to commit transaction: %s",
                            PQresultErrorMessage(res));
            return ORCM_ERROR;
        }
        PQclear(res);

        opal_output_verbose(2, orcm_db_base_framework.framework_output,
                            "postgres_record_diag_test succeeded");
        return ORCM_SUCCESS;
    }

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
        PQclear(res);
        PQexec(mod->conn, "rollback");
        ERR_MSG_FMT_RDT("%s", PQresultErrorMessage(res));
        return ORCM_ERROR;
    }
    PQclear(res);

    config_params[0] = hostname;
    config_params[1] = diag_type;
    config_params[2] = diag_subtype;
    config_params[3] = start_time_str;
    OPAL_LIST_FOREACH(mv, test_params, orcm_metric_value_t) {
        if (NULL == mv->value.key || 0 == strlen(mv->value.key)) {
            PQexec(mod->conn, "rollback");
            ERR_MSG_RDT("Key or node feature name not provided for value");
            return ORCM_ERR_BAD_PARAM;
        }

        ret = opal_value_to_orcm_db_item(&mv->value, &item);
        if (ORCM_SUCCESS != ret) {
            PQexec(mod->conn, "rollback");
            ERR_MSG_FMT_RDT("Unsupported data type: %s",
                            opal_dss.lookup_data_type(mv->value.type));
            return ORCM_ERR_NOT_SUPPORTED;
        }

        config_params[4] = mv->value.key;
        asprintf(&type_str, "%d", item.opal_type);
        config_params[5] = type_str;
        switch (item.item_type) {
        case ORCM_DB_ITEM_STRING:
            config_params[6] = NULL;
            config_params[7] = NULL;
            config_params[8] = item.value.value_str;
            value_str = NULL;
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
        free(type_str);
        if (NULL != value_str) {
            free(value_str);
        }

        if (!status_ok(res)) {
            PQclear(res);
            PQexec(mod->conn, "rollback");
            ERR_MSG_FMT_RDT("%s", PQresultErrorMessage(res));
            return ORCM_ERROR;
        }
    }

    res = PQexec(mod->conn, "commit");
    if (!status_ok(res)) {
        PQclear(res);
        PQexec(mod->conn, "rollback");
        ERR_MSG_FMT_RDT("Unable to commit transaction: %s",
                        PQresultErrorMessage(res));
        return ORCM_ERROR;
    }
    PQclear(res);

    opal_output_verbose(2, orcm_db_base_framework.framework_output,
                        "postgres_record_diag_test succeeded");

    return ORCM_SUCCESS;
}

static void tv_to_str_time_stamp(const struct timeval *time, char *tbuf,
                                 size_t size)
{
    struct timeval nrm_time = *time;
    struct tm *tm_info;
    char date_time[30];
    char fraction[10];

    /* Normalize */
    while (nrm_time.tv_usec < 0) {
        nrm_time.tv_usec += 1000000;
        nrm_time.tv_sec--;
    }
    while (nrm_time.tv_usec >= 1000000) {
        nrm_time.tv_usec -= 1000000;
        nrm_time.tv_sec++;
    }

    /* Print in format: YYYY-MM-DD HH:MM:SS.fraction time zone */
    tm_info = localtime(&nrm_time.tv_sec);
    strftime(date_time, sizeof(date_time), "%F %T", tm_info);
    snprintf(fraction, sizeof(fraction), "%f",
             (float)(time->tv_usec / 1000000.0));
    snprintf(tbuf, size, "%s%s", date_time, fraction + 1);
}

static void tm_to_str_time_stamp(const struct tm *time, char *tbuf,
                                 size_t size)
{
    strftime(tbuf, size, "%F %T", time);
}

static inline bool status_ok(PGresult *res)
{
    ExecStatusType status = PQresultStatus(res);
    return status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK;
}
