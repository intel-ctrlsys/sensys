/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SNMP_COLLECTOR_TESTS_H
#define SNMP_COLLECTOR_TESTS_H

#include "gtest/gtest.h"
#include "snmp_tests_mocking.h"

#include <map>
#include <vector>
#include <queue>

#include "orcm/mca/db/db.h"

class ut_snmp_collector_tests: public testing::Test
{
    protected:
        /* gtests */
        static void SetUpTestCase();
        static void TearDownTestCase();
        static void ClearBuffers();
        static void ResetTestEnvironment();

        /* Mocking */
        static void OrteErrmgrBaseLog(int err, char* file, int lineno);
        static void OpalOutputVerbose(int level, int output_id, const char* line);
        static char* OrteUtilPrintNameArgs(const orte_process_name_t *name);
        static void OrcmAnalyticsBaseSendData(orcm_analytics_value_t* data);
        static opal_event_base_t* OpalProgressThreadInit(const char* name);
        static int OpalProgressThreadFinalize(const char* name);
        static void MyDbStoreNew(int dbhandle, orcm_db_data_type_t data_type, opal_list_t *input,
                                 opal_list_t *ret, orcm_db_callback_fn_t cbfunc, void *cbdata);
        static struct snmp_session* SnmpOpen(struct snmp_session* ss);
        static int SnmpSynchResponse(netsnmp_session *session, netsnmp_pdu *pdu, netsnmp_pdu **response);
        static void SnmpFreePdu(netsnmp_pdu *pdu);
        static void sample_check(opal_buffer_t *bucket);
        static struct snmp_pdu *SnmpPDUCreate(int command);
        static int ReadObjid(const char *input, oid *objid, size_t *objidlen);
        static int PrintObjid(char *buf, size_t len,  oid *objid, size_t *objidlen);
        static netsnmp_variable_list *SnmpAddNullVar(netsnmp_pdu *pdu, const oid *objid, size_t objidlen);
        static vector<snmpCollector> init(void);

        static snmpCollector *collector;
        static const char* hostname_;
        static const char* plugin_name_;
        static const char* proc_name_;
        static int last_errno_;
        static std::string last_error_filename_;
        static void* last_user_data_;
        static int last_orte_error_;
        static int fail_pack_on_;
        static int fail_pack_count_;
        static int fail_unpack_on_;
        static int fail_unpack_count_;
        static bool fail_pack_buffer_;
        static bool fail_unpack_buffer_;
        static std::map<std::string,int> logged_data_;
        static std::map<std::string,int> golden_data_;
        static std::vector<std::string> opal_output_verbose_;
        static std::queue<int32_t> packed_int32_;
        static std::queue<std::string> packed_str_;
        static std::queue<struct timeval> packed_ts_;
        static std::vector<orcm_analytics_value_t*> current_analytics_values_;
        static std::map<std::string,std::string> database_data_;
};

#endif // SNMP_COLLECTOR_TESTS_H
