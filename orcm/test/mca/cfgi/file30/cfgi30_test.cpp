/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/cfgi/cfgi.h"
#include "cfgi30_mocking.h"
#include "cfgi30_test.h"
#include "opal/runtime/opal.h"
#include "orte/runtime/orte_globals.h"
#include "orcm/mca/parser/pugi/parser_pugi.h"
#include "orcm/mca/parser/pugi/pugi_impl.h"

// C++ includes
#include <iostream>

extern "C" {
  extern int read_config(opal_list_t *config);
  #include "orcm/mca/cfgi/file30/cfgi_file30_helpers.h"
};

void printing (opal_list_t *root);
void xml_lexical_tester(const char* file, int retcode);
void xml_conversion_tester(const char* file, int retcode);

using namespace std;

extern mocking_flags cfgi30_mocking;

void ut_cfgi30_tests::InitMockingFlags()
{

}

void ut_cfgi30_tests::set_file_path()
{
    char *source_dir = getenv("srcdir");
    if (NULL != source_dir){
        file_path = string(source_dir) + "/xml_inputs/";
    }else{
        file_path = "xml_inputs/";
    }
}

const char* ut_cfgi30_tests::set_name(const char *name)
{
    file_path = file_path + name;

    return file_path.c_str();
}

TEST_F(ut_cfgi30_tests, cfgi30_check_correct_port_field)
{
    char *port = strdup("54312");
    EXPECT_EQ(ORCM_SUCCESS,
             check_lex_port_field(port));
    free(port);
}

TEST_F(ut_cfgi30_tests, cfgi30_check_incorrect_wletters_port_field)
{
    char *port = strdup("543man12");
    EXPECT_EQ(ORCM_ERR_BAD_PARAM,
             check_lex_port_field(port));
    free(port);
}

TEST_F(ut_cfgi30_tests, cfgi30_check_out_of_bound_port_field)
{
    char *port = strdup("543123132432432542312");
    EXPECT_EQ(ORCM_ERR_BAD_PARAM,
             check_lex_port_field(port));
    free(port);
}

TEST_F(ut_cfgi30_tests, cfgi30_check_negative_port_field)
{
    char *port = strdup("-12345");
    EXPECT_EQ(ORCM_ERR_BAD_PARAM,
             check_lex_port_field(port));
    free(port);
}

TEST_F(ut_cfgi30_tests, check_aggregator_yes_no_field)
{
    char *yes = strdup("yes");
    EXPECT_EQ(ORCM_SUCCESS,
             check_aggregator_yes_no_field(yes));

    char *no = strdup("no");
    EXPECT_EQ(ORCM_SUCCESS,
             check_aggregator_yes_no_field(no));

    free(yes);
    free(no);
}

TEST_F(ut_cfgi30_tests, check_aggregator_yes_no_field_incorrect)
{
    char *value = strdup("it is not a yes or no");
    EXPECT_EQ(ORCM_ERR_BAD_PARAM,
	      check_aggregator_yes_no_field(value));
    free(value);
}

TEST_F(ut_cfgi30_tests, check_record_field_correct)
{
    char *value = strdup("RECORD");
    int record_count = 0;
    EXPECT_EQ(ORCM_SUCCESS,
              check_record_field(value, &record_count));
    free(value);
}

TEST_F(ut_cfgi30_tests, check_record_field_incorrect_value)
{
    char *value = strdup("RECORDA");
    int record_count = 0;
    EXPECT_EQ(ORCM_ERR_BAD_PARAM,
              check_record_field(value, &record_count));
    free(value);
}

TEST_F(ut_cfgi30_tests, check_record_field_correct_double_record)
{
    char *value = strdup("RECORD");
    int record_count = 1;
    EXPECT_EQ(ORCM_ERR_BAD_PARAM,
              check_record_field(value, &record_count));
    free(value);
}

TEST_F(ut_cfgi30_tests, check_duplicate_singleton_correct)
{
    const char* file_name = set_name("unified_xml_file.xml");

    int fileID = pugi_open(file_name);

    ASSERT_LT(0, fileID);

    opal_list_t *list = pugi_retrieve_document(fileID);

    EXPECT_EQ(ORCM_SUCCESS, check_duplicate_singleton(list));

    pugi_close(fileID);
}

TEST_F(ut_cfgi30_tests, check_duplicate_singleton_incorrect)
{
    const char* file_name = set_name("unified_duplicate_singleton.xml");

    int fileID = pugi_open(file_name);

    ASSERT_LT(0, fileID);

    opal_list_t *list = pugi_retrieve_document(fileID);

    EXPECT_EQ(ORCM_ERR_BAD_PARAM, check_duplicate_singleton(list));

    pugi_close(fileID);
}

TEST_F(ut_cfgi30_tests, check_lex_tags_and_fields_no_aggs)
{
    const char* file_name = set_name("no_aggs_xml_file.xml");

    xml_lexical_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, check_lex_tags_and_fields_valid_xml)
{
    const char* file_name = set_name("valid_xml_file.xml");

    xml_lexical_tester(file_name, ORCM_SUCCESS);
}

TEST_F(ut_cfgi30_tests, check_lex_tags_and_fields_no_role)
{
    const char* file_name = set_name("no_role.xml");

    xml_lexical_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, check_valid_test_full_example)
{
    const char* file_name = set_name("full_example.xml");

    xml_lexical_tester(file_name, ORCM_SUCCESS);
}

TEST_F(ut_cfgi30_tests, test_check_lexical_tags_and_filed_unified_xml)
{
  const char* file_name = set_name("unified_xml_file.xml");

    xml_lexical_tester(file_name, ORCM_SUCCESS);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_valid_xml)
{
    const char* file_name = set_name("valid_xml_file.xml");

    xml_conversion_tester(file_name, ORCM_SUCCESS);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_full_example)
{
    const char* file_name = set_name("full_example.xml");

    xml_conversion_tester(file_name, ORCM_SUCCESS);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_unified_xml)
{
    const char* file_name = set_name("unified_xml_file.xml");

    xml_conversion_tester(file_name, ORCM_SUCCESS);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_multiple_clusters)
{
    const char* file_name = set_name("configuration_multiple_clusters.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_no_cluster_at_top)
{
    const char* file_name = set_name("configuration_no_cluster_at_top.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_controller_child)
{
    const char* file_name = set_name("controller_has_child.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_double_scheduler)
{
    const char* file_name = set_name("double_scheduler.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_empty_row)
{
    const char* file_name = set_name("empty_row.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_no_name_junction)
{
    const char* file_name = set_name("junction_has_no_name.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_no_type_junction)
{
    const char* file_name = set_name("junction_has_no_type.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_main_junction_no_type_no_name)
{
    const char* file_name = set_name("main_junction_no_type_no_name.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_no_config_at_top)
{
    const char* file_name = set_name("no_config_at_top.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_node_in_row)
{
    const char* file_name = set_name("node_in_row.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_node_in_node)
{
    const char* file_name = set_name("node_inside_a_node.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_node_with_controller)
{
    const char* file_name = set_name("node_with_controller.xml");

    xml_conversion_tester(file_name, ORCM_SUCCESS);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_rack_in_cluster)
{
    const char* file_name = set_name("rack_in_cluster.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_row_in_rack)
{
    const char* file_name = set_name("row_in_a_rack.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

TEST_F(ut_cfgi30_tests, test_convert_to_orcm_cfgi_xml_parser_t_list_scheduler_has_child)
{
    const char* file_name = set_name("scheduler_has_child.xml");

    xml_conversion_tester(file_name, ORCM_ERR_BAD_PARAM);
}

/* utils */
void xml_lexical_tester(const char* file, int retcode)
{
    int fileID = pugi_open(file);

    ASSERT_LT(0, fileID);

    opal_list_t *list = pugi_retrieve_document(fileID);
    pugi_close(fileID);
    EXPECT_EQ(retcode, check_lex_tags_and_field(list));
}

void xml_conversion_tester(const char* file, int retcode)
{
    int fileID = pugi_open(file);

    ASSERT_LT(0, fileID);
    opal_list_t *source = pugi_retrieve_document(fileID);

    opal_list_t target;

    OBJ_CONSTRUCT(&target, opal_list_t);
    pugi_close(fileID);
    EXPECT_EQ(retcode, convert_to_orcm_cfgi_xml_parser_t_list(source,&target));
}
