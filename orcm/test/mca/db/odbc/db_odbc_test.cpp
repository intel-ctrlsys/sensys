/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "db_odbc_test_mocking.h"
#include "db_odbc_test.h"
#include "orcm/test/mca/analytics/util/analytics_util.h"

const char *hostname = "RandomName";
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

int db_odbc::alloc_handle_counter;
int db_odbc::alloc_handle_counter_limit;
int db_odbc::set_env_attr_counter;
int db_odbc::set_env_attr_counter_limit;
int db_odbc::set_connect_attr_counter;
int db_odbc::set_connect_attr_counter_limit;
int db_odbc::connect_counter;
int db_odbc::connect_counter_limit;
int db_odbc::prepare_counter;
int db_odbc::prepare_counter_limit;
int db_odbc::bind_parameter_counter;
int db_odbc::bind_parameter_counter_limit;
int db_odbc::execute_counter;
int db_odbc::execute_counter_limit;
int db_odbc::end_tran_counter;
int db_odbc::end_tran_counter_limit;

void db_odbc::SetUpTestCase()
{
    return;
}

void db_odbc::TearDownTestCase()
{
    return;
}

void db_odbc::reset_var()
{
    alloc_handle_counter = alloc_handle_counter_limit = 0;
    set_env_attr_counter = set_env_attr_counter_limit = 0;
    set_connect_attr_counter = set_connect_attr_counter_limit = 0;
    connect_counter = connect_counter_limit = 0;
    prepare_counter = prepare_counter_limit = 0;
    bind_parameter_counter = bind_parameter_counter_limit = 0;
    execute_counter = execute_counter_limit = 0;
    end_tran_counter = end_tran_counter_limit = 0;
}

static SQLRETURN sql_call_ret(int *counter, int limit)
{
    SQLRETURN ret = 0;
    if (*counter != limit) {
        *counter = *counter + 1;
    } else {
        ret = -1;
    }

    return ret;
}

SQLRETURN db_odbc::sql_alloc_handle(SQLSMALLINT HandleType,
                                    SQLHANDLE InputHandle, SQLHANDLE *OutputHandle)
{
    return sql_call_ret(&alloc_handle_counter, alloc_handle_counter_limit);
}

SQLRETURN db_odbc::sql_bind_parameter(SQLHSTMT hstmt, SQLUSMALLINT ipar,
                                      SQLSMALLINT fParamType, SQLSMALLINT fCType,
                                      SQLSMALLINT fSqlType, SQLULEN cbColDef,
                                      SQLSMALLINT ibScale, SQLPOINTER rgbValue,
                                      SQLLEN cbValueMax, SQLLEN *pcbValue)
{
    return sql_call_ret(&bind_parameter_counter, bind_parameter_counter_limit);
}

SQLRETURN db_odbc::sql_connect(SQLHDBC ConnectionHandle, SQLCHAR *ServerName,
                               SQLSMALLINT NameLength1, SQLCHAR *UserName,
                               SQLSMALLINT NameLength2, SQLCHAR *Authentication,
                               SQLSMALLINT NameLength3)
{
    return sql_call_ret(&connect_counter, connect_counter_limit);
}

SQLRETURN db_odbc::sql_end_tran(SQLSMALLINT HandleType,
                                SQLHANDLE Handle,SQLSMALLINT CompletionType)
{
    return sql_call_ret(&end_tran_counter, end_tran_counter_limit);
}

SQLRETURN db_odbc::sql_execute(SQLHSTMT StatementHandle)
{
    return sql_call_ret(&execute_counter, execute_counter_limit);
}

SQLRETURN db_odbc::sql_prepare(SQLHSTMT StatementHandle,
                               SQLCHAR *StatementText, SQLINTEGER TextLength)
{
    return sql_call_ret(&prepare_counter, prepare_counter_limit);
}

SQLRETURN db_odbc::sql_set_connect_attr(SQLHDBC ConnectionHandle, SQLINTEGER Attribute,
                                        SQLPOINTER Value, SQLINTEGER StringLength)
{
    return sql_call_ret(&set_connect_attr_counter, set_connect_attr_counter_limit);
}

SQLRETURN db_odbc::sql_set_env_attr(SQLHENV EnvironmentHandle, SQLINTEGER Attribute,
                                    SQLPOINTER Value, SQLINTEGER StringLength)
{
    return sql_call_ret(&set_env_attr_counter, set_env_attr_counter_limit);
}

static void reset_mocking()
{
    db_odbc_mocking.sql_alloc_handle_callback = NULL;
    db_odbc_mocking.sql_bind_parameter_callback = NULL;
    db_odbc_mocking.sql_connect_callback = NULL;
    db_odbc_mocking.sql_end_tran_callback = NULL;
    db_odbc_mocking.sql_execute_callback = NULL;
    db_odbc_mocking.sql_prepare_callback = NULL;
    db_odbc_mocking.sql_set_connect_attr_callback = NULL;
    db_odbc_mocking.sql_set_env_attr_callback = NULL;
}

static mca_db_odbc_module_t* create_odbc_mod(const char* odbcdsn,
                                             const char *table, const char *user)
{
    mca_db_odbc_module_t *mod = (mca_db_odbc_module_t*)malloc(sizeof(mca_db_odbc_module_t));
    mod->autocommit = true;
    mod->odbcdsn = (NULL != odbcdsn) ? strdup(odbcdsn) : NULL;
    mod->table = (NULL != table) ? strdup(table) : NULL;
    mod->user = (NULL != user) ? strdup(user) : NULL;
    mod->results_sets = OBJ_NEW(opal_pointer_array_t);
    mod->dbhandle = NULL;
    mod->envhandle = NULL;
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

static opal_list_t*
create_list_with_mandatory_fields_event(opal_data_type_t ctime_type, void *ctime,
                                        opal_data_type_t severity_type, void *severity,
                                        opal_data_type_t type_type, void *type,
                                        opal_data_type_t version_type, void *version,
                                        opal_data_type_t vendor_type, void *vendor,
                                        opal_data_type_t description_type, void *description)
{
    opal_list_t *input_list = OBJ_NEW(opal_list_t);
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

    return input_list;
}

static void fill_data(opal_list_t *input_list, bool int_element, bool str_element)
{
    double d_value_coretemp = 37.5;
    int i_value_coretemp = 37;
    const char *str_value = "random string";
    orcm_value_t *mv = analytics_util::load_orcm_value("core0", (void*)(&d_value_coretemp),
                                                       OPAL_DOUBLE, "");
    if (NULL != mv) {
        opal_list_append(input_list, (opal_list_item_t*)mv);
    }

    if (int_element) {
        mv = analytics_util::load_orcm_value("core1", (void*)(&i_value_coretemp), OPAL_INT, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }

    if (str_element) {
        mv = analytics_util::load_orcm_value("core2", (void*)str_value, OPAL_STRING, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
        mv = analytics_util::load_orcm_value("core3", (void*)str_value, OPAL_STRING, "");
        if (NULL != mv) {
            opal_list_append(input_list, (opal_list_item_t*)mv);
        }
    }
}

static int sql_mock_testing(int alloc_handle_counter_limit, int prepare_counter_limit,
                            int bind_parameter_counter_limit, int execute_counter_limit,
                            int end_tran_limit, opal_list_t *input_list,
                            orcm_db_data_type_t data_type, mca_db_odbc_module_t *mod)
{
    int rc = ORCM_SUCCESS;
    db_odbc_mocking.sql_alloc_handle_callback = db_odbc::sql_alloc_handle;
    db_odbc_mocking.sql_prepare_callback = db_odbc::sql_prepare;
    db_odbc_mocking.sql_bind_parameter_callback = db_odbc::sql_bind_parameter;
    db_odbc_mocking.sql_execute_callback = db_odbc::sql_execute;
    db_odbc_mocking.sql_end_tran_callback = db_odbc::sql_end_tran;
    db_odbc::alloc_handle_counter_limit = alloc_handle_counter_limit;
    db_odbc::prepare_counter_limit = prepare_counter_limit;
    db_odbc::bind_parameter_counter_limit = bind_parameter_counter_limit;
    db_odbc::execute_counter_limit = execute_counter_limit;
    db_odbc::end_tran_counter_limit = end_tran_limit;
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod, data_type, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    reset_mocking();
    db_odbc::reset_var();
    return rc;
}

static int sql_failing_at_certain_time(int alloc_handle_counter_limit, int prepare_counter_limit,
                                       int bind_parameter_counter_limit,
                                       int execute_counter_limit, int end_tran_limit,
                                       bool autocommit, bool int_element, bool str_element,
                                       const char *data_group, orcm_db_data_type_t data_type)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    mod->autocommit = autocommit;
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)data_group, OPAL_TIMEVAL, (void*)(&start_time));
    fill_data(input_list, int_element, str_element);
    rc = sql_mock_testing(alloc_handle_counter_limit, prepare_counter_limit,
                          bind_parameter_counter_limit, execute_counter_limit,
                          end_tran_limit, input_list, data_type, mod);
    OBJ_RELEASE(input_list);
    return rc;
}

static int sql_failing_diag_at_certain_time(int alloc_handle_counter_limit,
                                            int prepare_counter_limit,
                                            int bind_parameter_counter_limit,
                                            int execute_counter_limit,
                                            int end_tran_counter_limit, bool autocommit,
                                            bool more_data, bool int_element, bool str_element)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    mod->autocommit = autocommit;
    gettimeofday(&start_time, NULL);
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    if (more_data) {
        fill_data(input_list, int_element, str_element);
    }
    rc = sql_mock_testing(alloc_handle_counter_limit, prepare_counter_limit,
                          bind_parameter_counter_limit, execute_counter_limit,
                          end_tran_counter_limit, input_list, ORCM_DB_DIAG_DATA, mod);
    OBJ_RELEASE(input_list);
    return rc;
}

static int sql_failing_store_event_at_certain_time(int alloc_handle_counter_limit,
                                                   int prepare_counter_limit,
                                                   int bind_parameter_counter_limit,
                                                   int execute_counter_limit,
                                                   int end_tran_counter_limit, bool autocommit,
                                                   bool int_element, bool str_element)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    mod->autocommit = autocommit;
    gettimeofday(&start_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_event(OPAL_TIMEVAL, (void*)(&start_time),
                              OPAL_STRING, (void*)severity, OPAL_STRING, (void*)type,
                              OPAL_STRING, (void*)version, OPAL_STRING, (void*)vendor,
                              OPAL_STRING, (void*)description);
    fill_data(input_list, int_element, str_element);
    rc = sql_mock_testing(alloc_handle_counter_limit, prepare_counter_limit,
                          bind_parameter_counter_limit, execute_counter_limit,
                          end_tran_counter_limit, input_list, ORCM_DB_EVENT_DATA, mod);
    OBJ_RELEASE(input_list);
    return rc;
}

TEST_F(db_odbc, init_fail_connection)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    rc = mca_db_odbc_module.api.init((orcm_db_base_module_t*)mod);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_ERR_CONNECTION_FAILED, rc);
}

TEST_F(db_odbc, init_first_alloc_handle_fail_mock)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    db_odbc_mocking.sql_alloc_handle_callback = sql_alloc_handle;
    rc = mca_db_odbc_module.api.init((orcm_db_base_module_t*)mod);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    reset_mocking();
    reset_var();
    ASSERT_EQ(ORCM_ERR_CONNECTION_FAILED, rc);
}

TEST_F(db_odbc, init_first_set_env_attr_fail_mock)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    db_odbc_mocking.sql_alloc_handle_callback = sql_alloc_handle;
    db_odbc_mocking.sql_set_env_attr_callback = sql_set_env_attr;
    alloc_handle_counter_limit = 1;
    rc = mca_db_odbc_module.api.init((orcm_db_base_module_t*)mod);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    reset_mocking();
    reset_var();
    ASSERT_EQ(ORCM_ERR_CONNECTION_FAILED, rc);
}

TEST_F(db_odbc, init_second_alloc_handle_fail_mock)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    db_odbc_mocking.sql_alloc_handle_callback = sql_alloc_handle;
    db_odbc_mocking.sql_set_env_attr_callback = sql_set_env_attr;
    alloc_handle_counter_limit = 1;
    set_env_attr_counter_limit = 1;
    rc = mca_db_odbc_module.api.init((orcm_db_base_module_t*)mod);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    reset_mocking();
    reset_var();
    ASSERT_EQ(ORCM_ERR_CONNECTION_FAILED, rc);
}

TEST_F(db_odbc, init_first_set_connect_attr_fail_mock)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    db_odbc_mocking.sql_alloc_handle_callback = sql_alloc_handle;
    db_odbc_mocking.sql_set_env_attr_callback = sql_set_env_attr;
    db_odbc_mocking.sql_set_connect_attr_callback = sql_set_connect_attr;
    alloc_handle_counter_limit = 2;
    set_env_attr_counter_limit = 1;
    rc = mca_db_odbc_module.api.init((orcm_db_base_module_t*)mod);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    reset_mocking();
    reset_var();
    ASSERT_EQ(ORCM_ERR_CONNECTION_FAILED, rc);
}

TEST_F(db_odbc, init_second_set_connect_attr_fail_mock)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    db_odbc_mocking.sql_alloc_handle_callback = sql_alloc_handle;
    db_odbc_mocking.sql_set_env_attr_callback = sql_set_env_attr;
    db_odbc_mocking.sql_set_connect_attr_callback = sql_set_connect_attr;
    db_odbc_mocking.sql_connect_callback = sql_connect;
    alloc_handle_counter_limit = 2;
    set_env_attr_counter_limit = 1;
    set_connect_attr_counter_limit = 1;
    connect_counter_limit = 1;
    rc = mca_db_odbc_module.api.init((orcm_db_base_module_t*)mod);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    reset_mocking();
    reset_var();
    ASSERT_EQ(ORCM_ERR_CONNECTION_FAILED, rc);
}

TEST_F(db_odbc, finalize_null_table)
{
    mca_db_odbc_module_t *mod = create_odbc_mod(NULL, NULL, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
}

TEST_F(db_odbc, store_unsupported_type)
{
    orcm_db_data_type_t unknown_type = (orcm_db_data_type_t)4;
    int rc = mca_db_odbc_module.api.store_new(NULL, unknown_type, NULL, NULL);
    ASSERT_EQ(ORCM_ERR_NOT_IMPLEMENTED, rc);
}

TEST_F(db_odbc, store_data_sample_invalid_data_group_type)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&start_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_DOUBLE, (void*)(&d_value), OPAL_TIMEVAL, (void*)(&start_time));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_data_sample_invalid_opal_time)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)data_group, OPAL_TIME, NULL);
    orcm_value_t *mv = OBJ_NEW(orcm_value_t);
    mv->value.key = strdup(analytics_util::cppstr_to_cstr("ctime"));
    mv->value.type = OPAL_TIME;
    mv->value.data.tv.tv_sec = LONG_MAX;
    opal_list_append(input_list, (opal_list_item_t*)mv);
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_data_sample_string_time_invalid_hostname_type)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    mod->autocommit = false;
    const char *ctime_local = "2015-12-31 12:00:00";
    opal_list_t *input_list =  create_list_with_mandatory_fields(OPAL_DOUBLE, (void*)(&d_value),
                               OPAL_STRING, (void*)data_group, OPAL_STRING, (void*)ctime_local);
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_data_sample_invalid_time_type)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)data_group, OPAL_DOUBLE, (void*)(&d_value));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_data_sample_handle_alloc_fail)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)data_group, OPAL_TIMEVAL, (void*)(&start_time));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_no_data_sample_mock)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)data_group, OPAL_TIMEVAL, (void*)(&start_time));
    db_odbc_mocking.sql_alloc_handle_callback = sql_alloc_handle;
    alloc_handle_counter_limit = 1;
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    reset_mocking();
    reset_var();
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_data_sample_prepare_fail_mock)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)data_group, OPAL_TIMEVAL, (void*)(&start_time));
    double d_value_coretemp = 37.5;
    orcm_value_t *mv = analytics_util::load_orcm_value("core0", (void*)(&d_value_coretemp),
                                                       OPAL_DOUBLE, "C");
    if (NULL != mv) {
        opal_list_append(input_list, (opal_list_item_t*)mv);
    }
    db_odbc_mocking.sql_alloc_handle_callback = sql_alloc_handle;
    db_odbc_mocking.sql_prepare_callback = sql_prepare;
    alloc_handle_counter_limit = 1;
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_ENV_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    reset_mocking();
    reset_var();
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 0, 1, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 1, 1, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 2, 1, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_fourth_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 3, 1, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_fifth_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 4, 1, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_sixth_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 5, 1, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_seventh_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 6, 1, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_eighth_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 7, 1, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_ninth_bind_parameter_fail_null_unit_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 8, 1, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_real_type_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 9, 1, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_real_type_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 10, 1, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_real_type_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 11, 1, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_real_type_execute_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 12, 0, 1, true, false, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_int_type_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 14, 1, 1, true, true, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_int_type_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 15, 1, 1, true, true, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_int_type_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 16, 1, 1, true, true, false,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_string_type_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 19, 2, 1, true, true, true,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_string_type_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 20, 2, 1, true, true, true,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_string_type_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 21, 2, 1, true, true, true,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_second_string_type_bind_parameter_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 24, 3, 1, true, true, true,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_autocommit_true_endtran_fail_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 25, 4, 0, true, true, true,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_data_sample_autocommit_false_mock)
{
    int rc = sql_failing_at_certain_time(1, 1, 25, 4, 1, false, true, true,
                                         data_group, ORCM_DB_ENV_DATA);
    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(db_odbc, store_node_feature_no_time_stamp)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    mod->autocommit = false;
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)data_group, OPAL_TIMEVAL, NULL);
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_INVENTORY_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_node_feature_invalid_hostname_type)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_STRING, NULL, OPAL_TIMEVAL, (void*)(&start_time));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_INVENTORY_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_node_feature_invalid_ctime_type)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, NULL, OPAL_DOUBLE, (void*)(&d_value));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_INVENTORY_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_node_feature_invalid_ctime)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    start_time.tv_sec = LONG_MAX;
    start_time.tv_usec = 0;
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, NULL, OPAL_TIMEVAL, (void*)(&start_time));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_INVENTORY_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_node_feature_no_feature)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, NULL, OPAL_TIMEVAL, (void*)(&start_time));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_INVENTORY_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(db_odbc, store_node_feature_handle_alloc_fail)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)data_group, OPAL_TIMEVAL, (void*)(&start_time));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_INVENTORY_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_prepare_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 0, 0, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_first_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 0, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_second_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 1, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_third_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 2, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_fourth_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 3, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_fifth_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 4, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_sixth_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 5, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_zero_length_key_mock)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    opal_list_t *input_list = create_list_with_mandatory_fields(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)NULL, OPAL_TIMEVAL, (void*)(&start_time));
    double d_value_coretemp = 37.5;
    orcm_value_t *mv = analytics_util::load_orcm_value("\0", (void*)(&d_value_coretemp),
                                                       OPAL_DOUBLE, "");
    if (NULL != mv) {
        opal_list_append(input_list, (opal_list_item_t*)mv);
    }
    rc = sql_mock_testing(1, 1, 6, 0, 0, input_list, ORCM_DB_INVENTORY_DATA, mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_node_feature_seventh_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 6, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_real_first_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 7, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_real_second_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 8, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_real_third_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 9, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_real_fourth_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 10, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_real_execute_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 11, 0, 0, true, false, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_int_first_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 12, 1, 0, true, true, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_int_second_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 13, 1, 0, true, true, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_int_third_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 14, 1, 0, true, true, false,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_string_first_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 17, 2, 0, true, true, true,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_string_second_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 18, 2, 0, true, true, true,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_string_third_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 19, 2, 0, true, true, true,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_second_string_bind_parameter_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 22, 3, 0, true, true, true,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_end_trans_fail_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 24, 4, 0, true, true, true,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_node_feature_autocommit_false_mock)
{
    int rc =  sql_failing_at_certain_time(1, 1, 24, 4, 0, false, true, true,
                                          NULL, ORCM_DB_INVENTORY_DATA);
    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(db_odbc, store_diag_invalid_hostname_type)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    mod->autocommit = false;
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_diag_invalid_type_type)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)hostname,
                              OPAL_DOUBLE, (void*)(&d_value), OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_diag_invalid_subtype_type)
{
    int rc = ORCM_SUCCESS;
    gettimeofday(&start_time, NULL);
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)type, OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_diag_invalid_opal_time_start)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, NULL, OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    orcm_value_t *mv = OBJ_NEW(orcm_value_t);
    mv->value.key = strdup(analytics_util::cppstr_to_cstr("start_time"));
    mv->value.type = OPAL_TIME;
    mv->value.data.tv.tv_sec = LONG_MAX;
    opal_list_append(input_list, (opal_list_item_t*)mv);
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_diag_string_start_time_invalid_test_result_type)
{
    int rc = ORCM_SUCCESS;
    const char *start_time_local = "2015-12-31 12:00:00";
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_STRING, (void*)start_time_local, OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_diag_invalid_start_time_type)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_DOUBLE, (void*)(&d_value), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_diag_invalid_opal_time_end)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&start_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, NULL, OPAL_INT, (void*)(&component_index));
    orcm_value_t *mv = OBJ_NEW(orcm_value_t);
    mv->value.key = strdup(analytics_util::cppstr_to_cstr("end_time"));
    mv->value.type = OPAL_TIME;
    mv->value.data.tv.tv_sec = LONG_MAX;
    opal_list_append(input_list, (opal_list_item_t*)mv);
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_diag_string_end_time_invalid_component_index_type)
{
    int rc = ORCM_SUCCESS;
    const char *end_time_local = "2015-12-31 12:00:00";
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&start_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_STRING, (void*)(end_time_local), OPAL_DOUBLE, (void*)(&d_value));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_diag_invalid_end_time_type)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&start_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_DOUBLE, (void*)(&d_value), OPAL_INT, (void*)(&component_index));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_DIAG_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}



TEST_F(db_odbc, store_diag_handle_alloc_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(0, 0, 0, 0, 0, true, false, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_prepare_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(1, 0, 0, 0, 0, true, false, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(1, 1, 0, 0, 0, true, false, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(1, 1, 1, 0, 0, true, false, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(1, 1, 2, 0, 0, true, false, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_fourth_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(1, 1, 3, 0, 0, true, false, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_fifth_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(1, 1, 4, 0, 0, true, false, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_sixth_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(1, 1, 5, 0, 0, true, false, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_seventh_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(1, 1, 6, 0, 0, true, false, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_execute_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(1, 1, 7, 0, 0, true, false, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_autocommit_false_without_data_mock)
{
    int rc = sql_failing_diag_at_certain_time(1, 1, 7, 1, 0, false, false, false, false);
    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(db_odbc, store_diag_autocommit_true_without_data_mock)
{
    int rc = sql_failing_diag_at_certain_time(1, 1, 7, 1, 0, true, false, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_alloc_handle_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(1, 1, 7, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_prepare_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 1, 7, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 7, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 8, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 9, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_fourth_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 10, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_fifth_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 11, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_sixth_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 12, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_seventh_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 13, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_eighth_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 14, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_null_key_mock)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&start_time, NULL);
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    orcm_value_t *mv = analytics_util::load_orcm_value("", (void*)(&d_value), OPAL_DOUBLE, "");
    if (NULL != mv) {
        opal_list_append(input_list, (opal_list_item_t*)mv);
    }
    rc = sql_mock_testing(2, 2, 15, 1, 0, input_list, ORCM_DB_DIAG_DATA, mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_diag_with_data_zero_size_key_mock)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&start_time, NULL);
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    orcm_value_t *mv = analytics_util::load_orcm_value("\0", (void*)(&d_value), OPAL_DOUBLE, "");
    if (NULL != mv) {
        opal_list_append(input_list, (opal_list_item_t*)mv);
    }
    rc = sql_mock_testing(2, 2, 15, 1, 0, input_list, ORCM_DB_DIAG_DATA, mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_diag_with_data_unsupported_type_mock)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&start_time, NULL);
    gettimeofday(&end_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_diag(OPAL_STRING, (void*)hostname,
                              OPAL_STRING, (void*)type, OPAL_STRING, (void*)subtype,
                              OPAL_TIMEVAL, (void*)(&start_time), OPAL_STRING, (void*)test_result,
                              OPAL_TIMEVAL, (void*)(&end_time), OPAL_INT, (void*)(&component_index));
    orcm_value_t *mv = analytics_util::load_orcm_value("k", (void*)(&start_time), OPAL_TIMEVAL, "");
    if (NULL != mv) {
        opal_list_append(input_list, (opal_list_item_t*)mv);
    }
    rc = sql_mock_testing(2, 2, 15, 1, 0, input_list, ORCM_DB_DIAG_DATA, mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_NOT_SUPPORTED, rc);
}

TEST_F(db_odbc, store_diag_with_data_nineth_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 15, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_tenth_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 16, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_real_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 17, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_real_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 18, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_real_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 19, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_real_execute_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 20, 1, 0, true, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_int_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 22, 2, 0, true, true, true, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_int_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 23, 2, 0, true, true, true, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_int_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 24, 2, 0, true, true, true, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_string_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 27, 3, 0, true, true, true, true);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_string_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 28, 3, 0, true, true, true, true);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_string_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 29, 3, 0, true, true, true, true);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_second_string_bind_parameter_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 32, 4, 0, true, true, true, true);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_end_trans_fail_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 33, 5, 0, true, true, true, true);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_diag_with_data_autocommit_false_mock)
{
    int rc = sql_failing_diag_at_certain_time(2, 2, 33, 5, 1, false, true, true, true);
    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(db_odbc, store_event_invalid_timeval)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    mod->autocommit = false;
    start_time.tv_sec = LONG_MAX;
    start_time.tv_usec = 0;
    opal_list_t *input_list = create_list_with_mandatory_fields_event(OPAL_TIMEVAL, (void*)(&start_time),
                              OPAL_STRING, (void*)severity, OPAL_STRING, (void*)type,
                              OPAL_STRING, (void*)version, OPAL_STRING, (void*)vendor,
                              OPAL_STRING, (void*)description);
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_EVENT_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_event_multiple_branch)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&start_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_event(OPAL_TIMEVAL, (void*)(&start_time),
                              OPAL_STRING, (void*)severity, OPAL_STRING, (void*)type,
                              OPAL_DOUBLE, (void*)(&d_value), OPAL_DOUBLE, (void*)(&d_value),
                              OPAL_DOUBLE, (void*)(&d_value));
    rc = mca_db_odbc_module.api.store_new((orcm_db_base_module_t*)mod,
                                           ORCM_DB_EVENT_DATA, input_list, NULL);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_prepare_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(1, 0, 0, 0, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(1, 1, 0, 0, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(1, 1, 1, 0, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(1, 1, 2, 0, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_fourth_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(1, 1, 3, 0, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_fifth_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(1, 1, 4, 0, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_sixth_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(1, 1, 5, 0, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_seventh_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(1, 1, 6, 0, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_second_alloc_handle_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(1, 1, 7, 1, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_second_prepare_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 1, 7, 1, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_eigth_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 7, 1, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_zero_length_key_mock)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&start_time, NULL);
    opal_list_t *input_list = create_list_with_mandatory_fields_event(OPAL_TIMEVAL, (void*)(&start_time),
                              OPAL_STRING, (void*)severity, OPAL_STRING, (void*)type,
                              OPAL_STRING, (void*)version, OPAL_STRING, (void*)vendor,
                              OPAL_STRING, (void*)description);
    orcm_value_t *mv = analytics_util::load_orcm_value("\0", (void*)(&d_value), OPAL_DOUBLE, "");
    if (NULL != mv) {
        opal_list_append(input_list, (opal_list_item_t*)mv);
    }
    rc = sql_mock_testing(2, 2, 8, 1, 0, input_list, ORCM_DB_EVENT_DATA, mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, store_event_nineth_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 8, 1, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_tenth_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 9, 1, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_real_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 10, 1, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_real_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 11, 1, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_real_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 12, 1, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_real_fourth_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 13, 1, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_real_execute_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 14, 1, 0, true, false, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_int_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 16, 2, 0, true, true, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_int_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 17, 2, 0, true, true, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_int_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 18, 2, 0, true, true, false);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_string_first_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 22, 3, 0, true, true, true);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_string_second_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 23, 3, 0, true, true, true);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_string_third_bind_parameter_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 24, 3, 0, true, true, true);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_end_trans_fail_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 33, 5, 0, true, true, true);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, store_event_autocommit_false_mock)
{
    int rc = sql_failing_store_event_at_certain_time(2, 2, 33, 5, 0, false, true, true);
    ASSERT_EQ(ORCM_SUCCESS, rc);
}

TEST_F(db_odbc, record_data_samples_invalid_time)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    start_time.tv_sec = LONG_MAX;
    start_time.tv_usec = 0;
    opal_list_t *input_list = OBJ_NEW(opal_list_t);
    rc = mca_db_odbc_module.api.record_data_samples((orcm_db_base_module_t*)mod, hostname,
                                                    &start_time, data_group, input_list);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERR_BAD_PARAM, rc);
}

TEST_F(db_odbc, record_data_samples_handle_alloc_fail)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    gettimeofday(&start_time, NULL);
    opal_list_t *input_list = OBJ_NEW(opal_list_t);
    rc = mca_db_odbc_module.api.record_data_samples((orcm_db_base_module_t*)mod, hostname,
                                                    &start_time, data_group, input_list);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, update_node_features_handle_alloc_fail)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    opal_list_t *input_list = OBJ_NEW(opal_list_t);
    rc = mca_db_odbc_module.api.update_node_features((orcm_db_base_module_t*)mod,
                                                     hostname, input_list);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, record_diag_handle_alloc_fail)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    opal_list_t *input_list = OBJ_NEW(opal_list_t);
    gettimeofday(&start_time, NULL);
    gettimeofday(&end_time, NULL);
    rc = mca_db_odbc_module.api.record_diag_test((orcm_db_base_module_t*)mod, hostname, type,
                                                 subtype, localtime(&(start_time.tv_sec)),
                                                 localtime(&(end_time.tv_sec)), &component_index,
                                                 test_result, input_list);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    OBJ_RELEASE(input_list);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, commit_fail)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    mca_db_odbc_module.api.init((orcm_db_base_module_t*)mod);
    rc = mca_db_odbc_module.api.commit((orcm_db_base_module_t*)mod);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, rollback_fail)
{
    int rc = ORCM_SUCCESS;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    mca_db_odbc_module.api.init((orcm_db_base_module_t*)mod);
    rc = mca_db_odbc_module.api.rollback((orcm_db_base_module_t*)mod);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, get_num_row_empty_results)
{
    int rc = ORCM_SUCCESS;
    int num_rows = 0;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    mca_db_odbc_module.api.init((orcm_db_base_module_t*)mod);
    rc = mca_db_odbc_module.api.get_num_rows((orcm_db_base_module_t*)mod, 0, &num_rows);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_ERROR, rc);
}

TEST_F(db_odbc, get_num_row_invalid_results)
{
    int rc = ORCM_SUCCESS;
    int num_rows = 0;
    mca_db_odbc_module_t *mod = create_odbc_mod("odbc", "random_table", "randomuser:randompw");
    mca_db_odbc_module.api.init((orcm_db_base_module_t*)mod);
    const char *result = "result";
    opal_pointer_array_add(mod->results_sets, (void*)result);
    rc = mca_db_odbc_module.api.get_num_rows((orcm_db_base_module_t*)mod, 0, &num_rows);
    mca_db_odbc_module.api.finalize((orcm_db_base_module_t*)mod);
    ASSERT_EQ(ORCM_ERROR, rc);
}
