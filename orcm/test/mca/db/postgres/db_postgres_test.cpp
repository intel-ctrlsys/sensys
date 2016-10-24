/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "db_postgres_test.h"

#include "gtest/gtest.h"
#include "orcm/constants.h"
#include "orcm/test/mca/analytics/util/analytics_util.h"

using namespace std;

extern "C" {
    #include "orcm/mca/db/postgres/db_postgres.h"
}


const char *host = "localhost";
const char *db_name = "orcmdb";
const char *user = "orcmuser:orcm@123";


const char *data_group = "coretemp";
struct timeval start_time;
struct timeval end_time;
double d_value = 10;

const char *type = "type 1";
const char *subtype = "type 2";
const char *test_result = "SUCCESS";
int component_index = 0;

const char *severity = "critical";
const char *version = "version 1";
const char *vendor = "vendor 1";
const char *description = "this is a test";

static mca_db_postgres_module_t* create_postgres_mod(const char* dbname,
                                             const char *user, const char *uri)
{
    mca_db_postgres_module_t *mod = (mca_db_postgres_module_t*)malloc(sizeof(mca_db_postgres_module_t));
    mod->dbname = (NULL != dbname) ? strdup(dbname) : NULL;
    mod->user = (NULL != user) ? strdup(user) : NULL;
    mod->pguri = (NULL != uri) ? strdup(uri) : NULL;
    mod->autocommit = true;
    mod->conn = NULL;
    mod->pgoptions = NULL;
    mod->pgtty = NULL;
    mod->results_sets = NULL;
    return mod;
}


static opal_list_t *create_list_with_mandatory_fields(opal_data_type_t hostname_type, void *hostname,
                                                      opal_data_type_t data_group_type, void *data_group,
                                                      opal_data_type_t ctime_type, void *ctime)
{
    opal_list_t *input_list = OBJ_NEW(opal_list_t);
    orcm_value_t *mv = NULL;

    if (NULL != hostname) {
        mv = analytics_util::load_orcm_value("hostname", hostname, hostname_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != data_group) {
        mv = analytics_util::load_orcm_value("data_group", data_group, data_group_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != ctime) {
        mv = analytics_util::load_orcm_value("ctime", ctime, ctime_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }

    return input_list;
}


void
create_list_with_mandatory_fields_event( opal_list_t *input_list, opal_data_type_t ctime_type, void *ctime,
                                        opal_data_type_t severity_type, void *severity,
                                        opal_data_type_t type_type, void *type,
                                        opal_data_type_t version_type, void *version,
                                        opal_data_type_t vendor_type, void *vendor,
                                        opal_data_type_t description_type, void *description)
{
    orcm_value_t *mv = NULL;

    if (NULL != ctime) {
        mv = analytics_util::load_orcm_value("ctime", ctime, ctime_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != severity) {
        mv = analytics_util::load_orcm_value("severity", severity, severity_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != type) {
        mv = analytics_util::load_orcm_value("type", type, type_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != version) {
        mv = analytics_util::load_orcm_value("version", version, version_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != vendor) {
        mv = analytics_util::load_orcm_value("vendor", vendor, vendor_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != description) {
        mv = analytics_util::load_orcm_value("description", description, description_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }

}

static opal_list_t*
create_list_with_mandatory_fields_diag(opal_data_type_t hostname_type, void *hostname,
                                       opal_data_type_t diag_type_type, void *diag_type,
                                       opal_data_type_t diag_subtype_type, void *diag_subtype,
                                       opal_data_type_t start_time_type, void *start_time,
                                       opal_data_type_t test_result_type, void *test_result,
                                       opal_data_type_t end_time_type, void *end_time,
                                       opal_data_type_t component_index_type, void *component_index)
{
    opal_list_t *input_list = OBJ_NEW(opal_list_t);
    orcm_value_t *mv = NULL;

    if (NULL != hostname) {
        mv = analytics_util::load_orcm_value("hostname", hostname, hostname_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != diag_type) {
        mv = analytics_util::load_orcm_value("diag_type", diag_type, diag_type_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != diag_subtype) {
        mv = analytics_util::load_orcm_value("diag_subtype", diag_subtype, diag_subtype_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != start_time) {
        mv = analytics_util::load_orcm_value("start_time", start_time, start_time_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != test_result) {
        mv = analytics_util::load_orcm_value("test_result", test_result, test_result_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != end_time) {
        mv = analytics_util::load_orcm_value("end_time", end_time, end_time_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    if (NULL != component_index) {
        mv = analytics_util::load_orcm_value("component_index",
                                             component_index, component_index_type, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
    return input_list;
}

static opal_value_t*
create_argument_to_function(char* value, orcm_db_qry_arg_types type){
    opal_value_t *return_item = OBJ_NEW(opal_value_t);
    return_item->type = type;
    return_item->data.string = value;
    return return_item;
}

static opal_list_t* create_list_with_supported_types(){
    opal_list_t *argument_list = OBJ_NEW(opal_list_t);

    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    strdup("value"), ORCM_DB_QRY_DATE));
    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    strdup("value"), ORCM_DB_QRY_FLOAT));
    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    strdup("value"), ORCM_DB_QRY_INTEGER));
    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    strdup("value"), ORCM_DB_QRY_INTERVAL));
    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    strdup("value"), ORCM_DB_QRY_OCOMMA_LIST));
    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    strdup("value"), ORCM_DB_QRY_STRING));
    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    strdup("value"), ORCM_DB_QRY_NULL));

    return argument_list;
}

static opal_list_t* create_list_with_null_values(){
    opal_list_t *argument_list = OBJ_NEW(opal_list_t);

    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    NULL, ORCM_DB_QRY_DATE));
    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    NULL, ORCM_DB_QRY_FLOAT));
    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    NULL, ORCM_DB_QRY_INTEGER));
    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    NULL, ORCM_DB_QRY_INTERVAL));
    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    NULL, ORCM_DB_QRY_OCOMMA_LIST));
    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    NULL, ORCM_DB_QRY_STRING));
    opal_list_append(argument_list, (opal_list_item_t*)create_argument_to_function(
                                    NULL, ORCM_DB_QRY_NULL));
    return argument_list;
}

TEST(ut_db_postgres_tests, init_failure)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, "dummy_host");
    rc = mca_db_postgres_module.api.init((orcm_db_base_module_t*)mod);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    EXPECT_EQ(ORCM_ERR_CONNECTION_FAILED,rc);
}

TEST(ut_db_postgres_tests, init_null_dbname)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(NULL, user, host);
    rc = mca_db_postgres_module.api.init((orcm_db_base_module_t*)mod);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    EXPECT_EQ(ORCM_ERR_BAD_PARAM,rc);
}

TEST(ut_db_postgres_tests, init_null_uri)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, NULL);
    rc = mca_db_postgres_module.api.init((orcm_db_base_module_t*)mod);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    EXPECT_EQ(ORCM_ERR_BAD_PARAM,rc);
}

TEST(ut_db_postgres_tests, init_incorrect_uri)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, "localhost:incorrect:uri");
    rc = mca_db_postgres_module.api.init((orcm_db_base_module_t*)mod);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    EXPECT_EQ(ORCM_ERR_BAD_PARAM,rc);
}

TEST(ut_db_postgres_tests, store_unsupported_type)
{
    orcm_db_data_type_t unknown_type = (orcm_db_data_type_t)4;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    int rc = mca_db_postgres_module.api.store_new(NULL, unknown_type, NULL, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_ERR_NOT_IMPLEMENTED, rc);
}

TEST(ut_db_postgres_tests, store_data_sample_invalid_data_group_type)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    gettimeofday(&start_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)host,
                              OPAL_DOUBLE, (void*)(&d_value), OPAL_TIMEVAL, (void*)(&start_time));
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_data_sample_invalid_time_stamp)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    gettimeofday(&start_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)data_group, OPAL_TIME,NULL);


    orcm_value_t *mv = OBJ_NEW(orcm_value_t);
    mv->value.key = strdup(analytics_util::cppstr_to_cstr("ctime"));
    mv->value.type = OPAL_TIME;
    mv->value.data.tv.tv_sec = LONG_MAX;
    opal_list_append(input_list, (opal_list_item_t*)mv);
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_data_sample_invalid_time_stamp_type)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)data_group, OPAL_DOUBLE, (void*)(&d_value));
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_data_sample_invalid_hostname_type)
{
    int rc = ORCM_SUCCESS;
    const char *current_time = "2016-07-06 12:00:00";
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_STRING, (void*)data_group, OPAL_STRING, (void*)(&current_time));
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_data_sample_no_data_samples)
{
    int rc = ORCM_SUCCESS;
    const char *current_time = "2016-07-06 12:00:00";
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)data_group, OPAL_STRING, (void*)(&current_time));
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_data_sample_event_invalid_time_stamp_type)
{
    int rc = ORCM_SUCCESS;
    const char *current_time = "2016-07-06 12:00:00";
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)data_group, OPAL_STRING, (void*)(&current_time));
    create_list_with_mandatory_fields_event(input_list, OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_STRING, (void*)severity, OPAL_STRING, (void*)type,
                              OPAL_STRING, (void*)version, OPAL_STRING, (void*)vendor,
                              OPAL_STRING, (void*)description);

    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_diag_invalid_hostname_type)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_diag_invalid_diagnostic_type)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)host,
                              OPAL_DOUBLE, (void*)(&d_value), OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_diag_ivalid_diagnostic_subtype)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)type, OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_diag_invalid_start_time)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIME, NULL, OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));

    orcm_value_t *mv = OBJ_NEW(orcm_value_t);
    mv->value.key = strdup("start_time");
    mv->value.type = OPAL_TIME;
    mv->value.data.tv.tv_sec = LONG_MAX;
    opal_list_append(input_list, (opal_list_item_t*)mv);

    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_diag_invalid_start_time_type)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_DOUBLE, (void*)(&d_value), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_diag_invalid_test_results_type)
{
    int rc = ORCM_SUCCESS;
    const char *current_time = "2016-07-06 12:00:00";
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_STRING, (void*)(&current_time), OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_diag_invalid_end_time)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_TIME, NULL, OPAL_INT, (void*)(&component_index));

    orcm_value_t *mv = OBJ_NEW(orcm_value_t);
    mv->value.key = strdup("end_time");
    mv->value.type = OPAL_TIME;
    mv->value.data.tv.tv_sec = LONG_MAX;
    opal_list_append(input_list, (opal_list_item_t*)mv);

    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_diag_invalid_end_time_type)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_DOUBLE, (void*)(&d_value), OPAL_INT, (void*)(&component_index));
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}


TEST(ut_db_postgres_tests, store_diag_invalid_component_type)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    const char *current_time = "2016-07-06 12:00:00";
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_STRING, (void*)(&current_time), OPAL_DOUBLE, (void*)(&d_value));

    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_node_feature_missing_time_stamp)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)data_group, OPAL_TIME, NULL);
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_INVENTORY_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_node_feature_invalid_hostname_type)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_STRING, (void*)data_group, OPAL_TIMEVAL, (void*)(&start_time));
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_INVENTORY_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_node_feature_invalid_time_stamp_type)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)data_group, OPAL_DOUBLE, (void*)(&d_value));
    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_INVENTORY_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_node_feature_invalid_time_stamp_value)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)data_group, OPAL_TIMEVAL, NULL);
    orcm_value_t *mv = OBJ_NEW(orcm_value_t);
    mv->value.key = strdup("ctime");
    mv->value.type = OPAL_TIMEVAL;
    mv->value.data.tv.tv_sec = LONG_MAX;
    opal_list_append(input_list, (opal_list_item_t*)mv);

    rc = mca_db_postgres_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_INVENTORY_DATA, input_list, NULL);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_sample_invalid_time_stamp_value)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    gettimeofday(&start_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)data_group, OPAL_TIME,NULL);


    orcm_value_t *mv = OBJ_NEW(orcm_value_t);
    mv->value.key = strdup(analytics_util::cppstr_to_cstr("ctime"));
    mv->value.type = OPAL_TIME;
    mv->value.data.tv.tv_sec = LONG_MAX;
    opal_list_append(input_list, (opal_list_item_t*)mv);
    rc = mca_db_postgres_module.api.store((orcm_db_base_module_t*)mod,
                                           data_group, input_list);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_sample_invalid_time_stamp_type)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)host,
                              OPAL_STRING, (void*)data_group, OPAL_DOUBLE, (void*)(&d_value));
    rc = mca_db_postgres_module.api.store((orcm_db_base_module_t*)mod,
                                           data_group, input_list);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, store_sample_invalid_hostname_type)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_STRING, (void*)data_group, OPAL_TIMEVAL, (void*)(&start_time));
    rc = mca_db_postgres_module.api.store((orcm_db_base_module_t*)mod,
                                           data_group, input_list);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}


TEST(ut_db_postgres_tests, update_node_features_fail)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_STRING, (void*)data_group, OPAL_TIMEVAL, (void*)(&start_time));
    rc = mca_db_postgres_module.api.update_node_features((orcm_db_base_module_t*)mod,
                                           data_group, input_list);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERROR, rc);
}


TEST(ut_db_postgres_tests, commit_fail)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, "dummy_host");
    mca_db_postgres_module.api.init((orcm_db_base_module_t*)mod);
    rc = mca_db_postgres_module.api.commit((orcm_db_base_module_t*)mod);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(ut_db_postgres_tests, rollback_fail)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, "dummy_host");
    mca_db_postgres_module.api.init((orcm_db_base_module_t*)mod);
    rc = mca_db_postgres_module.api.rollback((orcm_db_base_module_t*)mod);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(ut_db_postgres_tests, close_result_set_fail)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    mca_db_postgres_module.api.init((orcm_db_base_module_t*)mod);
    rc = mca_db_postgres_module.api.close_result_set((orcm_db_base_module_t*)mod, 0);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(ut_db_postgres_tests, get_next_row_fail)
{
    int rc = ORCM_SUCCESS;
    opal_list_t *row = OBJ_NEW(opal_list_t);
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    mca_db_postgres_module.api.init((orcm_db_base_module_t*)mod);
    rc = mca_db_postgres_module.api.get_next_row((orcm_db_base_module_t*)mod, 0, row);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    ORCM_RELEASE(row);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(ut_db_postgres_tests, get_num_rows_fail)
{
    int rc = ORCM_SUCCESS;
    int *num_rows = NULL;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    mca_db_postgres_module.api.init((orcm_db_base_module_t*)mod);
    rc = mca_db_postgres_module.api.get_num_rows((orcm_db_base_module_t*)mod, 0, num_rows);
    mca_db_postgres_module.api.finalize((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST(ut_db_postgres_tests, check_for_invalid_characters)
{
    int rc = ORCM_SUCCESS;
    rc = check_for_invalid_characters("function_name");
    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST(ut_db_postgres_tests, check_for_invalid_characters_fail)
{
    int rc = ORCM_SUCCESS;
    rc = check_for_invalid_characters("function;name");
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
    rc = check_for_invalid_characters("function(name");
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
    rc = check_for_invalid_characters("function\'name");
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST(ut_db_postgres_tests, build_query_from_function_name_and_arguments_1)
{
    char* query = NULL;
    query = build_query_from_function_name_and_arguments("function_name",NULL);
    ASSERT_EQ(0, strcmp(query, "select * from function_name();"));
}

TEST(ut_db_postgres_tests, build_query_from_function_name_and_arguments_2)
{
    char* query = NULL;
    opal_list_t* argument_list = create_list_with_supported_types();
    query = build_query_from_function_name_and_arguments("function_name",argument_list);
    ASSERT_EQ(0, strcmp(query, "select * from function_name('value'::TIMESTAMP,value::DOUBLE,value::INT,'value'::INTERVAL,'value','value',);"));
}

TEST(ut_db_postgres_tests, build_query_from_function_name_and_arguments_3)
{
    char* query = NULL;
    opal_list_t* argument_list = create_list_with_null_values();
    query = build_query_from_function_name_and_arguments("function_name",argument_list);
    ASSERT_EQ(0, strcmp(query, "select * from function_name('INFINITY'::TIMESTAMP,NULL::DOUBLE,NULL::INT,NULL::INTERVAL,NULL,NULL,);"));
}

TEST(ut_db_postgres_tests, build_query_from_function_name_and_arguments_fail)
{
    char* rc = NULL;
    rc = build_query_from_function_name_and_arguments(NULL,NULL);
    ASSERT_EQ(NULL, rc);
}

TEST(ut_db_postgres_tests, build_query_from_function_name_and_arguments_fail_2)
{
    char* rc = NULL;
    rc = build_query_from_function_name_and_arguments("",NULL);
    ASSERT_EQ(NULL, rc);
}

TEST(ut_db_postgres_tests, fetch_function_null_function_name)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    rc = mca_db_postgres_module.api.fetch_function((orcm_db_base_module_t*)mod,NULL,NULL,NULL);
    EXPECT_EQ(ORCM_ERR_NOT_IMPLEMENTED,rc);
}

TEST(ut_db_postgres_tests, fetch_function_empty_function_name)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    rc = mca_db_postgres_module.api.fetch_function((orcm_db_base_module_t*)mod,"",NULL,NULL);
    EXPECT_EQ(ORCM_ERR_NOT_IMPLEMENTED,rc);
}

TEST(ut_db_postgres_tests, fetch_function_invalid_function_name)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    rc = mca_db_postgres_module.api.fetch_function((orcm_db_base_module_t*)mod,"function;name",NULL,NULL);
    EXPECT_EQ(ORCM_ERR_BAD_PARAM,rc);
}

TEST(ut_db_postgres_tests, fetch_function_null_kvs)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    rc = mca_db_postgres_module.api.fetch_function((orcm_db_base_module_t*)mod,"function_name",NULL,NULL);
    EXPECT_EQ(ORCM_ERROR,rc);
}

TEST(ut_db_postgres_tests, fetch_function_pqexec_fail)
{
    int rc = ORCM_SUCCESS;
    mca_db_postgres_module_t *mod = create_postgres_mod(db_name, user, host);
    opal_list_t* argument_list = create_list_with_supported_types();
    rc = mca_db_postgres_module.api.fetch_function((orcm_db_base_module_t*)mod,"function_name",argument_list,argument_list);
    EXPECT_EQ(ORCM_ERROR,rc);
}

TEST(ut_db_postgres_tests, escape_psql_string)
{
    int rc;
    const char *test_str = "''a'bc''d'";
    char *test_result = NULL;

    rc = postgres_escape_string((char *)test_str, &test_result);
    EXPECT_EQ(ORCM_SUCCESS,rc);
    ASSERT_STREQ(test_result,"''''a''bc''''d''");
    SAFEFREE(test_result);
}
