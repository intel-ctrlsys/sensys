/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "snmp_parser_tests.h"

using namespace std;

TEST_F(ut_snmp_parser_tests, parse_default_file){
    vector<snmpCollector> v;
    snmpParser sp;
    v = sp.parse();
    //ASSERT_EQ(X, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_no_file){
    snmpParser sp(""); // It will take the default snmp configuration file.
    sp.parse();
}

TEST_F(ut_snmp_parser_tests, parse_file_not_found){
    snmpParser sp("unexistent_file.conf");
    ASSERT_THROW(sp.parse(), fileNotFound);
}

TEST_F(ut_snmp_parser_tests, parse_file_with_no_tags){
    vector<snmpCollector> v;
    snmpParser sp("test_files/no_tags.conf");
    v = sp.parse();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_file_with_no_snmp_config_tags){
    vector<snmpCollector> v;
    snmpParser sp("test_files/no_snmp_config.conf");
    v = sp.parse();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_file_with_no_node_tags){
    vector<snmpCollector> v;
    snmpParser sp("test_files/no_nodes.conf");
    v = sp.parse();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_file_with_no_snmp_config_information_for_monitored_nodes){
    vector<snmpCollector> v;
    snmpParser sp("test_files/no_snmp_config_for_monitored_nodes.conf");
    v = sp.parse();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_file_with_no_node_information_for_this_host){
    vector<snmpCollector> v;
    snmpParser sp("test_files/no_node_information_for_this_host.conf");
    v = sp.parse();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_empty_file){
    vector<snmpCollector> v;
    snmpParser sp("test_files/empty.conf");
    v = sp.parse();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_successful_file){
    vector<snmpCollector> v;
    snmpParser sp("test_files/successful.conf");
    v = sp.parse();
    ASSERT_EQ(10, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_wrong_snmp_config_tag_entries){
    vector<snmpCollector> v;
    snmpParser sp("test_files/wrong_snmp_tag_entries.conf");
    v = sp.parse();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_wrong_version_number_01){
    snmpParser sp("test_files/wrong_version_number_01.conf");
    // It will try to create an snmpCollector version3 without providing the
    // password, which will throw an invalidPassword exception.
    ASSERT_THROW(sp.parse(), invalidPassword);
}

TEST_F(ut_snmp_parser_tests, parse_wrong_version_number_02){
    vector<snmpCollector> v;
    snmpParser sp("test_files/wrong_version_number_02.conf");
    v = sp.parse();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_no_version_number){
    vector<snmpCollector> v;
    snmpParser sp("test_files/no_version_number.conf");
    v = sp.parse();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_default_auth_value){
    vector<snmpCollector> v;
    snmpParser sp("test_files/default_auth_value.conf");
    v = sp.parse();
    ASSERT_EQ(10, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_SHA1_auth_value){
    vector<snmpCollector> v;
    snmpParser sp("test_files/sha1_auth_value.conf");
    v = sp.parse();
    ASSERT_EQ(10, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_default_sec_value){
    vector<snmpCollector> v;
    snmpParser sp("test_files/default_sec_value.conf");
    v = sp.parse();
    ASSERT_EQ(10, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_noauth_sec_value){
    vector<snmpCollector> v;
    snmpParser sp("test_files/noauth_sec_value.conf");
    v = sp.parse();
    ASSERT_EQ(10, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_authpriv_sec_value){
    vector<snmpCollector> v;
    snmpParser sp("test_files/authpriv_sec_value.conf");
    v = sp.parse();
    ASSERT_EQ(10, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_wrong_oids_v1){
    snmpParser sp("test_files/wrong_oids_v1.conf");
    ASSERT_THROW(sp.parse(), invalidOIDParsing);
}

TEST_F(ut_snmp_parser_tests, parse_wrong_oids_v3){
    snmpParser sp("test_files/wrong_oids_v3.conf");
    ASSERT_THROW(sp.parse(), invalidOIDParsing);
}

TEST_F(ut_snmp_parser_tests, parse_multiple_host_entries_on_snmp_config_tag){
    vector<snmpCollector> v;
    snmpParser sp("test_files/multiple_hosts_on_snmp_config_tag.conf");
    v = sp.parse();
    ASSERT_EQ(20, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_multiple_host_entries_on_multiple_snmp_config_tag){
    vector<snmpCollector> v;
    snmpParser sp("test_files/multiple_hosts_on_multiple_snmp_config_tag.conf");
    v = sp.parse();
    ASSERT_EQ(20, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_close_file){
    snmpParser sp("test_files/successful.conf");
    sp.unsetParseFile();
    ASSERT_THROW(sp.parse(), fileNotFound);
}
