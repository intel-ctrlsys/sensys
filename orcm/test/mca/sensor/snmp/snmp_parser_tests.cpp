/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "snmp_parser_tests.h"

#define NUM_COLLECTORS 22

using namespace std;

void ut_snmp_parser_tests::initParserFramework()
{
    OBJ_CONSTRUCT(&orcm_parser_base_framework.framework_components, opal_list_t);
    OBJ_CONSTRUCT(&orcm_parser_base.actives, opal_list_t);
    orcm_parser_active_module_t *act_module;
    act_module = OBJ_NEW(orcm_parser_active_module_t);
    act_module->priority = MY_MODULE_PRIORITY;
    act_module->module = &orcm_parser_pugi_module;
    opal_list_append(&orcm_parser_base.actives, &act_module->super);
}

void ut_snmp_parser_tests::cleanParserFramework()
{
    OPAL_LIST_DESTRUCT(&orcm_parser_base_framework.framework_components);
    OPAL_LIST_DESTRUCT(&orcm_parser_base.actives);
}

TEST_F(ut_snmp_parser_tests, parse_default_file){
    snmpParser sp; // It will take the default snmp configuration file.
    sp.getSnmpCollectorVector();
}

TEST_F(ut_snmp_parser_tests, parse_no_file){
    snmpParser sp(""); // It will take the default snmp configuration file.
    sp.getSnmpCollectorVector();
}

TEST_F(ut_snmp_parser_tests, parse_file_not_found){
    ASSERT_THROW(snmpParser sp("unexistent_file.xml"), fileNotFound);
}

TEST_F(ut_snmp_parser_tests, parse_file_with_no_tags){
    ASSERT_THROW(snmpParser sp(test_files_dir+"no_tags.xml"), fileNotFound);
}

TEST_F(ut_snmp_parser_tests, parse_file_with_no_aggregator_tags){
    snmpCollectorVector v;
    snmpParser sp(test_files_dir+"no_aggregators.xml");
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_file_with_no_hostname_tags){
    snmpCollectorVector v;
    snmpParser sp(test_files_dir+"no_hostname.xml");
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_empty_file){
    ASSERT_THROW(snmpParser sp(test_files_dir+"empty.xml"), fileNotFound);
}

TEST_F(ut_snmp_parser_tests, parse_successful_file){
    snmpCollectorVector v;
    snmpParser sp(test_files_dir+"successful.xml");
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(NUM_COLLECTORS, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_wrong_snmp_config_tag_entries){
    snmpCollectorVector v;
    snmpParser sp(test_files_dir+"wrong_snmp_tag_entries.xml");
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_wrong_version_number_01){
    ASSERT_THROW(snmpParser sp(test_files_dir+"wrong_version_number_01.xml"), invalidPassword);
    // It will try to create an snmpCollector version3 without providing the
    // password, which will throw an invalidPassword exception.
}

TEST_F(ut_snmp_parser_tests, parse_wrong_version_number_02){
    snmpCollectorVector v;
    snmpParser sp(test_files_dir+"wrong_version_number_02.xml");
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_no_version_number){
    snmpCollectorVector v;
    snmpParser sp(test_files_dir+"no_version_number.xml");
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_default_auth_value){
    snmpCollectorVector v;
    snmpParser sp(test_files_dir+"default_auth_value.xml");
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(NUM_COLLECTORS, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_SHA1_auth_value){
    snmpCollectorVector v;
    snmpParser sp(test_files_dir+"sha1_auth_value.xml");
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(NUM_COLLECTORS, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_default_sec_value){
    snmpCollectorVector v;
    snmpParser sp(test_files_dir+"default_sec_value.xml");
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(NUM_COLLECTORS, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_noauth_sec_value){
    snmpCollectorVector v;
    snmpParser sp(test_files_dir+"noauth_sec_value.xml");
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(NUM_COLLECTORS, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_authpriv_sec_value){
    snmpCollectorVector v;
    snmpParser sp(test_files_dir+"authpriv_sec_value.xml");
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(NUM_COLLECTORS, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_wrong_oids_v1){
    ASSERT_THROW(snmpParser sp(test_files_dir+"wrong_oids_v1.xml"), invalidOIDParsing);
}

TEST_F(ut_snmp_parser_tests, parse_wrong_oids_v3){
    ASSERT_THROW(snmpParser sp(test_files_dir+"wrong_oids_v3.xml"), invalidOIDParsing);
}
