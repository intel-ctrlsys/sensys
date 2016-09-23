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

void ut_snmp_parser_tests::SetUpTestCase(){
    opal_init_test();
    initParserFramework();
    writeConfigFile();
    writeTestFiles();
}

void ut_snmp_parser_tests::TearDownTestCase(){
    cleanParserFramework();
    removeTestFiles();
    removeConfigFile();
}

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

void ut_snmp_parser_tests::writeConfigFile()
{
    int ret = testFilesObj.writeDefaultSnmpConfigFile();
    ASSERT_TRUE(ORCM_SUCCESS == ret);
}

void ut_snmp_parser_tests::removeConfigFile()
{
    int ret = testFilesObj.removeDefaultSnmpConfigFile();
    ASSERT_TRUE(ORCM_SUCCESS == ret);
}

void ut_snmp_parser_tests::writeTestFiles()
{
    int ret = testFilesObj.writeTestFiles();
    ASSERT_TRUE(ORCM_SUCCESS == ret);
}

void ut_snmp_parser_tests::removeTestFiles()
{
    int ret = testFilesObj.removeTestFiles();
    ASSERT_TRUE(ORCM_SUCCESS == ret);
}

TEST_F(ut_snmp_parser_tests, parse_default_file){
    orcm_cfgi_base.config_file = strdup(SNMP_DEFAULT_FILE_NAME);
    orcm_cfgi_base.version = 3.1;

    snmpParser sp; // It will take the default snmp configuration file.
    sp.getSnmpCollectorVector();

    free(orcm_cfgi_base.config_file);
}

TEST_F(ut_snmp_parser_tests, parse_no_file){
    orcm_cfgi_base.config_file = strdup(SNMP_DEFAULT_FILE_NAME);
    orcm_cfgi_base.version = 3.1;

    char *prefix = opal_install_dirs.prefix;
    opal_install_dirs.prefix = NULL;
    snmpParser sp(""); // It will take the default snmp configuration file.
    sp.getSnmpCollectorVector();
    opal_install_dirs.prefix = prefix;

    free(orcm_cfgi_base.config_file);
}

TEST_F(ut_snmp_parser_tests, parse_file_not_found){
    ASSERT_THROW(snmpParser sp("unexistent_file.xml"), fileNotFound);
}

TEST_F(ut_snmp_parser_tests, parse_file_with_no_tags){
    ASSERT_THROW(snmpParser sp(NO_TAGS_XML_NAME), fileNotFound);
}

TEST_F(ut_snmp_parser_tests, parse_file_with_no_aggregator_tags){
    snmpCollectorVector v;
    snmpParser sp(NO_AGGREGATORS_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_file_with_no_hostname_tags){
    snmpCollectorVector v;
    snmpParser sp(NO_HOSTNAME_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_empty_file){
    ASSERT_THROW(snmpParser sp(EMPTY_XML_NAME), fileNotFound);
}

TEST_F(ut_snmp_parser_tests, parse_successful_file){
    snmpCollectorVector v;
    snmpParser sp(SUCCESSFUL_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(EXPECTED_COLLECTORS_BY_CONFIG*2, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_wrong_snmp_config_tag_entries){
    snmpCollectorVector v;
    snmpParser sp(WRONG_SNMP_TAG_ENTRIES_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(EXPECTED_COLLECTORS_BY_CONFIG*2, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_wrong_version_number_01){
    ASSERT_THROW(snmpParser sp(WRONG_VERSION_NUMBER_01_XML_NAME), invalidPassword);
    // It will try to create an snmpCollector version3 without providing the
    // password, which will throw an invalidPassword exception.
}

TEST_F(ut_snmp_parser_tests, parse_wrong_version_number_02){
    snmpCollectorVector v;
    snmpParser sp(WRONG_VERSION_NUMBER_02_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_no_version_number){
    snmpCollectorVector v;
    snmpParser sp(NO_VERSION_NUMBER_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(0, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_default_priv_value){
    snmpCollectorVector v;
    snmpParser sp(DEFAULT_PRIV_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(EXPECTED_COLLECTORS_BY_CONFIG, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_AES_priv_value){
    snmpCollectorVector v;
    snmpParser sp(AES_PRIV_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(EXPECTED_COLLECTORS_BY_CONFIG, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_NOPRIV_priv_value){
    snmpCollectorVector v;
    snmpParser sp(NOPRIV_PRIV_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(EXPECTED_COLLECTORS_BY_CONFIG, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_default_auth_value){
    snmpCollectorVector v;
    snmpParser sp(DEFAULT_AUTH_VALUE_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(EXPECTED_COLLECTORS_BY_CONFIG, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_SHA1_auth_value){
    snmpCollectorVector v;
    snmpParser sp(SHA1_AUTH_VALUE_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(EXPECTED_COLLECTORS_BY_CONFIG, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_default_sec_value){
    snmpCollectorVector v;
    snmpParser sp(DEFAULT_SEC_VALUE_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(EXPECTED_COLLECTORS_BY_CONFIG, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_noauth_sec_value){
    snmpCollectorVector v;
    snmpParser sp(NOAUTH_SEC_VALUE_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(EXPECTED_COLLECTORS_BY_CONFIG, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_authpriv_sec_value){
    snmpCollectorVector v;
    snmpParser sp(AUTHPRIV_SEC_VALUE_XML_NAME);
    v = sp.getSnmpCollectorVector();
    ASSERT_EQ(EXPECTED_COLLECTORS_BY_CONFIG, v.size());
}

TEST_F(ut_snmp_parser_tests, parse_wrong_oids_v1){
    ASSERT_THROW(snmpParser sp(WRONG_OIDS_V1_XML_NAME), invalidOIDParsing);
}

TEST_F(ut_snmp_parser_tests, parse_wrong_oids_v3){
    ASSERT_THROW(snmpParser sp(WRONG_OIDS_V3_XML_NAME), invalidOIDParsing);
}
