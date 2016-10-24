/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <iostream>
#include <string>

#include "orcm/tools/octl/common.h"
#include "opal/mca/installdirs/installdirs.h"

#include "octl_tests.h"
using namespace std;


void ut_octl_output_handler_tests::SetUpTestCase()
{

    if(opal_install_dirs.opaldatadir != NULL){
        free(opal_install_dirs.opaldatadir);
    }
    opal_install_dirs.opaldatadir = strdup(".");

}

void ut_octl_output_handler_tests::TearDownTestCase()
{

}

//Test with a wrong label to make regex_label fail
TEST_F(ut_octl_output_handler_tests, orcm_octl_info_regex_failure)
{
    char *invalid_label = (char *)"]$*";
    regex_t regex_comp;
    int regex_res;

    regcomp(&regex_comp, "^$", REG_EXTENDED|REG_ICASE);

    testing::internal::CaptureStdout();

    orcm_octl_info((char *)invalid_label);

    char *output = strdup(testing::internal::GetCapturedStdout().c_str());
    regex_res = regexec(&regex_comp, output, 0, NULL, 0);
    ASSERT_EQ(0, regex_res);
}

TEST_F(ut_octl_output_handler_tests, orcm_octl_error_regex_failure)
{

    char *invalid_label = (char *)"]$*";
    regex_t regex_comp;
    int regex_res;

    regcomp(&regex_comp,  "^.*ERROR.*unable.*generate.*message.*$", REG_EXTENDED|REG_ICASE);

    testing::internal::CaptureStderr();

    orcm_octl_error(invalid_label);

    char *output = strdup(testing::internal::GetCapturedStderr().c_str());
    regex_res = regexec(&regex_comp, output, 0, NULL, 0);
    ASSERT_EQ(0, regex_res);
}

TEST_F(ut_octl_output_handler_tests, orcm_octl_usage_regex_failure)
{

    char *invalid_label = (char *)"]$*";
    regex_t regex_comp;
    int regex_res;

    regcomp(&regex_comp, "^$", REG_EXTENDED|REG_ICASE);

    testing::internal::CaptureStderr();

    orcm_octl_usage(invalid_label, 0);

    char *output = strdup(testing::internal::GetCapturedStderr().c_str());
    regex_res = regexec(&regex_comp, output, 0, NULL, 0);
    ASSERT_EQ(0, regex_res);
}

TEST_F(ut_octl_output_handler_tests, orcm_octl_usage_type_invalid_arg)
{

    regex_t regex_comp;
    int regex_res;

    regcomp(&regex_comp,  "^.*ERROR.*invalid.*argument.*$", REG_EXTENDED|REG_ICASE);

    testing::internal::CaptureStderr();

    orcm_octl_usage((char *)"sensor", 0);

    char *output = strdup(testing::internal::GetCapturedStderr().c_str());
    regex_res = regexec(&regex_comp, output, 0, NULL, 0);
    ASSERT_EQ(0, regex_res);
}

TEST_F(ut_octl_output_handler_tests, orcm_octl_usage_type_illegal_cmd)
{

    regex_t regex_comp;
    int regex_res;

    regcomp(&regex_comp,  "^.*ERROR.*illegal.*command.*$", REG_EXTENDED|REG_ICASE);

    testing::internal::CaptureStderr();

    orcm_octl_usage((char *)"sensor", 1);

    char *output = strdup(testing::internal::GetCapturedStderr().c_str());
    regex_res = regexec(&regex_comp, output, 0, NULL, 0);
    ASSERT_EQ(0, regex_res);
}

TEST_F(ut_octl_output_handler_tests, orcm_octl_usage_type_unknown)
{

    regex_t regex_comp;
    int regex_res;

    regcomp(&regex_comp,  "^.*ERROR.*unknown.*$", REG_EXTENDED|REG_ICASE);

    testing::internal::CaptureStderr();

    orcm_octl_usage((char *)"sensor", -1);

    char *output = strdup(testing::internal::GetCapturedStderr().c_str());
    regex_res = regexec(&regex_comp, output, 0, NULL, 0);
    ASSERT_EQ(0, regex_res);
}
