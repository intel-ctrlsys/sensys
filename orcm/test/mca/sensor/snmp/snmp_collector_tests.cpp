/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
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
}

#define NOOP_JOBID -999
#define SNMP_ERR_NOERROR 0

#define SAFE_OBJ_RELEASE(x) if(NULL!=x) OBJ_RELEASE(x); x=NULL
#define ASSERT_PTREQ(x,y)  ASSERT_EQ((void*)x,(void*)y)
#define ASSERT_PTRNE(x,y)  ASSERT_NE((void*)x,(void*)y)
#define ASSERT_NULL(x)     ASSERT_PTREQ(NULL,x)
#define ASSERT_NOT_NULL(x) ASSERT_PTRNE(NULL,x)
#define EXPECT_PTREQ(x,y)  EXPECT_EQ((void*)x,(void*)y)
#define EXPECT_PTRNE(x,y)  EXPECT_NE((void*)x,(void*)y)
#define EXPECT_NULL(x)     EXPECT_PTREQ(NULL,x)
#define EXPECT_NOT_NULL(x) EXPECT_PTRNE(NULL,x)

/* Fixture */
using namespace std;

char dataStr[] = "jelou";
int long dataInt = 9;
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

extern "C" {
    extern orcm_sensor_snmp_component_t mca_sensor_snmp_component;
    extern orte_proc_info_t orte_process_info;

    extern int orcm_sensor_snmp_open(void);
    extern int orcm_sensor_snmp_close(void);
    extern int orcm_sensor_snmp_query(mca_base_module_t **module, int *priority);
    extern int snmp_component_register(void);
}

void ut_snmp_collector_tests::SetUpTestCase()
{
    opal_dss_register_vars();

    opal_init_test();

    ResetTestEnvironment();
}

void ut_snmp_collector_tests::TearDownTestCase()
{
    golden_data_.clear();

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
    snmp_mocking.read_objid_callback = NULL;
    snmp_mocking.snmp_add_null_var_callback = NULL;
    snmp_mocking.snprint_objid_callback = NULL;
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
    snmp_mocking.read_objid_callback = ReadObjid;
    snmp_mocking.snmp_add_null_var_callback = SnmpAddNullVar;
    snmp_mocking.snprint_objid_callback = PrintObjid;

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
}

/* Mocking methods */
struct snmp_session* ut_snmp_collector_tests::SnmpOpen(struct snmp_session* ss)
{
  return new snmp_session;
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

int ut_snmp_collector_tests::ReadObjid(const char *input, oid *objid, size_t *objidlen)
{
    return 1;
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
    t_double->next_variable = NULL;

    (*response)->variables = t_str;

    return STAT_SUCCESS;
}

void ut_snmp_collector_tests::SnmpFreePdu(netsnmp_pdu *pdu)
{
  netsnmp_variable_list *vars = pdu->variables;

  delete pdu;

  return;
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
   int32_t n = 1;
   char *plugin_name = NULL;
   opal_buffer_t *buf = NULL;

   ASSERT_FALSE(NULL == bucket);

   ASSERT_EQ(OPAL_SUCCESS, opal_dss.unpack(bucket, &buf, &n, OPAL_BUFFER));
   ASSERT_EQ(OPAL_SUCCESS, opal_dss.unpack(buf, &plugin_name, &n, OPAL_STRING));

   vector<vardata> collected_samples = unpackDataFromBuffer(buf);

   ASSERT_FALSE(collected_samples.empty());

   char *str = collected_samples[2].getValue<char*>();
   ASSERT_STREQ(dataStr, str);
   ASSERT_EQ(collected_samples[3].getValue<int64_t>(), dataInt);
   ASSERT_FLOAT_EQ(collected_samples[4].getValue<float>(), dataFloat);
   ASSERT_DOUBLE_EQ(collected_samples[5].getValue<double>(), dataDouble);

   OBJ_RELEASE(buf);
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

TEST_F(ut_snmp_collector_tests, test_pack_collected_data_function)
{
    char *str;
    snmpCollector *collector;
    vector<vardata> collectedData;

    collector = new snmpCollector("192.168.1.100", "public");
    collectedData = collector->collectData();
    ASSERT_EQ(collectedData[0].getDataType(),OPAL_STRING);

    for (int i = 0; i < collectedData.size(); ++i) {
        switch (collectedData[i].getDataType()) {
            case OPAL_STRING:
                str = collectedData[i].getValue<char*>();
                ASSERT_STREQ(str, dataStr);
                break;
            case OPAL_INT32:
                ASSERT_EQ(collectedData[i].getValue<int32_t>(), dataInt);
                break;
            case OPAL_INT64:
                ASSERT_EQ(collectedData[i].getValue<int64_t>(), dataInt);
                break;
            case OPAL_FLOAT:
                ASSERT_FLOAT_EQ(collectedData[i].getValue<float>(), dataFloat);
                break;
            case OPAL_DOUBLE:
                ASSERT_DOUBLE_EQ(collectedData[i].getValue<double>(), dataDouble);
                break;
            default:
                break;
        }
    }
    delete collector;
}
