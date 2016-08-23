/*
 * Copyright (c) 2015-2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#define GTEST_MOCK_TESTING
#include "orcm/mca/sensor/snmp/sensor_snmp.h"
#include "orcm/mca/sensor/snmp/snmp.h"
#include "orcm/mca/sensor/snmp/snmp_collector.h"
#include "orcm/mca/sensor/snmp/snmp_parser.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#undef GTEST_MOCK_TESTING

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/transform_oids.h>

#include "snmp_collector_tests.h"

#include "gtest/gtest.h"
#include <memory.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include "opal/dss/dss.h"
#include "orte/util/proc_info.h"
#include "orcm/mca/sensor/base/sensor_private.h"

extern "C" {
    #include "opal/runtime/opal_progress_threads.h"
    #include "opal/runtime/opal.h"
    #include "orcm/mca/parser/base/base.h"
    #include "orcm/mca/parser/pugi/parser_pugi.h"

    extern void collect_snmp_sample(orcm_sensor_sampler_t *sampler);
    extern orcm_sensor_snmp_component_t mca_sensor_snmp_component;
}

#define NOOP_JOBID -999
#define SNMP_ERR_NOERROR 0
#define MY_MODULE_PRIORITY 20

#define SAFE_OBJ_RELEASE(x) if(NULL!=x) OBJ_RELEASE(x); x=NULL
#define ASSERT_PTREQ(x,y)  ASSERT_EQ((void*)x,(void*)y)
#define ASSERT_PTRNE(x,y)  ASSERT_NE((void*)x,(void*)y)
#define ASSERT_NULL(x)     ASSERT_PTREQ(NULL,x)
#define ASSERT_NOT_NULL(x) ASSERT_PTRNE(NULL,x)
#define EXPECT_PTREQ(x,y)  EXPECT_EQ((void*)x,(void*)y)
#define EXPECT_PTRNE(x,y)  EXPECT_NE((void*)x,(void*)y)
#define EXPECT_NULL(x)     EXPECT_PTREQ(NULL,x)
#define EXPECT_NOT_NULL(x) EXPECT_PTRNE(NULL,x)

#define SNMP_ERR_BADVALUE               (3)
#define HOSTNAME_STR "hostname"
#define CTIME_STR "ctime"
#define TOT_HOSTNAMES_STR "tot_hostnames"
#define TOT_ITEMS_STR "tot_items"
#define OIDS_LIST ".1.3.6.1.2.1.1.7.0,.1.3.6.1.2.1.1.7.9"
#define OIDS_LIST_SIZE 2
#define OID_0 ".1.3.6.1.2.1.1.7.0"
#define OID_1 ".1.3.6.1.2.1.1.7.9"

/* Fixture */
using namespace std;

char dataStr[] = "jelou";
int long dataInt = 9;
uint32_t dataUint = 9;
int long dataIp = 570534080;
char dataIpAddr[] = "192.168.1.34";
float dataFloat = 9.9;
double dataDouble = 999;

const char* ut_snmp_collector_tests::hostname_ = "test_host";
const char* ut_snmp_collector_tests::plugin_name_ = "snmp";
const char* ut_snmp_collector_tests::proc_name_ = "snmp_tests";

snmpCollector* ut_snmp_collector_tests::collector = NULL;
int ut_snmp_collector_tests::last_errno_ = 0;
string ut_snmp_collector_tests::last_error_filename_;
void* ut_snmp_collector_tests::last_user_data_ = NULL;
int ut_snmp_collector_tests::last_orte_error_ = ORCM_SUCCESS;
int ut_snmp_collector_tests::fail_pack_on_ = -1;
int ut_snmp_collector_tests::fail_pack_count_ = 0;
int ut_snmp_collector_tests::fail_unpack_on_ = -1;
int ut_snmp_collector_tests::fail_unpack_count_ = 0;
bool ut_snmp_collector_tests::fail_pack_buffer_ = false;
bool ut_snmp_collector_tests::fail_unpack_buffer_ = false;
map<string,int> ut_snmp_collector_tests::logged_data_;
map<string,int> ut_snmp_collector_tests::golden_data_;
vector<string> ut_snmp_collector_tests::opal_output_verbose_;
queue<int32_t> ut_snmp_collector_tests::packed_int32_;
queue<string> ut_snmp_collector_tests::packed_str_;
queue<struct timeval> ut_snmp_collector_tests::packed_ts_;
std::vector<orcm_analytics_value_t*> ut_snmp_collector_tests::current_analytics_values_;
map<string,string> ut_snmp_collector_tests::database_data_;
char* ut_snmp_collector_tests::config_file = NULL;
char* ut_snmp_collector_tests::tmp_config_file = NULL;

extern "C" {
    extern orcm_sensor_snmp_component_t mca_sensor_snmp_component;
    extern orte_proc_info_t orte_process_info;

    extern int orcm_sensor_snmp_open(void);
    extern int orcm_sensor_snmp_close(void);
    extern int orcm_sensor_snmp_query(mca_base_module_t **module, int *priority);
    extern int snmp_component_register(void);
}

void ut_snmp_collector_tests::SetUp()
{
    mca_sensor_snmp_component.config_file = tmp_config_file;
    ResetTestEnvironment();
}

void ut_snmp_collector_tests::SetUpTestCase()
{
    opal_dss_register_vars();

    opal_init_test();

    initParserFramework();

    config_file = mca_sensor_snmp_component.config_file;
    tmp_config_file = strdup(SNMP_DEFAULT_FILE_NAME);
    testFilesObj.writeDefaultSnmpConfigFile();
}

void ut_snmp_collector_tests::TearDownTestCase()
{
    golden_data_.clear();

    cleanParserFramework();

    testFilesObj.removeDefaultSnmpConfigFile();
    mca_sensor_snmp_component.config_file = config_file;
    config_file = NULL;
    SAFEFREE(tmp_config_file);
}

void ut_snmp_collector_tests::TearDown()
{
    snmp_mocking.orte_errmgr_base_log_callback = NULL;
    snmp_mocking.opal_output_verbose_callback = NULL;
    snmp_mocking.orte_util_print_name_args_callback = NULL;
    snmp_mocking.orcm_analytics_base_send_data_callback = NULL;
    snmp_mocking.opal_progress_thread_init_callback = NULL;
    snmp_mocking.opal_progress_thread_finalize_callback = NULL;
    snmp_mocking.snmp_open_callback = NULL;
    snmp_mocking.snmp_synch_response_callback = NULL;
    snmp_mocking.snmp_free_pdu_callback = NULL;
    snmp_mocking.snmp_pdu_create_callback = NULL;
    snmp_mocking.snmp_parse_oid_callback = NULL;
    snmp_mocking.snmp_add_null_var_callback = NULL;
    snmp_mocking.snprint_objid_callback = NULL;
    snmp_mocking.orcm_util_load_orcm_analytics_value_callback = NULL;
}

void ut_snmp_collector_tests::ClearBuffers()
{
    while(0 < packed_int32_.size()) {
        packed_int32_.pop();
    }
    while(0 < packed_ts_.size()) {
        packed_ts_.pop();
    }
    while(0 < packed_str_.size()) {
        packed_str_.pop();
    }
}

void ut_snmp_collector_tests::ResetTestEnvironment()
{
    logged_data_.clear();
    opal_output_verbose_.clear();
    database_data_.clear();
    ClearBuffers();

    snmp_mocking.orte_errmgr_base_log_callback = OrteErrmgrBaseLog;
    snmp_mocking.opal_output_verbose_callback = OpalOutputVerbose;
    snmp_mocking.orte_util_print_name_args_callback = OrteUtilPrintNameArgs;
    snmp_mocking.orcm_analytics_base_send_data_callback = OrcmAnalyticsBaseSendData;
    snmp_mocking.opal_progress_thread_init_callback = NULL;
    snmp_mocking.opal_progress_thread_finalize_callback = NULL;
    snmp_mocking.snmp_open_callback = SnmpOpen;
    snmp_mocking.snmp_synch_response_callback = SnmpSynchResponse;
    snmp_mocking.snmp_free_pdu_callback = SnmpFreePdu;
    snmp_mocking.snmp_pdu_create_callback = SnmpPDUCreate;
    snmp_mocking.snmp_parse_oid_callback = ReadObjid;
    snmp_mocking.snmp_add_null_var_callback = SnmpAddNullVar;
    snmp_mocking.snprint_objid_callback = PrintObjid;
    snmp_mocking.orcm_util_load_orcm_analytics_value_callback = NULL;

    orte_process_info.nodename = (char*)hostname_;

    snmp_finalize_relay();

    last_orte_error_ = ORCM_SUCCESS;
    fail_pack_on_ = -1;
    fail_pack_count_ = 0;
    fail_unpack_on_ = -1;
    fail_unpack_count_ = 0;
    fail_pack_buffer_ = false;
    fail_unpack_buffer_ = false;

    mca_sensor_snmp_component.use_progress_thread = false;
    mca_sensor_snmp_component.sample_rate = 0;

    for(int i = 0; i < current_analytics_values_.size(); ++i) {
        SAFE_OBJ_RELEASE(current_analytics_values_[i]);
    }
    current_analytics_values_.clear();

    orcm_sensor_base.collect_metrics = true;
    orcm_sensor_base.collect_inventory = true;
    mca_sensor_snmp_component.collect_metrics = true;
    mca_sensor_snmp_component.test = false;
    mca_sensor_snmp_component.use_progress_thread = false;
}

void ut_snmp_collector_tests::initParserFramework()
{
    OBJ_CONSTRUCT(&orcm_parser_base_framework.framework_components, opal_list_t);
    OBJ_CONSTRUCT(&orcm_parser_base.actives, opal_list_t);
    orcm_parser_active_module_t *act_module;
    act_module = OBJ_NEW(orcm_parser_active_module_t);
    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_pugi_module;
    opal_list_append(&orcm_parser_base.actives, &act_module->super);
}

void ut_snmp_collector_tests::cleanParserFramework()
{
    OPAL_LIST_DESTRUCT(&orcm_parser_base_framework.framework_components);
    OPAL_LIST_DESTRUCT(&orcm_parser_base.actives);
}


/* Mocking methods */
struct snmp_session* ut_snmp_collector_tests::SnmpOpen(struct snmp_session* ss)
{
  return new snmp_session;
}

struct snmp_session* ut_snmp_collector_tests::SnmpOpenReturnNull(struct snmp_session* ss)
{
  return NULL;
}

void ut_snmp_collector_tests::OrteErrmgrBaseLog(int err, char* file, int lineno)
{
    (void)file;
    (void)lineno;
    last_orte_error_ = err;
}

void ut_snmp_collector_tests::OpalOutputVerbose(int level, int output_id, const char* line)
{
    opal_output_verbose_.push_back(string(line));
}

char* ut_snmp_collector_tests::OrteUtilPrintNameArgs(const orte_process_name_t *name)
{
    (void)name;
    return (char*)proc_name_;
}

void ut_snmp_collector_tests::OrcmAnalyticsBaseSendData(orcm_analytics_value_t* data)
{
    if(NULL != data) {
        OBJ_RETAIN(data);
        current_analytics_values_.push_back(data);
    }
}

opal_event_base_t* ut_snmp_collector_tests::OpalProgressThreadInit(const char* name)
{
    return NULL;
}

int ut_snmp_collector_tests::OpalProgressThreadFinalize(const char* name)
{
    return ORCM_SUCCESS;
}

struct snmp_pdu *ut_snmp_collector_tests::SnmpPDUCreate(int command)
{
    return new snmp_pdu;
}

oid* ut_snmp_collector_tests::ReadObjid(const char *input, oid *objid, size_t *objidlen)
{
    return (oid*) malloc(sizeof(oid));
}

int ut_snmp_collector_tests::PrintObjid(char *buf, size_t len,
                                        oid *objid, size_t *objidlen)
{
    return 1;
}

netsnmp_variable_list *ut_snmp_collector_tests::SnmpAddNullVar(netsnmp_pdu *pdu,
                                                               const oid *objid,
                                                               size_t objidlen)
{
    return NULL;
}

int ut_snmp_collector_tests::SnmpSynchResponse(netsnmp_session *session,
                                               netsnmp_pdu *pdu,
                                               netsnmp_pdu **response)
{
    variable_list *t_str = new variable_list;
    variable_list *t_int = new variable_list;
    variable_list *t_float = new variable_list;
    variable_list *t_double = new variable_list;
    variable_list *t_ipaddr = new variable_list;
    variable_list *t_counter = new variable_list;
    variable_list *t_gauge = new variable_list;
    variable_list *t_timeticks = new variable_list;

    size_t dataLen = strlen(dataStr);

    *response = new netsnmp_pdu;
    (*response)->errstat = SNMP_ERR_NOERROR;

    t_str->name = (oid*)strdup("myStr");
    t_str->name_length = strlen((char*)t_str->name);
    t_str->type = ASN_OCTET_STR;
    t_str->val.string = new u_char[dataLen+1];
    memcpy(t_str->val.string, dataStr, dataLen);
    t_str->val_len = dataLen;
    t_str->next_variable = t_int;

    t_int->name = (oid*)strdup("myInt");
    t_int->name_length = strlen((char*)t_int->name);
    t_int->type = ASN_INTEGER;
    t_int->val.integer = new long;
    *t_int->val.integer = dataInt;
    t_int->next_variable = t_float;

    t_float->name = (oid*)strdup("myFloat");
    t_float->name_length = strlen((char*)t_float->name);
    t_float->type = ASN_OPAQUE_FLOAT;
    t_float->val.floatVal = new float;
    *t_float->val.floatVal = dataFloat;
    t_float->next_variable = t_double;

    t_double->name = (oid*)strdup("myDouble");
    t_double->name_length = strlen((char*)t_double->name);
    t_double->type = ASN_OPAQUE_DOUBLE;
    t_double->val.doubleVal = new double;
    *t_double->val.doubleVal = dataDouble;
    t_double->next_variable = t_ipaddr;

    t_ipaddr->name = (oid*)strdup("myIpAddress");
    t_ipaddr->name_length = strlen((char*)t_ipaddr->name);
    t_ipaddr->type = ASN_IPADDRESS;
    t_ipaddr->val.integer = new long;
    *t_ipaddr->val.integer = dataIp;
    t_ipaddr->next_variable = t_counter;

    t_counter->name = (oid*)strdup("myCounter");
    t_counter->name_length = strlen((char*)t_counter->name);
    t_counter->type = ASN_COUNTER;
    t_counter->val.integer = (long*) &dataUint;
    t_counter->next_variable = t_gauge;

    t_gauge->name = (oid*)strdup("myGauge");
    t_gauge->name_length = strlen((char*)t_gauge->name);
    t_gauge->type = ASN_INTEGER;
    t_gauge->val.integer = (long*) &dataUint;
    t_gauge->next_variable = t_timeticks;

    t_timeticks->name = (oid*)strdup("myTimeticks");
    t_timeticks->name_length = strlen((char*)t_timeticks->name);
    t_timeticks->type = ASN_INTEGER;
    t_timeticks->val.integer = (long*) &dataUint;
    t_timeticks->next_variable = NULL;

    (*response)->variables = t_str;

    return STAT_SUCCESS;
}


int ut_snmp_collector_tests::SnmpSynchResponse_timeout(netsnmp_session *session,
                                               netsnmp_pdu *pdu,
                                               netsnmp_pdu **response)
{
    return STAT_TIMEOUT;
}


int ut_snmp_collector_tests::SnmpSynchResponse_error(netsnmp_session *session,
                                               netsnmp_pdu *pdu,
                                               netsnmp_pdu **response)
{
    return STAT_ERROR;
}

int ut_snmp_collector_tests::SnmpSynchResponse_packeterror(netsnmp_session *session,
                                               netsnmp_pdu *pdu,
                                               netsnmp_pdu **response)
{
    SnmpSynchResponse(session, pdu, response);
    if (response != NULL) {
        (*response)->errstat = SNMP_ERR_BADVALUE;
    }
    return STAT_SUCCESS;
}

void ut_snmp_collector_tests::SnmpFreePdu(netsnmp_pdu *pdu)
{
  netsnmp_variable_list *vars = pdu->variables;

  delete pdu;

  return;
}

orcm_analytics_value_t* ut_snmp_collector_tests::OrcmUtilLoadAnalyticsValue(
                            opal_list_t *key, opal_list_t *nc, opal_list_t *c)
{
    return NULL;
}

TEST_F(ut_snmp_collector_tests, test_init_finalize_relays)
{
    ASSERT_EQ(ORCM_SUCCESS, snmp_init_relay());

    ASSERT_EQ(ORCM_ERROR, snmp_init_relay());

    snmp_finalize_relay();
}

TEST_F(ut_snmp_collector_tests, test_start_stop_relays)
{
    ASSERT_EQ(ORCM_SUCCESS, snmp_init_relay());

    snmp_start_relay((orte_jobid_t)NOOP_JOBID);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    snmp_stop_relay((orte_jobid_t)NOOP_JOBID);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    snmp_finalize_relay();

    snmp_start_relay((orte_jobid_t)0);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);

    snmp_stop_relay((orte_jobid_t)0);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_snmp_collector_tests, test_sample_relay)
{
    ASSERT_EQ(ORCM_SUCCESS, snmp_init_relay());

    snmp_sample_relay(NULL);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    snmp_finalize_relay();

    snmp_sample_relay(NULL);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_snmp_collector_tests, test_log_relay)
{
    ASSERT_EQ(ORCM_SUCCESS, snmp_init_relay());

    snmp_log_relay(NULL);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    snmp_finalize_relay();

    snmp_log_relay(NULL);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_snmp_collector_tests, test_set_sample_rate_relay)
{
    ASSERT_EQ(ORCM_SUCCESS, snmp_init_relay());

    snmp_set_sample_rate_relay(1);
    ASSERT_EQ(ORCM_SUCCESS, last_orte_error_);

    snmp_finalize_relay();

    snmp_set_sample_rate_relay(1);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_snmp_collector_tests, test_get_sample_rate_relay)
{
    ASSERT_EQ(ORCM_SUCCESS, snmp_init_relay());

    snmp_get_sample_rate_relay(NULL);
    ASSERT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);

    snmp_finalize_relay();

    snmp_get_sample_rate_relay(NULL);
    ASSERT_EQ(ORCM_ERR_NOT_AVAILABLE, last_orte_error_);
}

TEST_F(ut_snmp_collector_tests, test_snmp_construction_and_init_finalize)
{
    snmp_impl dummy;

    ASSERT_NULL(dummy.ev_base_);
    ASSERT_NULL(dummy.snmp_sampler_);

    ASSERT_EQ(ORCM_SUCCESS, dummy.init());
    ASSERT_STREQ("test_host", dummy.hostname_.c_str());

    dummy.finalize();
}

TEST_F(ut_snmp_collector_tests, test_start_and_stop)
{
    snmp_impl dummy;

    dummy.init();

    dummy.start(0);
    ASSERT_NULL(dummy.ev_base_);
    ASSERT_NULL(dummy.snmp_sampler_);

    dummy.stop(0);
    ASSERT_NULL(dummy.ev_base_);
    ASSERT_NULL(dummy.snmp_sampler_);

    dummy.finalize();

    mca_sensor_snmp_component.use_progress_thread = true;
    dummy.init();

    dummy.start(0);
    EXPECT_NOT_NULL(dummy.ev_base_);
    EXPECT_NOT_NULL(dummy.snmp_sampler_);

    dummy.stop(0);
    ASSERT_NOT_NULL(dummy.ev_base_);
    ASSERT_NULL(dummy.snmp_sampler_);

    dummy.finalize();
}

TEST_F(ut_snmp_collector_tests, test_get_and_set_sample_rate)
{
    int rate;
    snmp_impl dummy;

    dummy.get_sample_rate(&rate);
    ASSERT_EQ(0, rate);

    rate = -1;
    dummy.set_sample_rate(5);
    dummy.get_sample_rate(&rate);
    ASSERT_EQ(0, rate);

    mca_sensor_snmp_component.use_progress_thread = true;

    rate = -1;
    dummy.get_sample_rate(&rate);
    ASSERT_EQ(0, rate);

    rate = -1;
    dummy.set_sample_rate(5);
    dummy.get_sample_rate(&rate);
    ASSERT_EQ(5, rate);
}

TEST_F(ut_snmp_collector_tests, test_ev_create_thread_and_destroy_thread)
{
    snmp_impl dummy;

    ResetTestEnvironment();

    dummy.init();
    dummy.start(0);

    dummy.ev_create_thread();
    ASSERT_NULL(dummy.ev_base_);

    dummy.snmp_sampler_ = OBJ_NEW(orcm_sensor_sampler_t);

    dummy.ev_create_thread();
    ASSERT_NOT_NULL(dummy.ev_base_);

    dummy.ev_destroy_thread();
    ASSERT_NULL(dummy.ev_base_);

    SAFE_OBJ_RELEASE(dummy.snmp_sampler_);

    dummy.stop(0);
    dummy.finalize();
}

TEST_F(ut_snmp_collector_tests, neg_test_ev_create_thread_and_destroy_thread)
{
    snmp_impl dummy;

    dummy.ev_create_thread();
    ASSERT_NULL(dummy.ev_base_);

    dummy.snmp_sampler_ = OBJ_NEW(orcm_sensor_sampler_t);

    dummy.ev_create_thread();
    ASSERT_NOT_NULL(dummy.ev_base_);
    dummy.ev_destroy_thread();
    ASSERT_NULL(dummy.ev_base_);

    SAFE_OBJ_RELEASE(dummy.snmp_sampler_);
}

TEST_F(ut_snmp_collector_tests, test_ev_pause_and_resume)
{
    snmp_impl dummy;

    dummy.init();
    dummy.start(0);

    ASSERT_NULL(dummy.ev_base_);
    ASSERT_FALSE(dummy.ev_paused_);

    dummy.ev_pause();
    ASSERT_FALSE(dummy.ev_paused_);
    dummy.ev_resume();
    ASSERT_FALSE(dummy.ev_paused_);

    dummy.stop(0);
    dummy.finalize();

    mca_sensor_snmp_component.use_progress_thread = true;
    dummy.init();
    dummy.start(0);

    ASSERT_NOT_NULL(dummy.ev_base_);
    ASSERT_FALSE(dummy.ev_paused_);

    dummy.ev_pause();
    ASSERT_TRUE(dummy.ev_paused_);
    dummy.ev_resume();
    ASSERT_FALSE(dummy.ev_paused_);

    dummy.stop(0);
    dummy.finalize();
}

void ut_snmp_collector_tests::sample_check(opal_buffer_t *bucket)
{
    const char *str = NULL;
    int32_t n = 1;
    char *plugin_name = NULL;
    opal_buffer_t *buf = NULL;
    opal_buffer_t *samplesBuffer = NULL;

    ASSERT_FALSE(NULL == bucket);

    ASSERT_EQ(OPAL_SUCCESS, opal_dss.unpack(bucket, &buf, &n, OPAL_BUFFER));
    ASSERT_EQ(OPAL_SUCCESS, opal_dss.unpack(buf, &plugin_name, &n, OPAL_STRING));

    vardata timeStamp = fromOpalBuffer(buf);
    vardata deviceName = fromOpalBuffer(buf);

    vector<vardata> collected_samples;
    int nSamples = fromOpalBuffer(buf).getValue<int>();

    for (int i = 0; i < nSamples; i++) {
        collected_samples.push_back(fromOpalBuffer(buf));
    }

    ASSERT_FALSE(collected_samples.empty());
    ASSERT_TRUE(2 < collected_samples.size());

    str = collected_samples[0].strData.c_str();
    ASSERT_STREQ(dataStr, str);

    ASSERT_EQ(collected_samples[1].getValue<int64_t>(), dataInt);
    ASSERT_FLOAT_EQ(collected_samples[2].getValue<float>(), dataFloat);
    ASSERT_DOUBLE_EQ(collected_samples[3].getValue<double>(), dataDouble);

    str = collected_samples[4].strData.c_str();
    ASSERT_STREQ(dataIpAddr, str);

    ASSERT_EQ(collected_samples[5].getValue<uint32_t>(), dataUint);
    ASSERT_EQ(collected_samples[6].getValue<uint32_t>(), dataUint);
    ASSERT_EQ(collected_samples[7].getValue<uint32_t>(), dataUint);

    OBJ_RELEASE(buf);
}

TEST_F(ut_snmp_collector_tests, test_perthread_snmp_sample)
{
    snmp_impl dummy;
    int fd = 0;
    short args = 0;

    mca_sensor_snmp_component.test = true;
    mca_sensor_snmp_component.use_progress_thread = true;

    dummy.init();
    dummy.start(fd);
    dummy.perthread_snmp_sample_relay(fd, args, (void*) &dummy);
    dummy.stop(fd);
    dummy.finalize();
}

TEST_F(ut_snmp_collector_tests, test_sample)
{
    snmp_impl dummy;

    dummy.init();

    opal_output_verbose_.clear();
    last_orte_error_ = 0;

    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);

    dummy.sample(&sampler);

    opal_buffer_t* buffer = &(sampler.bucket);
    sample_check(buffer);

    dummy.finalize();
}

TEST_F(ut_snmp_collector_tests, test_log)
{
    int32_t n = 1;
    snmp_impl dummy;
    char *plugin_name = NULL;
    opal_buffer_t *buf;

    dummy.init();

    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);

    dummy.sample(&sampler);

    ASSERT_EQ(OPAL_SUCCESS, opal_dss.unpack(&sampler.bucket, &buf, &n, OPAL_BUFFER));
    ASSERT_EQ(OPAL_SUCCESS, opal_dss.unpack(buf, &plugin_name, &n, OPAL_STRING));

    dummy.log(buf);

    OBJ_RELEASE(buf);
    dummy.finalize();
}

TEST_F(ut_snmp_collector_tests, test_component_functions)
{
    int ret = 0;
    int priority = -1;
    mca_base_module_t* module = NULL;

    ret = orcm_sensor_snmp_query(&module, &priority);
    ASSERT_EQ(ORCM_SUCCESS, ret);
    ASSERT_NE(-1, priority);
    ASSERT_NOT_NULL(module);

    ret = orcm_sensor_snmp_open();
    ASSERT_EQ(ORCM_SUCCESS, ret);

    ret = snmp_component_register();
    ASSERT_EQ(ORCM_SUCCESS, ret);

    ret = orcm_sensor_snmp_close();
    ASSERT_EQ(ORCM_SUCCESS, ret);
}

TEST_F(ut_snmp_collector_tests, test_v3_constructors)
{
    snmpCollector *collector;

    collector = new snmpCollector("192.168.1.100", "username", "password");
    ASSERT_NOT_NULL(collector);
    delete collector;
}

TEST_F(ut_snmp_collector_tests, test_getOIDs)
{
    snmp_impl snmp;
    snmpCollector collector;

    collector.setOIDs(OIDS_LIST);
    list<string> oidList = collector.getOIDsList();
    ASSERT_EQ(OIDS_LIST_SIZE, oidList.size());
    EXPECT_STREQ(oidList.front().c_str(), OID_0);
    EXPECT_STREQ(oidList.back().c_str(), OID_1);

    vector<vardata> v = snmp.getOIDsVardataVector(collector);
    ASSERT_EQ(OIDS_LIST_SIZE, v.size());
    EXPECT_STREQ(v[0].strData.c_str(), OID_0);
    EXPECT_STREQ(v[1].strData.c_str(), OID_1);
}

TEST_F(ut_snmp_collector_tests, noSnmpConfigAvailable)
{
    snmp_impl snmp;

    mca_sensor_snmp_component.config_file = (char *) NO_AGGREGATORS_XML_NAME;
    testFilesObj.writeInvalidSnmpConfigFile();
    EXPECT_TRUE(ORCM_ERROR == snmp.init());

    mca_sensor_snmp_component.config_file = NULL;
    testFilesObj.removeInvalidSnmpConfigFile();
}

TEST_F(ut_snmp_collector_tests, test_negative_collectData)
{
    snmpCollector *collector;
    collector = new snmpCollector("192.168.1.100", "public");
    collector->setOIDs(OID_0);

    // Timeout
    snmp_mocking.snmp_synch_response_callback = SnmpSynchResponse_timeout;
    ASSERT_THROW(collector->collectData(), snmpTimeout);

    // Data collection error
    snmp_mocking.snmp_synch_response_callback = SnmpSynchResponse_error;
    ASSERT_THROW(collector->collectData(), dataCollectionError);

    // Data packet collection error
    snmp_mocking.snmp_synch_response_callback = SnmpSynchResponse_packeterror;
    ASSERT_THROW(collector->collectData(), packetError);

    //Invalid snmp session
    snmp_mocking.snmp_open_callback = SnmpOpenReturnNull;
    ASSERT_THROW(collector->collectData(), invalidSession);

    delete collector;
}

TEST_F(ut_snmp_collector_tests, snmp_sensor_sample_tests)
{
    ResetTestEnvironment();
    {
        orcm_sensor_sampler_t sampler;
        OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
        snmp_impl snmp;
        orcm_sensor_base.collect_metrics = false;
        mca_sensor_snmp_component.collect_metrics = false;
        snmp.init();
        snmp.start(0);
        snmp.sample(&sampler);
        EXPECT_EQ(0, (snmp.diagnostics_ & 0x1));
        snmp.stop(0);
        snmp.finalize();
        OBJ_DESTRUCT(&sampler);
    }
    {
        orcm_sensor_sampler_t sampler;
        OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
        snmp_impl snmp;
        orcm_sensor_base.collect_metrics = true;
        mca_sensor_snmp_component.collect_metrics = false;
        snmp.init();
        snmp.start(0);
        snmp.sample(&sampler);
        snmp.start(0);
        EXPECT_EQ(0, (snmp.diagnostics_ & 0x1));
        snmp.stop(0);
        snmp.finalize();
        OBJ_DESTRUCT(&sampler);
    }
    {
        orcm_sensor_sampler_t sampler;
        OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
        snmp_impl snmp;
        orcm_sensor_base.collect_metrics = true;
        mca_sensor_snmp_component.collect_metrics = true;
        snmp.init();
        snmp.start(0);
        snmp.sample(&sampler);
        EXPECT_EQ(1, (snmp.diagnostics_ & 0x1));
        snmp.stop(0);
        snmp.finalize();
        OBJ_DESTRUCT(&sampler);
    }
    orcm_sensor_base.collect_metrics = true;
    mca_sensor_snmp_component.collect_metrics = true;
}

TEST_F(ut_snmp_collector_tests, snmp_sensor_api_tests)
{
    // Setup
    orcm_sensor_base.collect_metrics = false;
    mca_sensor_snmp_component.collect_metrics = false;

    snmp_init_relay();
    snmp_start_relay(0);

    // Tests
    EXPECT_TRUE(ORCM_SUCCESS == snmp_enable_sampling_relay("snmp"));
    EXPECT_TRUE(ORCM_SUCCESS == snmp_reset_sampling_relay("snmp"));
    EXPECT_TRUE(ORCM_SUCCESS == snmp_enable_sampling_relay("all"));
    EXPECT_TRUE(ORCM_SUCCESS == snmp_disable_sampling_relay("not_the_right_datagroup"));
    EXPECT_TRUE(ORCM_SUCCESS == snmp_disable_sampling_relay("snmp"));

    // Cleanup
    snmp_stop_relay(0);
    snmp_finalize_relay();
}

TEST_F(ut_snmp_collector_tests, snmp_api_tests)
{
    // Setup
    snmp_impl snmp;
    orcm_sensor_base.collect_metrics = false;
    mca_sensor_snmp_component.collect_metrics = false;

    snmp.init();
    snmp.start(0);

    // Tests
    snmp.enable_sampling("snmp");
    EXPECT_TRUE(snmp.runtime_metrics_->DoCollectMetrics(NULL));
    snmp.reset_sampling("snmp");
    EXPECT_FALSE(snmp.runtime_metrics_->DoCollectMetrics(NULL));
    snmp.enable_sampling("all");
    EXPECT_TRUE(snmp.runtime_metrics_->DoCollectMetrics(NULL));
    snmp.disable_sampling("not_the_right_datagroup");
    EXPECT_TRUE(snmp.runtime_metrics_->DoCollectMetrics(NULL));
    snmp.disable_sampling("snmp");
    EXPECT_FALSE(snmp.runtime_metrics_->DoCollectMetrics(NULL));

    // Cleanup
    snmp.stop(0);
    snmp.finalize();
}


TEST_F(ut_snmp_collector_tests, snmp_api_tests_2)
{
    // Setup
    snmp_impl snmp;
    orcm_sensor_base.collect_metrics = false;
    mca_sensor_snmp_component.collect_metrics = false;
    snmp.init();
    snmp.start(0);

    snmp.runtime_metrics_->TrackSensorLabel("PDU Temp 1");
    snmp.runtime_metrics_->TrackSensorLabel("PDU Temp 2");
    snmp.runtime_metrics_->TrackSensorLabel("PDU Temp 3");
    snmp.runtime_metrics_->TrackSensorLabel("PDU Temp 4");

    // Tests
    snmp.enable_sampling("snmp:PDU Temp 3");
    EXPECT_FALSE(snmp.runtime_metrics_->DoCollectMetrics("PDU Temp 1"));
    EXPECT_TRUE(snmp.runtime_metrics_->DoCollectMetrics("PDU Temp 3"));
    EXPECT_EQ(1,snmp.runtime_metrics_->CountOfCollectedLabels());
    snmp.disable_sampling("snmp:PDU Temp 3");
    EXPECT_FALSE(snmp.runtime_metrics_->DoCollectMetrics("PDU Temp 3"));
    snmp.enable_sampling("snmp:PDU Temp 2");
    EXPECT_TRUE(snmp.runtime_metrics_->DoCollectMetrics("PDU Temp 2"));
    snmp.reset_sampling("snmp:PDU Temp 2");
    EXPECT_FALSE(snmp.runtime_metrics_->DoCollectMetrics("PDU Temp 2"));
    snmp.enable_sampling("snmp:no_PDU");
    EXPECT_FALSE(snmp.runtime_metrics_->DoCollectMetrics("PDU Temp 2"));
    snmp.enable_sampling("snmp:all");
    EXPECT_TRUE(snmp.runtime_metrics_->DoCollectMetrics("PDU Temp 0"));
    EXPECT_TRUE(snmp.runtime_metrics_->DoCollectMetrics("PDU Temp 3"));
    EXPECT_EQ(4,snmp.runtime_metrics_->CountOfCollectedLabels());

    // Cleanup
    snmp.stop(0);
    snmp.finalize();
}

#define TEST_VECTOR_SIZE 8
static struct test_data {
    const char* key;
    float value;
} test_vector[TEST_VECTOR_SIZE] = {
    { "PDU1 Power", 1234.0 },
    { "PDU1 Avg Power", 894.0 },
    { "PDU1 Min Power", 264.0 },
    { "PDU1 Max Power", 2352.0 },
    { "PDU2 Power", 1234.0 },
    { "PDU2 Avg Power", 894.0 },
    { "PDU2 Min Power", 264.0 },
    { "PDU2 Max Power", 2352.0 },
};


TEST_F(ut_snmp_collector_tests, snmp_generate_test_data)
{
    orcm_sensor_sampler_t sampler;
    opal_buffer_t* bucket = &(sampler.bucket);
    snmp_impl snmp;

    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);
    mca_sensor_snmp_component.test = true;

    snmp.init();
    snmp.sample(&sampler);
    snmp.finalize();

    mca_sensor_snmp_component.test = false;

    opal_buffer_t* buffer = NULL;
    int n = 1;
    int rc = opal_dss.unpack(bucket, &buffer, &n, OPAL_BUFFER);
    OBJ_DESTRUCT(&sampler);
    ASSERT_EQ(ORCM_SUCCESS, rc);
    char* hostname = NULL;
    n = 1;
    rc = opal_dss.unpack(buffer, &hostname, &n, OPAL_STRING);
    EXPECT_EQ(ORCM_SUCCESS, rc);
    EXPECT_STREQ(plugin_name_, hostname) << "Wrong plugin name!";
    EXPECT_NO_THROW({
        vardata time_stamp = fromOpalBuffer(buffer);
        EXPECT_STREQ(CTIME_STR, time_stamp.getKey().c_str()) << "Wrong order; expected time stamp!";
    });
    EXPECT_NO_THROW({
        vardata hostname = fromOpalBuffer(buffer);
        EXPECT_STREQ(HOSTNAME_STR, hostname.getKey().c_str()) << "Wrong order; expected hostname!";
    });
    for(int i = 0; i < TEST_VECTOR_SIZE; ++i) {
        EXPECT_NO_THROW({
            vardata item = fromOpalBuffer(buffer);
            EXPECT_STREQ(test_vector[i].key, item.getKey().c_str()) << "Wrong order; expected item label!";
            EXPECT_EQ(test_vector[i].value, item.getValue<float>()) << "Wrong value retreived!";
        });
    }

    snmp.finalize();
    mca_sensor_snmp_component.test = false;
    SAFE_OBJ_RELEASE(buffer);
}

TEST_F(ut_snmp_collector_tests, test_snmp_inventory_test_branch)
{
    char *plugin_name = NULL;
    int32_t n = 1;
    opal_buffer_t *buffer = OBJ_NEW(opal_buffer_t);
    mca_sensor_snmp_component.test = true;
    orcm_sensor_base.dbhandle = -1;

    snmp_init_relay();
    snmp_inventory_collect(buffer);
    EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);
    EXPECT_EQ(OPAL_SUCCESS, opal_dss.unpack(buffer, &plugin_name, &n, OPAL_STRING));
    snmp_inventory_log((char*)hostname_, buffer);
    EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);
    snmp_finalize_relay();

    SAFE_OBJ_RELEASE(buffer);
}

TEST_F(ut_snmp_collector_tests, test_snmp_corrupted_inventory_buffer)
{
    opal_buffer_t *buffer = OBJ_NEW(opal_buffer_t);
    mca_sensor_snmp_component.test = true;

    snmp_init_relay();
    snmp_inventory_collect(buffer);
    EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);
    snmp_inventory_log((char*)hostname_, buffer);
    EXPECT_NE(ORCM_SUCCESS, last_orte_error_);
    snmp_finalize_relay();

    SAFE_OBJ_RELEASE(buffer);
}

TEST_F(ut_snmp_collector_tests, test_snmp_inventory_no_dbhandle)
{
    char *plugin_name = NULL;
    int32_t n = 1;
    opal_buffer_t *buffer = OBJ_NEW(opal_buffer_t);
    orcm_sensor_base.dbhandle = -1;

    snmp_init_relay();
    snmp_inventory_collect(buffer);
    EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);
    EXPECT_EQ(OPAL_SUCCESS, opal_dss.unpack(buffer, &plugin_name, &n, OPAL_STRING));
    snmp_inventory_log((char*)hostname_, buffer);
    EXPECT_EQ(ORCM_SUCCESS, last_orte_error_);
    snmp_finalize_relay();

    SAFE_OBJ_RELEASE(buffer);
}

TEST_F(ut_snmp_collector_tests, test_snmp_inventory_null_buffer)
{
    snmp_init_relay();
    snmp_inventory_collect(NULL);
    EXPECT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);
    snmp_inventory_log((char*)hostname_, NULL );
    EXPECT_EQ(ORTE_ERR_BAD_PARAM, last_orte_error_);
    snmp_finalize_relay();
}

TEST_F(ut_snmp_collector_tests, test_unable_to_allocate_obj)
{

    int32_t n = 1;
    snmp_impl dummy;
    char *plugin_name = NULL;
    opal_buffer_t *buf;
    orcm_sensor_sampler_t sampler;
    OBJ_CONSTRUCT(&sampler, orcm_sensor_sampler_t);

    snmp_mocking.orcm_util_load_orcm_analytics_value_callback = OrcmUtilLoadAnalyticsValue;

    dummy.init();
    dummy.sample(&sampler);
    ASSERT_EQ(OPAL_SUCCESS, opal_dss.unpack(&sampler.bucket, &buf, &n, OPAL_BUFFER));
    ASSERT_EQ(OPAL_SUCCESS, opal_dss.unpack(buf, &plugin_name, &n, OPAL_STRING));

    dummy.log(buf);

    dummy.finalize();
    SAFE_OBJ_RELEASE(buf);
}


TEST_F(ut_snmp_collector_tests, snmp_generate_inv_test_data)
{
    opal_buffer_t* buffer = OBJ_NEW(opal_buffer_t);
    mca_sensor_snmp_component.test = true;
    snmp_init_relay();
    snmp_inventory_collect(buffer);
    snmp_finalize_relay();
    mca_sensor_snmp_component.test = false;

    int n = 1;
    char* hostname = NULL;
    int rc = opal_dss.unpack(buffer, &hostname, &n, OPAL_STRING);
    EXPECT_EQ(ORCM_SUCCESS, rc);
    EXPECT_STREQ(plugin_name_, hostname) << "Wrong plugin name!";
    EXPECT_NO_THROW({
        vardata time_stamp = fromOpalBuffer(buffer);
        EXPECT_STREQ(CTIME_STR, time_stamp.getKey().c_str()) << "Wrong order; expected time stamp!";
    });
    EXPECT_NO_THROW({
        vardata hostname_count = fromOpalBuffer(buffer);
        EXPECT_STREQ(TOT_HOSTNAMES_STR, hostname_count.getKey().c_str()) << "Wrong order; expected hostname count!";
        EXPECT_EQ(1, hostname_count.getValue<uint64_t>());
    });
    EXPECT_NO_THROW({
        vardata items_count = fromOpalBuffer(buffer);
        EXPECT_STREQ(TOT_ITEMS_STR, items_count.getKey().c_str()) << "Wrong order; expected items count!";
        EXPECT_EQ(TEST_VECTOR_SIZE+1, items_count.getValue<uint64_t>());
    });
    EXPECT_NO_THROW({
        vardata hostname_str = fromOpalBuffer(buffer);
        EXPECT_STREQ(HOSTNAME_STR, hostname_str.getKey().c_str()) << "Wrong order; expected 'hostname' key!";
       });
    for(int i = 0; i < TEST_VECTOR_SIZE; ++i) {
        EXPECT_NO_THROW({
            vardata item = fromOpalBuffer(buffer);
            stringstream ss;
            ss << "sensor_snmp_" << (i+1);
            EXPECT_STREQ(ss.str().c_str(), item.getKey().c_str()) << "Wrong order; expected item label!";
            //EXPECT_STREQ(test_vector[i].key, item.getValue<char*>()) << "Wrong value retreived!";
            EXPECT_STREQ(test_vector[i].key, item.strData.c_str()) << "Wrong value retreived!";
        });
    }

    SAFE_OBJ_RELEASE(buffer);
}
